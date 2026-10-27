// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessSearch.h"
#include "HAL/PlatformTime.h"

namespace
{
    constexpr int32 MateScore = 100000;
    constexpr int32 Infinity = 200000;
    constexpr int32 MaxPly = 96;
    constexpr int32 TableSize = 1 << 16;

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

// 初始化一次搜索，内存由线程安全共享指针管理。
FChessSearch::FChessSearch(const FChessPosition& InPosition, EChessColor InColor, int32 InDepth, double InSeconds)
    : Board(InPosition), Color(InColor), MaxDepth(FMath::Clamp(InDepth, 1, 12)), TimeBudget(FMath::Max(0.01, InSeconds)), InitialPositionKey(InPosition.Hash(InColor))
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
void FChessSearch::OrderMoves(TArray<FChessMove2P>& Moves, const FChessMove2P& Preferred, EChessColor Side)
{
    for (FChessMove2P& Move : Moves)
    {
        const EChessType Victim = Board.Get(Move.to).Type;
        Move.score = SameMove(Move, Preferred) ? 10000000 : Victim != EChessType::EMPTY ?
            1000000 + FChessPosition::PieceValue(Victim) * 16 - FChessPosition::PieceValue(Board.Get(Move.from).Type) :
            History[static_cast<uint8>(Side)][Move.from.X * 9 + Move.from.Y][Move.to.X * 9 + Move.to.Y];
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
    OrderMoves(Moves, {}, Color);
    Result.Move = Moves[0]; // 时间不足时仍保留一个合法走法
    Table.SetNum(TableSize);
    Path.Reserve(MaxPly);
    for (int32 Depth = 1; Depth <= MaxDepth; ++Depth)
    {
        if (FPlatformTime::Seconds() >= Deadline || ShouldStop()) break;
        RootMove = Result.Move;
        const int32 Score = Negamax(Depth, -Infinity, Infinity, Color, 0);
        if (bAborted) break;
        Result.Move = RootMove;
        Result.Move.score = Score;
        Result.CompletedDepth = Depth;
        if (FMath::Abs(Score) >= MateScore - MaxPly) break;
    }
    Result.Nodes = Nodes;
    Result.bCancelled = bCancelled.load(std::memory_order_relaxed);
    if (Result.bCancelled) Result.Move = FChessMove2P();
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
    if (Depth <= 0) return Quiescence(Alpha, Beta, Side, Ply, 8);
    TArray<FChessMove2P> Moves = Board.GenerateLegalMoves(Side);
    if (Moves.IsEmpty()) return -MateScore + Ply; // 中国象棋困毙也判负
    if (Ply >= MaxPly - 1) return Board.Evaluate(Side);

    const int32 OriginalAlpha = Alpha;
    const FTableEntry Cached = Table[Key & (TableSize - 1)];
    const bool bHit = Cached.Depth >= 0 && Cached.Key == Key;
    // 重复判定依赖祖先局面，只有祖先集合一致时才复用搜索分数。
    if (bHit && Cached.Context == PathContext && Cached.Depth >= Depth)
    {
        const int32 Score = LoadScore(Cached.Score, Ply);
        if (Cached.Bound == 0 || (Cached.Bound == 1 && Score <= Alpha) || (Cached.Bound == 2 && Score >= Beta))
        {
            if (Ply == 0) RootMove = Cached.Move;
            return Score;
        }
    }
    OrderMoves(Moves, bHit ? Cached.Move : FChessMove2P(), Side);
    Path.Add(Key);
    PathContext ^= Key;
    int32 BestScore = -Infinity;
    FChessMove2P BestMove;
    for (const FChessMove2P& Move : Moves)
    {
        const FChessPiece2P Captured = Board.MakeMove(Move);
        const int32 Score = -Negamax(Depth - 1, -Beta, -Alpha, FChessPosition::Opponent(Side), Ply + 1);
        Board.UndoMove(Move, Captured);
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
    const int32 StandPat = Board.Evaluate(Side);
    if (Ply >= MaxPly - 1 || (!bInCheck && Remaining <= 0)) return StandPat;
    if (!bInCheck)
    {
        if (StandPat >= Beta) return StandPat;
        Alpha = FMath::Max(Alpha, StandPat);
    }
    OrderMoves(Moves, {}, Side);
    Path.Add(Key);
    PathContext ^= Key;
    for (const FChessMove2P& Move : Moves)
    {
        if (!bInCheck && Board.Get(Move.to).Type == EChessType::EMPTY) continue;
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
