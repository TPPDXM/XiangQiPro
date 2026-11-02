// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessPosition.h"

FChessPosition FChessPosition::Standard()
{
    FChessPosition Board;
    constexpr EChessType Back[] = {EChessType::JV, EChessType::MA, EChessType::XIANG, EChessType::SHI,
        EChessType::JIANG, EChessType::SHI, EChessType::XIANG, EChessType::MA, EChessType::JV};
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const auto Color = static_cast<EChessColor>(Side);
        for (int32 File = 0; File < 9; ++File) Board.Set({Side ? 9 : 0, File}, {Back[File], Color});
        for (int32 File : {1, 7}) Board.Set({Side ? 7 : 2, File}, {EChessType::PAO, Color});
        for (int32 File : {0, 2, 4, 6, 8}) Board.Set({Side ? 6 : 3, File}, {EChessType::BING, Color});
    }
    return Board;
}

// 判断棋盘边界。
bool FChessPosition::IsInside(Position Pos)
{
    return Pos.X >= 0 && Pos.X < 10 && Pos.Y >= 0 && Pos.Y < 9;
}

// 获取对方阵营。
EChessColor FChessPosition::Opponent(EChessColor Color)
{
    return Color == EChessColor::REDCHESS ? EChessColor::BLACKCHESS : EChessColor::REDCHESS;
}

// 判断九宫范围。
bool FChessPosition::IsInPalace(Position Pos, EChessColor Color)
{
    return IsInside(Pos) && Pos.Y >= 3 && Pos.Y <= 5 &&
        (Color == EChessColor::REDCHESS ? Pos.X <= 2 : Pos.X >= 7);
}

// 安全读取格子数据。
FChessPiece2P FChessPosition::Get(Position Pos) const
{
    return IsInside(Pos) ? Squares[Pos.X * 9 + Pos.Y] : FChessPiece2P{};
}

// 安全设置格子数据。
void FChessPosition::Set(Position Pos, FChessPiece2P Piece)
{
    if (IsInside(Pos)) Squares[Pos.X * 9 + Pos.Y] = Piece;
}

// 查找将帅位置。
Position FChessPosition::FindKing(EChessColor Color) const
{
    for (int32 Index = 0; Index < 90; ++Index)
    {
        if (Squares[Index].Type == EChessType::JIANG && Squares[Index].Color == Color)
            return Position(Index / 9, Index % 9);
    }
    return Position(-1, -1);
}

// 统计直线上两个位置之间的阻挡数。
int32 FChessPosition::CountBetween(Position From, Position To) const
{
    if (!IsInside(From) || !IsInside(To) || From == To || (From.X != To.X && From.Y != To.Y)) return -1;
    const Position Step(FMath::Sign(To.X - From.X), FMath::Sign(To.Y - From.Y));
    int32 Count = 0;
    for (Position Pos = From + Step; Pos != To; Pos += Step)
        if (Get(Pos).Type != EChessType::EMPTY) ++Count;
    return Count;
}

// 检查棋子几何走法，统一处理蹩马腿、塞象眼和炮架。
bool FChessPosition::CanReach(Position From, Position To) const
{
    if (!IsInside(From) || !IsInside(To) || From == To) return false;
    const FChessPiece2P Piece = Get(From);
    const FChessPiece2P Target = Get(To);
    if (Piece.Type == EChessType::EMPTY || (Target.Type != EChessType::EMPTY && Target.Color == Piece.Color)) return false;
    const int32 DX = To.X - From.X;
    const int32 DY = To.Y - From.Y;
    const int32 AX = FMath::Abs(DX);
    const int32 AY = FMath::Abs(DY);
    switch (Piece.Type)
    {
    case EChessType::JIANG:
        return (AX + AY == 1 && IsInPalace(To, Piece.Color)) ||
            (bFlyingKingCapture && Target.Type == EChessType::JIANG && DY == 0 && CountBetween(From, To) == 0);
    case EChessType::SHI:
        return AX == 1 && AY == 1 && (IsInPalace(To, Piece.Color) || (bVariantRules && !IsInPalace(From, Piece.Color)));
    case EChessType::XIANG:
    {
        const bool bHome = Piece.Color == EChessColor::REDCHESS ? To.X <= 4 : To.X >= 5;
        const bool bAway = Piece.Color == EChessColor::REDCHESS ? From.X >= 5 : From.X <= 4;
        return AX == 2 && AY == 2 && (bHome || (bVariantRules && bAway)) && Get((From + To) / 2).Type == EChessType::EMPTY;
    }
    case EChessType::MA:
        return ((AX == 2 && AY == 1) || (AX == 1 && AY == 2)) &&
            Get(From + Position(AX == 2 ? DX / 2 : 0, AY == 2 ? DY / 2 : 0)).Type == EChessType::EMPTY;
    case EChessType::JV:
        return CountBetween(From, To) == 0;
    case EChessType::PAO:
        return CountBetween(From, To) == (Target.Type == EChessType::EMPTY ? 0 : 1);
    case EChessType::BING:
        return (DY == 0 && DX == (Piece.Color == EChessColor::REDCHESS ? 1 : -1)) ||
            (DX == 0 && AY == 1 && (Piece.Color == EChessColor::REDCHESS ? From.X >= 5 : From.X <= 4));
    default:
        return false;
    }
}

// 使用攻击规则检查将军，不递归生成合法走法。
bool FChessPosition::IsInCheck(EChessColor Color) const
{
    const Position King = FindKing(Color);
    if (!IsInside(King)) return bRequireKing;
    for (int32 Index = 0; Index < 90; ++Index)
    {
        if (Squares[Index].Type != EChessType::EMPTY && Squares[Index].Color == Opponent(Color) &&
            CanReach(Position(Index / 9, Index % 9), King)) return true;
    }
    return false;
}

// 按棋子类型枚举有限的目标位置，避免逐格尝试整张棋盘。
void FChessPosition::GeneratePieceMoves(Position From, TArray<FChessMove2P>& Moves) const
{
    const FChessPiece2P Piece = Get(From);
    // 验证目标后追加走法。
    const auto Add = [this, From, &Moves](Position To)
    {
        if (CanReach(From, To)) Moves.Emplace(From, To);
    };
    if (Piece.Type == EChessType::JV || Piece.Type == EChessType::PAO)
    {
        for (int32 X = 0; X < 10; ++X) Add(Position(X, From.Y));
        for (int32 Y = 0; Y < 9; ++Y) Add(Position(From.X, Y));
    }
    else if (Piece.Type == EChessType::MA)
    {
        static const Position Steps[] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
        for (Position Step : Steps) Add(From + Step);
    }
    else if (Piece.Type == EChessType::SHI || Piece.Type == EChessType::XIANG)
    {
        const int32 Step = Piece.Type == EChessType::SHI ? 1 : 2;
        Add(From + Position(Step, Step)); Add(From + Position(Step, -Step));
        Add(From + Position(-Step, Step)); Add(From + Position(-Step, -Step));
    }
    else if (Piece.Type == EChessType::JIANG || Piece.Type == EChessType::BING)
    {
        Add(From + Position(1, 0)); Add(From + Position(-1, 0));
        Add(From + Position(0, 1)); Add(From + Position(0, -1));
        // 烽火弈允许将帅照面，且不允许飞将吃子：不扫描对方将位，也不生成虚假的长程威胁。
        // 经典模式保留原照面吃将规则，避免把中路棋子错误地当作必须保留的屏障。
        if (Piece.Type == EChessType::JIANG && bFlyingKingCapture)
        {
            const Position King = FindKing(Opponent(Piece.Color));
            if (FMath::Abs(King.X - From.X) + FMath::Abs(King.Y - From.Y) > 1) Add(King);
        }
    }
}

// 在所有阶段过滤自将走法，无棋可走时保持空结果。
TArray<FChessMove2P> FChessPosition::GenerateLegalMoves(EChessColor Color)
{
    TArray<FChessMove2P> Candidates;
    TArray<FChessMove2P> Legal;
    if (Color != EChessColor::REDCHESS && Color != EChessColor::BLACKCHESS) return Legal;
    if (bRequireKing && !IsInside(FindKing(Color))) return Legal;
    Candidates.Reserve(64);
    for (int32 Index = 0; Index < 90; ++Index)
        if (Squares[Index].Type != EChessType::EMPTY && Squares[Index].Color == Color)
            GeneratePieceMoves(Position(Index / 9, Index % 9), Candidates);
    Legal.Reserve(Candidates.Num());
    for (const FChessMove2P& Move : Candidates)
    {
        const FChessPiece2P Captured = MakeMove(Move);
        if (!IsInCheck(Color)) Legal.Add(Move);
        UndoMove(Move, Captured);
    }
    return Legal;
}

// 试走只移动两个格子，不修改真实棋子。
FChessPiece2P FChessPosition::MakeMove(const FChessMove2P& Move)
{
    const FChessPiece2P Captured = Get(Move.to);
    Set(Move.to, Get(Move.from));
    Set(Move.from, {});
    return Captured;
}

// 恢复试走前的数据。
void FChessPosition::UndoMove(const FChessMove2P& Move, FChessPiece2P Captured)
{
    Set(Move.from, Get(Move.to));
    Set(Move.to, Captured);
}

// 获取子力价值，非法枚举按空格处理。
int32 FChessPosition::PieceValue(EChessType Type)
{
    static constexpr int32 Values[] = {0, 10000, 120, 120, 300, 600, 320, 70};
    const uint8 Index = static_cast<uint8>(Type);
    return Index < UE_ARRAY_COUNT(Values) ? Values[Index] : 0;
}

// 使用快照中的实时位置估值，过河兵、中心马和开放车获得奖励。
int32 FChessPosition::Evaluate(EChessColor Color) const
{
    int32 Score = 0;
    int32 MajorCount = 0;
    for (const FChessPiece2P& Piece : Squares)
        if (Piece.Type == EChessType::JV || Piece.Type == EChessType::MA || Piece.Type == EChessType::PAO) ++MajorCount;
    for (int32 Index = 0; Index < 90; ++Index)
    {
        const FChessPiece2P Piece = Squares[Index];
        if (Piece.Type == EChessType::EMPTY) continue;
        const int32 Rank = Piece.Color == EChessColor::REDCHESS ? Index / 9 : 9 - Index / 9;
        const int32 Center = 4 - FMath::Abs(Index % 9 - 4);
        int32 Value = PieceValue(Piece.Type);
        switch (Piece.Type)
        {
        case EChessType::BING: Value += Rank * 4 + (Rank >= 5 ? 55 + Center * 8 : 0) + (MajorCount <= 4 ? Rank * 5 : 0); break;
        case EChessType::MA: Value += Center * 8 + FMath::Min(Rank, 7) * 5; break;
        case EChessType::JV: Value += Center * 3 + FMath::Min(Rank, 7) * 3; break;
        case EChessType::PAO: Value += Center * 5 + (MajorCount > 6 ? 15 : -10); break;
        case EChessType::SHI:
        case EChessType::XIANG: Value += MajorCount > 4 ? 15 : 0; break;
        default: break;
        }
        Score += Piece.Color == Color ? Value : -Value;
    }
    return Score;
}

// 最小价值攻击子交换序列；每次交换后重新判合法性，避免把牵制子当成有效保护。
int32 FChessPosition::StaticExchange(const FChessMove2P& Move) const
{
    if (!Move.IsValid() || Get(Move.to).Type == EChessType::EMPTY) return 0;
    FChessPosition Copy = *this;
    int32 Gains[32] = {};
    Gains[0] = PieceValue(Copy.Get(Move.to).Type);
    Copy.MakeMove(Move);
    EChessColor Side = Opponent(Get(Move.from).Color);
    int32 Depth = 0;
    while (Depth < 31 && Copy.Get(Move.to).Type != EChessType::JIANG)
    {
        FChessMove2P Cheapest;
        int32 Cost = MAX_int32;
        for (int32 I = 0; I < 90; ++I)
        {
            const auto Piece = Copy.Squares[I];
            const Position From(I / 9, I % 9);
            if (Piece.Type == EChessType::EMPTY || Piece.Color != Side || PieceValue(Piece.Type) >= Cost || !Copy.CanReach(From, Move.to)) continue;
            const FChessMove2P Capture(From, Move.to);
            const auto Victim = Copy.MakeMove(Capture);
            const bool Legal = !Copy.IsInCheck(Side);
            Copy.UndoMove(Capture, Victim);
            if (Legal) { Cheapest = Capture; Cost = PieceValue(Piece.Type); }
        }
        if (!Cheapest.IsValid()) break;
        ++Depth;
        Gains[Depth] = PieceValue(Copy.Get(Move.to).Type) - Gains[Depth - 1];
        Copy.MakeMove(Cheapest);
        Side = Opponent(Side);
    }
    // 任一方都可以停止交换，不能强迫对手完成明显亏损的后续吃子。
    while (Depth > 0) { Gains[Depth - 1] = -FMath::Max(-Gains[Depth - 1], Gains[Depth]); --Depth; }
    return Gains[0];
}

// 只生成伪合法活动图，估值不递归调用搜索；将帅安全仍由走法生成器严格保证。
int32 FChessPosition::EvaluateActivity(EChessColor Color) const
{
    int32 Values[2] = {}, Horses[2] = {}, Cannons[2] = {}, Rooks[2] = {}, Pressure[2] = {};
    int32 PawnFiles[2][9] = {};
    int32 MajorCount = 0;
    for (const auto& Piece : Squares)
        if (Piece.Type == EChessType::MA || Piece.Type == EChessType::PAO || Piece.Type == EChessType::JV) ++MajorCount;
    const Position Kings[] = {FindKing(EChessColor::REDCHESS), FindKing(EChessColor::BLACKCHESS)};
    TArray<FChessMove2P> Moves;
    Moves.Reserve(32);
    for (int32 I = 0; I < 90; ++I)
    {
        const auto Piece = Squares[I];
        if (Piece.Type == EChessType::EMPTY || static_cast<uint8>(Piece.Color) > 1) continue;
        const int32 S = static_cast<uint8>(Piece.Color);
        const Position From(I / 9, I % 9);
        const bool Major = Piece.Type == EChessType::JV || Piece.Type == EChessType::MA || Piece.Type == EChessType::PAO;
        Moves.Reset();
        if (Major || Piece.Type == EChessType::JIANG) GeneratePieceMoves(From, Moves);
        if (Major)
        {
            Values[S] += Moves.Num() * (Piece.Type == EChessType::JV ? 3 : 5);
            if (Piece.Type == EChessType::MA) ++Horses[S];
            if (Piece.Type == EChessType::PAO) ++Cannons[S];
            if (Piece.Type == EChessType::JV) ++Rooks[S];
            for (const auto& M : Moves)
            {
                const auto King = Kings[1 - S];
                if (IsInside(King) && FMath::Abs(M.to.X - King.X) + FMath::Abs(M.to.Y - King.Y) <= 2) ++Pressure[S];
                const auto Target = Get(M.to);
                if (Target.Type != EChessType::EMPTY) Values[S] += FMath::Min(24, PieceValue(Target.Type) / 35);
            }
            if (Moves.Num() <= 1) Values[S] -= 32; // 受困车马炮不应按完全活动子估值。
        }
        if (Piece.Type == EChessType::BING)
        {
            ++PawnFiles[S][From.Y];
            const int32 Rank = S == 0 ? From.X : 9 - From.X;
            if (Rank >= 5)
            {
                for (int32 File : {From.Y - 1, From.Y + 1})
                    if (IsInside({From.X, File}) && Get({From.X, File}).Type == EChessType::BING && Get({From.X, File}).Color == Piece.Color)
                        Values[S] += MajorCount <= 4 ? 20 : 10;
                // 到底兵失去向前活动空间，残局尤其不能过度估价。
                if (Rank == 9) Values[S] -= 30;
            }
        }
        if (Piece.Type == EChessType::JIANG)
        {
            Values[S] += Moves.Num() * 4;
            int32 Guards = 0;
            for (const auto& P : Squares) if (P.Color == Piece.Color && (P.Type == EChessType::SHI || P.Type == EChessType::XIANG)) ++Guards;
            Values[S] += Guards * FMath::Min(12, MajorCount * 2);
        }
    }
    for (int32 S = 0; S < 2; ++S)
    {
        Values[S] += FMath::Min(200, Pressure[S] * (4 + FMath::Min(8, Pressure[S])));
        if (Rooks[S] >= 2) Values[S] += 18;
        if (Horses[S] && Cannons[S]) Values[S] += 22;
        if (MajorCount <= 4) Values[S] += Horses[S] * 12 - Cannons[S] * 8;
        for (int32 File = 0; File < 9; ++File) Values[S] -= FMath::Max(0, PawnFiles[S][File] - 1) * 12;
    }
    return Color == EChessColor::REDCHESS ? Values[0] - Values[1] : Values[1] - Values[0];
}

// 生成稳定局面键，供置换表和重复局面检测使用。
uint64 FChessPosition::Hash(EChessColor Color) const
{
    uint64 Key = 1469598103934665603ull;
    for (const FChessPiece2P& Piece : Squares)
    {
        Key ^= static_cast<uint8>(Piece.Type) | (Piece.Type == EChessType::EMPTY ? 0 : static_cast<uint8>(Piece.Color) << 4);
        Key *= 1099511628211ull;
    }
    Key ^= static_cast<uint8>(Color) | (bVariantRules ? 4 : 0) | (bRequireKing ? 8 : 0) | (bFlyingKingCapture ? 0 : 16);
    return Key;
}
