// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessSearch.h"
#include "ChessOpeningBook.h"
#include "HAL/PlatformTime.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
    constexpr int32 MateScore = 100000;
    constexpr int32 Infinity = 200000;
    constexpr int32 MaxPly = 96;
    constexpr int32 TableSize = 1 << 16;

    struct FOpeningAnalysis
    {
        FChessSearchResult Result;
        FChessSearchStyle Style;
        int32 MaxDepth;
        double Budget;
    };
    FCriticalSection OpeningCacheMutex;
    TMap<uint64, FOpeningAnalysis> OpeningCache;

    // 比较走法坐标，排序分数不参与匹配。
    bool SameMove(const FChessMove2P& A, const FChessMove2P& B)
    {
        return A.IsValid() && B.IsValid() && A.from == B.from && A.to == B.to;
    }

    // 写入置换表时消除杀棋分数对当前搜索层数的依赖。
    int32 StoreScore(int32 Score, int32 Ply)
    {
        return Score > MateScore - MaxPly ? Score + Ply : Score < -MateScore + MaxPly ? Score - Ply : Score;
    }

    // 读取置换表时还原当前层数下的杀棋距离。
    int32 LoadScore(int32 Score, int32 Ply)
    {
        return Score > MateScore - MaxPly ? Score - Ply : Score < -MateScore + MaxPly ? Score + Ply : Score;
    }
}

FChessAISettings FChessAISettings::ForDifficulty(uint8 Difficulty)
{
    static constexpr int32 Depths[] = {1, 3, 7, 12, 12};
    static constexpr double Seconds[] = {0.08, 0.35, 2.5, 7.0, 12.0};
    static constexpr int32 CaptureDepths[] = {0, 2, 6, 10, 12};
    static constexpr int32 Inaccuracies[] = {180, 25, 0, 0, 0};
    const int32 Index = FMath::Min<int32>(Difficulty, 4);
    FChessAISettings Result;
    Result.Depth = Depths[Index];
    Result.Seconds = Seconds[Index];
    Result.Style.CaptureDepth = CaptureDepths[Index];
    Result.Style.Inaccuracy = Inaccuracies[Index];
    Result.Style.bUseOpeningBook = Index >= 2;
    Result.Style.bAdvancedSearch = Index >= 2;
    Result.Style.bSelectiveSearch = Index >= 4;
    Result.Style.bTacticalEvaluation = Index >= 3;
    return Result;
}

FChessAISettings FChessAISettings::ForLearning(uint8 Difficulty, int32 Wins, int32 Observations)
{
    FChessAISettings Result = ForDifficulty(Difficulty);
    const int32 Growth = FMath::Clamp(Wins, 0, 12);
    // 第一胜就增加完整搜索层，避免简单档第二局仍只看一手，却改变静态叶子评估。
    // 预算先连续增长，再逐层提升；样本数只增加时间，不在一盘中改变搜索语义。
    Result.Depth = FMath::Min(12, Result.Depth + Growth);
    Result.Seconds = FMath::Min(15.0, Result.Seconds * (1.0 + .6 * Growth)
        + FMath::Min(1.5, FMath::Max(0, Observations) * .008));
    Result.Style.CaptureDepth = FMath::Min(12, Result.Style.CaptureDepth + Growth * 2);
    Result.Style.Inaccuracy = FMath::Max(0, Result.Style.Inaccuracy - Growth * 25);
    // 高级算法沿用选定档位，避免第三局突然切换多项算法造成难度跳变。
    return Result;
}

// 初始化一次搜索，内存由线程安全共享指针管理。
FChessSearch::FChessSearch(const FChessPosition& InPosition, EChessColor InColor, int32 InDepth, double InSeconds,
    FChessLearning InLearning, FChessCheckHistory InCheckHistory, FChessSearchStyle InStyle)
    : Board(InPosition), Learning(MoveTemp(InLearning)), Style(InStyle), CheckHistory(MoveTemp(InCheckHistory)), Color(InColor),
      MaxDepth(FMath::Clamp(InDepth, 1, 12)), TimeBudget(FMath::Max(0.01, InSeconds)), InitialPositionKey(InPosition.Hash(InColor))
{
}

// 发布原子取消信号，不等待后台线程结束。
void FChessSearch::Cancel()
{
    bCancelled.store(true, std::memory_order_relaxed);
}

// 定期检查时间，每个节点检查取消请求。
bool FChessSearch::ShouldStop()
{
    if (bCancelled.load(std::memory_order_relaxed) || ((Nodes & 63) == 0 && FPlatformTime::Seconds() >= Deadline))
        bAborted = true;
    return bAborted;
}

// 检测搜索路径内的重复，避免在循环中无限延伸。
bool FChessSearch::IsRepeated(uint64 Key) const
{
    return Path.Contains(Key);
}

// 预计算排序分数，避免比较器反复读取棋盘。
void FChessSearch::OrderMoves(TArray<FChessMove2P>& Moves, const FChessMove2P& Preferred, EChessColor Side, int32 Ply)
{
    for (FChessMove2P& Move : Moves)
    {
        const EChessType Victim = Board.Get(Move.to).Type;
        Move.score = SameMove(Move, Preferred) ? 10000000 : Victim != EChessType::EMPTY ?
            1000000 + FChessPosition::PieceValue(Victim) * 16 - FChessPosition::PieceValue(Board.Get(Move.from).Type) :
            (Style.bAdvancedSearch && Ply < MaxPly && SameMove(Move, Killers[Ply][0])) ? 900000 :
            (Style.bAdvancedSearch && Ply < MaxPly && SameMove(Move, Killers[Ply][1])) ? 800000 :
            History[static_cast<uint8>(Side)][Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y];
        if (Ply == 0) Move.score += RootBias[Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y];
        if (Style.bSelectiveSearch && Victim != EChessType::EMPTY && !SameMove(Move, Preferred))
        {
            const int32 Exchange = Board.StaticExchange(Move);
            // 亏损交换排在优质静着之后，但不删除，仍允许战术弃子。
            Move.score += Exchange < 0 ? -1200000 : FMath::Min(20000, Exchange * 8);
        }
        if (Side == EChessColor::REDCHESS) Move.score += Learning.MovePriority(Board, Move);
        // 反例只改变访问顺序，不直接扣除棋子的真实价值，避免学会“害怕正确走法”。
        if (Side == EChessColor::BLACKCHESS) Move.score -= Learning.AvoidancePenalty(Board, Move) * 8;
    }
    // 高价值吃子和已验证的主变化优先搜索。
    Moves.Sort([](const FChessMove2P& A, const FChessMove2P& B) { return A.score > B.score; });
}

// 从浅到深搜索，任何时候都只提交完整层的结果。
FChessSearchResult FChessSearch::FindBestMove()
{
    FChessSearchResult Result;
    Deadline = FPlatformTime::Seconds() + TimeBudget;
    if (bCancelled.load(std::memory_order_relaxed))
    {
        Result.bCancelled = true;
        return Result;
    }
    TArray<FChessMove2P> Moves = Board.GenerateLegalMoves(Color);
    Result.bNoLegalMoves = Moves.IsEmpty();
    if (Moves.IsEmpty()) return Result;
    RestrictedRootMoves.Reset();
    Moves.RemoveAll([&](const FChessMove2P& Move)
    {
        if (!CheckHistory.WouldRepeat(Board, Move)) return false;
        RestrictedRootMoves.Add(Move);
        return true;
    });
    if (Moves.IsEmpty())
    {
        // 唯一选择只剩第三次长将时AI认负，不能停住棋局或提交非法走法。
        Result.bRepetitionLimitReached = true;
        return Result;
    }
    FRandomStream Random(Style.RandomSeed);
    const TArray<FChessMove2P> BookMoves = Style.bUseOpeningBook ? FChessOpeningBook::FindMoves(Board, Color) : TArray<FChessMove2P>();
    // 只复用无随机误差、无学习和长将限制的同一开局。预算和棋力策略必须匹配，
    // 困难/大师不能读到棋圣的分析，棋圣也不会被低档缓存覆盖。
    const bool bCacheOpening = !BookMoves.IsEmpty() && Style.Inaccuracy == 0 && RestrictedRootMoves.IsEmpty() &&
        Learning.Observations == 0 && Learning.PositionMoves.IsEmpty() && Learning.AICaution.IsEmpty() && Learning.HarmfulReplies.IsEmpty();
    if (bCacheOpening)
    {
        FScopeLock Lock(&OpeningCacheMutex);
        if (const auto* Cached = OpeningCache.Find(InitialPositionKey); Cached && Cached->MaxDepth == MaxDepth &&
            Cached->Budget >= TimeBudget && Cached->Style.CaptureDepth == Style.CaptureDepth &&
            Cached->Style.bAdvancedSearch == Style.bAdvancedSearch && Cached->Style.bSelectiveSearch == Style.bSelectiveSearch &&
            Cached->Style.bTacticalEvaluation == Style.bTacticalEvaluation &&
            Moves.ContainsByPredicate([&](const FChessMove2P& Move) { return SameMove(Move, Cached->Result.Move); }))
        {
            Result = Cached->Result;
            Result.Nodes = 0;
            Result.bFromOpeningCache = true;
            Result.bCancelled = bCancelled.load(std::memory_order_relaxed);
            if (Result.bCancelled) Result.Move = {};
            return Result;
        }
    }
    for (const FChessMove2P& Move : Moves)
    {
        int32& Bias = RootBias[Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y];
        Bias = Style.Inaccuracy > 0 ? Random.RandRange(-Style.Inaccuracy, Style.Inaccuracy) : 0;
        // 棋谱只是有限的位置偏好，真实搜索可以推翻它，绝不覆盖将杀与应将。
        if (BookMoves.ContainsByPredicate([&](const FChessMove2P& Book) { return SameMove(Move, Book); })) Bias += 35;
    }
    OrderMoves(Moves, {}, Color, 0);
    Result.Move = Moves[0]; // 时间不足时仍保留一个合法走法
    Table.SetNum(TableSize);
    EvaluationTable.SetNum(TableSize);
    Path.Reserve(MaxPly);
    for (int32 Depth = 1; Depth <= MaxDepth; ++Depth)
    {
        if (FPlatformTime::Seconds() >= Deadline || ShouldStop()) break;
        RootMove = Result.Move;
        int32 Score;
        if (Style.bSelectiveSearch && Depth >= 4)
        {
            // 渴望窗口只缩小首次搜索区间；失败必须完整重搜，不能发布窗口外的界值。
            const int32 Previous = Result.Move.score;
            Score = Negamax(Depth, Previous - 70, Previous + 70, Color, 0);
            if (!bAborted && (Score <= Previous - 70 || Score >= Previous + 70))
                Score = Negamax(Depth, -Infinity, Infinity, Color, 0);
        }
        else Score = Negamax(Depth, -Infinity, Infinity, Color, 0);
        if (bAborted) break;
        Result.Move = RootMove;
        Result.Move.score = Score;
        Result.CompletedDepth = Depth;
        if (FMath::Abs(Score) >= MateScore - MaxPly) break;
    }
    Result.Nodes = Nodes;
    Result.bCancelled = bCancelled.load(std::memory_order_relaxed);
    if (Result.bCancelled) Result.Move = FChessMove2P();
    if (bCacheOpening && !Result.bCancelled && Result.CompletedDepth > 0)
    {
        FScopeLock Lock(&OpeningCacheMutex);
        if (OpeningCache.Num() >= 256) OpeningCache.Reset();
        OpeningCache.Add(InitialPositionKey, {Result, Style, MaxDepth, TimeBudget});
    }
    return Result;
}

// 搜索合法走法，优先更快获胜或更晚被将死的变化。
int32 FChessSearch::Negamax(int32 Depth, int32 Alpha, int32 Beta, EChessColor Side, int32 Ply)
{
    ++Nodes;
    if (ShouldStop()) return 0;
    if (!FChessPosition::IsInside(Board.FindKing(Side))) return -MateScore + Ply;
    if (!FChessPosition::IsInside(Board.FindKing(FChessPosition::Opponent(Side)))) return MateScore - Ply;
    const uint64 Key = Board.Hash(Side);
    if (IsRepeated(Key)) return 0;
    if (Depth <= 0) return Quiescence(Alpha, Beta, Side, Ply, Style.CaptureDepth);
    // 强档延伸应将变化，限制总层数，避免连续将军无限延伸。
    const bool bInCheck = Board.IsInCheck(Side);
    if (Style.bAdvancedSearch && bInCheck && Ply < MaxDepth * 2) ++Depth;
    const int32 OriginalAlpha = Alpha;
    const FTableEntry Cached = Table[Key & (TableSize - 1)];
    const bool bHit = Cached.Depth >= 0 && Cached.Key == Key;
    // 重复判定依赖祖先局面，只有祖先集合一致时才复用搜索分数。
    if (Ply < MaxPly - 1 && bHit && Cached.Context == PathContext && Cached.Depth >= Depth)
    {
        const int32 Score = LoadScore(Cached.Score, Ply);
        if (Cached.Bound == 0 || (Cached.Bound == 1 && Score <= Alpha) || (Cached.Bound == 2 && Score >= Beta))
        {
            if (Ply == 0) RootMove = Cached.Move;
            return Score;
        }
    }
    TArray<FChessMove2P> Moves = Board.GenerateLegalMoves(Side);
    if (Ply == 0)
        Moves.RemoveAll([&](const FChessMove2P& Move)
            { return RestrictedRootMoves.ContainsByPredicate([&](const FChessMove2P& Restricted) { return SameMove(Move, Restricted); }); });
    if (Moves.IsEmpty()) return -MateScore + Ply; // 中国象棋困毙也判负
    if (Ply >= MaxPly - 1) return Evaluate(Side);
    OrderMoves(Moves, bHit ? Cached.Move : FChessMove2P(), Side, Ply);
    Path.Add(Key);
    PathContext ^= Key;
    int32 BestScore = -Infinity;
    FChessMove2P BestMove;
    int32 MoveIndex = 0;
    for (const FChessMove2P& Move : Moves)
    {
        // 只对根节点应用选定档位的随机/棋谱偏好，并同步平移子节点窗口保持剪枝一致。
        // 学习反例仅在OrderMoves排序，不能把历史惩罚当成真实子力损失参与评分。
        const int32 LearnedPenalty = -(Ply == 0 ? RootBias[Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y] : 0);
        const FChessPiece2P Captured = Board.MakeMove(Move);
        int32 Score;
        if (Style.bAdvancedSearch && BestMove.IsValid())
        {
            // 只缩减非主变化的后置安静着。应将、将军、吃子、杀手着均保留完整深度。
            const bool Reduce = Style.bSelectiveSearch && Ply > 0 && Beta - Alpha == 1 && Depth >= 3 && MoveIndex >= 4 &&
                !bInCheck && Captured.Type == EChessType::EMPTY && !Board.IsInCheck(FChessPosition::Opponent(Side)) &&
                !SameMove(Move, Killers[Ply][0]) && !SameMove(Move, Killers[Ply][1]);
            const int32 Reduction = Reduce ? (Depth >= 6 && MoveIndex >= 12 ? 2 : 1) : 0;
            // 主变化搜索：其余分支先用零窗口验证，需要时再完整重搜。
            Score = -Negamax(Depth - 1 - Reduction, -Alpha - 1 - LearnedPenalty, -Alpha - LearnedPenalty,
                FChessPosition::Opponent(Side), Ply + 1) - LearnedPenalty;
            if (!bAborted && Reduction > 0 && Score > Alpha)
                Score = -Negamax(Depth - 1, -Alpha - 1 - LearnedPenalty, -Alpha - LearnedPenalty,
                    FChessPosition::Opponent(Side), Ply + 1) - LearnedPenalty;
            if (!bAborted && Score > Alpha && Score < Beta)
                Score = -Negamax(Depth - 1, -Beta - LearnedPenalty, -Alpha - LearnedPenalty,
                    FChessPosition::Opponent(Side), Ply + 1) - LearnedPenalty;
        }
        else Score = -Negamax(Depth - 1, -Beta - LearnedPenalty, -Alpha - LearnedPenalty,
            FChessPosition::Opponent(Side), Ply + 1) - LearnedPenalty;
        Board.UndoMove(Move, Captured);
        ++MoveIndex;
        if (bAborted) break;
        if (Score > BestScore)
        {
            BestScore = Score;
            BestMove = Move;
            if (Ply == 0) RootMove = Move;
        }
        Alpha = FMath::Max(Alpha, Score);
        if (Alpha >= Beta)
        {
            if (Captured.Type == EChessType::EMPTY)
            {
                int32& Value = History[static_cast<uint8>(Side)][Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y];
                Value = FMath::Min(100000, Value + Depth * Depth);
                if (!SameMove(Move, Killers[Ply][0]))
                {
                    Killers[Ply][1] = Killers[Ply][0];
                    Killers[Ply][0] = Move;
                }
            }
            break;
        }
    }
    PathContext ^= Key;
    Path.Pop(EAllowShrinking::No);
    if (bAborted) return 0;
    FTableEntry& Entry = Table[Key & (TableSize - 1)];
    Entry.Key = Key;
    Entry.Context = PathContext;
    Entry.Move = BestMove;
    Entry.Depth = static_cast<int16>(Depth);
    Entry.Score = StoreScore(BestScore, Ply);
    Entry.Bound = BestScore <= OriginalAlpha ? 1 : BestScore >= Beta ? 2 : 0;
    return BestScore;
}

// 静态搜索消除吃子交换的水平线效应，被将军时禁止直接静态停步。
int32 FChessSearch::Quiescence(int32 Alpha, int32 Beta, EChessColor Side, int32 Ply, int32 Remaining)
{
    ++Nodes;
    if (ShouldStop()) return 0;
    if (!FChessPosition::IsInside(Board.FindKing(Side))) return -MateScore + Ply;
    if (!FChessPosition::IsInside(Board.FindKing(FChessPosition::Opponent(Side)))) return MateScore - Ply;
    const uint64 Key = Board.Hash(Side);
    if (IsRepeated(Key)) return 0;
    TArray<FChessMove2P> Moves = Board.GenerateLegalMoves(Side);
    if (Moves.IsEmpty()) return -MateScore + Ply;
    const bool bInCheck = Board.IsInCheck(Side);
    if (Ply >= MaxPly - 1 || (!bInCheck && Remaining <= 0)) return Evaluate(Side);
    const int32 StandPat = bInCheck ? 0 : Evaluate(Side);
    if (!bInCheck)
    {
        if (StandPat >= Beta) return StandPat;
        Alpha = FMath::Max(Alpha, StandPat);
    }
    OrderMoves(Moves, {}, Side, Ply);
    Path.Add(Key);
    PathContext ^= Key;
    for (const FChessMove2P& Move : Moves)
    {
        const auto Victim = Board.Get(Move.to);
        bool GivesCheck = false;
        const bool bQuietChecks = Remaining >= Style.CaptureDepth - 1;
        if (!bInCheck && Victim.Type == EChessType::EMPTY && !(Style.bSelectiveSearch && bQuietChecks)) continue;
        const bool bDeltaCandidate = Victim.Type != EChessType::EMPTY && Victim.Type != EChessType::JIANG &&
            StandPat + FChessPosition::PieceValue(Victim.Type) + 180 < Alpha;
        if (!bInCheck && Style.bSelectiveSearch && (Victim.Type == EChessType::EMPTY || bDeltaCandidate))
        {
            const auto Taken = Board.MakeMove(Move);
            GivesCheck = Board.IsInCheck(FChessPosition::Opponent(Side));
            Board.UndoMove(Move, Taken);
        }
        // 静态搜索前两层也检查安静将军，覆盖弃子后续杀；总层数和时间仍有限制。
        if (!bInCheck && Victim.Type == EChessType::EMPTY && !(GivesCheck && Remaining >= Style.CaptureDepth - 1)) continue;
        if (!bInCheck && !GivesCheck && Style.bSelectiveSearch && Victim.Type != EChessType::EMPTY &&
            Victim.Type != EChessType::JIANG && StandPat + FChessPosition::PieceValue(Victim.Type) + 180 < Alpha &&
            Board.StaticExchange(Move) < 0) continue;
        const FChessPiece2P Captured = Board.MakeMove(Move);
        const int32 Score = -Quiescence(-Beta, -Alpha, FChessPosition::Opponent(Side), Ply + 1, Remaining - 1);
        Board.UndoMove(Move, Captured);
        if (bAborted) break;
        Alpha = FMath::Max(Alpha, Score);
        if (Alpha >= Beta) break;
    }
    PathContext ^= Key;
    Path.Pop(EAllowShrinking::No);
    return bAborted ? 0 : Alpha;
}

// 学习影响真实搜索估值，但限制在半个兵以内，保留战术与将杀优先级。
int32 FChessSearch::Evaluate(EChessColor Side)
{
    const uint64 Key = Board.Hash(Side);
    FEvaluationEntry& Cached = EvaluationTable[Key & (TableSize - 1)];
    if (Cached.bValid && Cached.Key == Key) return Cached.Score;
    const int32 LearnedStyle = Learning.StyleEvaluation(Board);
    const int32 Score = Board.Evaluate(Side) + (Style.bTacticalEvaluation ? Board.EvaluateActivity(Side) : 0) +
        (Side == EChessColor::REDCHESS ? LearnedStyle : -LearnedStyle);
    Cached = {Key, Score, true};
    return Score;
}
