// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessBoard2P.h"
#include "ChessBoard2PActor.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/GameObject/SettingPoint.h"

// 子系统初始为空，棋盘Actor就绪后再建立坐标。
UChessBoard2P::UChessBoard2P()
{
}

// 清理上一局数据，避免子系统跨关卡保留旧Actor和重复行。
void UChessBoard2P::ResetBoard()
{
    check(IsInGameThread());
    for (const auto& Row : AllChess)
        for (const auto& Chess : Row)
            if (Chess.IsValid()) Chess->Destroy();
    for (const auto& Row : SettingPoints)
        for (const auto& Point : Row)
            if (Point.IsValid()) Point->Destroy();
    MarkPositionChanged();
    AllChess.Reset();
    SettingPoints.Reset();
    BoardLocs.Reset();
    sideToMove = EChessColor::REDCHESS;
    CheckHistory.Reset();
}

// 初始化固定大小棋盘和世界坐标。
void UChessBoard2P::InitializeBoard(TWeakObjectPtr<AChessBoard2PActor> ChessBoard2PActor)
{
    check(IsInGameThread());
    ResetBoard();
    if (!ChessBoard2PActor.IsValid()) return;
    AllChess.SetNum(10);
    SettingPoints.SetNum(10);
    BoardLocs.SetNum(10);
    const FVector Start = ChessBoard2PActor->BorderLoc1;
    const FVector End = ChessBoard2PActor->BorderLoc2;
    for (int32 X = 0; X < 10; ++X)
    {
        AllChess[X].SetNum(9);
        SettingPoints[X].SetNum(9);
        BoardLocs[X].SetNum(9);
        for (int32 Y = 0; Y < 9; ++Y)
            BoardLocs[X][Y] = FVector(Start.X + (End.X - Start.X) / 9.f * X - 0.02f * X,
                Start.Y + (End.Y - Start.Y) / 8.f * Y, Start.Z);
    }
}

// 主线程串行读取90格，后台不再访问这些弱指针。
FChessPosition UChessBoard2P::CreateSnapshot() const
{
    check(IsInGameThread());
    FChessPosition Snapshot;
    Snapshot.bVariantRules = bVariantRules;
    Snapshot.bRequireKing = bRequireKing;
    Snapshot.bFlyingKingCapture = bFlyingKingCapture;
    for (int32 X = 0; X < 10; ++X)
        for (int32 Y = 0; Y < 9; ++Y)
            if (const auto Chess = GetChess(X, Y); Chess.IsValid() && !Chess->IsDead())
                Snapshot.Set(Position(X, Y), {Chess->GetType(), Chess->GetColor()});
    return Snapshot;
}

// 落子入口同时验证坐标、棋子身份和将帅安全。
bool UChessBoard2P::IsLegalMove(TWeakObjectPtr<AChesses> Target, const FChessMove2P& Move) const
{
    if (!Target.IsValid() || Target->IsDead() || !Move.IsValid() || GetChess(Move.from) != Target) return false;
    FChessPosition Snapshot = CreateSnapshot();
    if (!Snapshot.CanReach(Move.from, Move.to)) return false;
    Snapshot.MakeMove(Move);
    return !Snapshot.IsInCheck(Target->GetColor());
}

// 按最新走法增删提示，仍合法的点保留显示和粒子状态。
void UChessBoard2P::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target)
{
    if (!Target.IsValid()) return;
    TSet<FIntPoint> Destinations;
    for (const FChessMove2P& Move : Moves)
    {
        if (Move.IsValid()) Destinations.Add(Move.to);
    }
    for (int32 X = 0; X < SettingPoints.Num(); ++X)
        for (int32 Y = 0; Y < SettingPoints[X].Num(); ++Y)
        {
            const auto Point = SettingPoints[X][Y];
            if (!Point.IsValid()) continue;
            const bool bShow = Destinations.Contains(FIntPoint(X, Y));
            Point->SetTargetChess(bShow ? Target : TWeakObjectPtr<AChesses>());
            Point->SetActivate(bShow);
        }
}

// 安全隐藏所有现有落子提示。
void UChessBoard2P::DismissSettingPoint2P()
{
    for (const auto& Row : SettingPoints)
        for (const auto& Point : Row)
            if (Point.IsValid())
            {
                Point->SetActivate(false);
                Point->SetTargetChess(nullptr);
            }
}

// 设置当前执棋阵营。
void UChessBoard2P::SetSideToMove(EChessColor Color)
{
    sideToMove = Color;
}

// 未初始化或坐标越界时返回空引用。
TWeakObjectPtr<AChesses> UChessBoard2P::GetChess(int32 X, int32 Y) const
{
    return IsValidPosition(X, Y) && AllChess.IsValidIndex(X) && AllChess[X].IsValidIndex(Y) ? AllChess[X][Y] : nullptr;
}

// 通过简化坐标读取棋子。
TWeakObjectPtr<AChesses> UChessBoard2P::GetChess(FIntPoint Pos) const
{
    return GetChess(Pos.X, Pos.Y);
}

// 检查实际数组长度后更新棋子。
void UChessBoard2P::SetChess(int32 X, int32 Y, TWeakObjectPtr<AChesses> Chess)
{
    check(IsInGameThread());
    if (IsValidPosition(X, Y) && AllChess.IsValidIndex(X) && AllChess[X].IsValidIndex(Y))
    {
        AllChess[X][Y] = Chess;
        MarkPositionChanged();
    }
}

// 提交棋盘变化后播放动画，吃将事件可立即结束游戏。
void UChessBoard2P::ApplyMove(TWeakObjectPtr<AChesses> Target, FChessMove2P Move)
{
    if (!IsLegalMove(Target, Move)) return;
    CheckHistory.Record(CreateSnapshot(), Move);
    const auto Captured = GetChess(Move.to);
    Target->PrepareCapture(Captured.Get());
    SetChess(Move.to.X, Move.to.Y, Target);
    SetChess(Move.from.X, Move.from.Y, nullptr);
    if (Target.IsValid()) Target->ApplyMove(Move);
}

// 输出当前棋盘的棋子类型，便于调试。
void UChessBoard2P::DebugCheckBoardState() const
{
    for (int32 X = 9; X >= 0; --X)
    {
        FString Row;
        for (int32 Y = 0; Y < 9; ++Y)
        {
            const auto Chess = GetChess(X, Y);
            Row += FString::Printf(TEXT("%d "), Chess.IsValid() ? static_cast<int32>(Chess->GetType()) : 0);
        }
        ULogger::Log(Row);
    }
}

// 保留旧调用接口；只允许在游戏线程试走，搜索使用FChessPosition。
void UChessBoard2P::MakeTestMove(FChessMove2P Move, TWeakObjectPtr<AChesses> MovedPiece)
{
    if (!Move.IsValid() || !MovedPiece.IsValid()) return;
    SetChess(Move.to.X, Move.to.Y, MovedPiece);
    SetChess(Move.from.X, Move.from.Y, nullptr);
}

// 恢复游戏线程中的兼容试走数据。
void UChessBoard2P::UndoTestMove(FChessMove2P Move, TWeakObjectPtr<AChesses> MovedPiece, TWeakObjectPtr<AChesses> CapturedPiece)
{
    if (!Move.IsValid()) return;
    SetChess(Move.from.X, Move.from.Y, MovedPiece);
    SetChess(Move.to.X, Move.to.Y, CapturedPiece);
}

// 判断棋盘坐标范围。
bool UChessBoard2P::IsValidPosition(int32 X, int32 Y) const noexcept
{
    return FChessPosition::IsInside(Position(X, Y));
}

// 使用统一规则判断九宫范围。
bool UChessBoard2P::IsInPalace(int32 X, int32 Y, EChessColor Color) const noexcept
{
    return FChessPosition::IsInPalace(Position(X, Y), Color);
}

// 获取直线上两个位置之间的棋子数。
int32 UChessBoard2P::CountPiecesBetween(int32 FromX, int32 FromY, int32 ToX, int32 ToY) const
{
    return CreateSnapshot().CountBetween(Position(FromX, FromY), Position(ToX, ToY));
}

// 玩家与AI共用完整合法走法生成。
TArray<FChessMove2P> UChessBoard2P::GenerateAllMoves(EChessColor Color)
{
    return CreateSnapshot().GenerateLegalMoves(Color);
}

// 为选中棋子筛选合法走法。
TArray<FChessMove2P> UChessBoard2P::GenerateMovesForChess(int32 X, int32 Y, TWeakObjectPtr<AChesses> Chess)
{
    TArray<FChessMove2P> Moves;
    if (!Chess.IsValid() || Chess->IsDead() || GetChess(X, Y) != Chess) return Moves;
    for (const FChessMove2P& Move : GenerateAllMoves(Chess->GetColor()))
        if (Move.from == Position(X, Y)) Moves.Add(Move);
    return Moves;
}

// 检查将帅之间是否没有棋子阻隔。
bool UChessBoard2P::AreKingsFacingEachOther() const
{
    return CountPiecesBetweenKings() == 0;
}

// 获取将帅之间的棋子数，不同列或缺少将帅时返回-1。
int32 UChessBoard2P::CountPiecesBetweenKings() const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const Position Red = Snapshot.FindKing(EChessColor::REDCHESS);
    const Position Black = Snapshot.FindKing(EChessColor::BLACKCHESS);
    return Red.Y == Black.Y ? Snapshot.CountBetween(Red, Black) : -1;
}

// 生成将帅直接照面的吃子走法。
void UChessBoard2P::GenerateKingDirectAttackMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const Position To = Snapshot.FindKing(FChessPosition::Opponent(Color));
    if (Y == To.Y && Snapshot.CanReach(Position(X, Y), To)) Moves.Emplace(Position(X, Y), To);
}

// 使用统一攻击规则判断将军。
bool UChessBoard2P::IsKingInCheck(EChessColor Color)
{
    return CreateSnapshot().IsInCheck(Color);
}

// 检查指定阵营的棋子能否攻击目标位置。
bool UChessBoard2P::CanAttackPosition(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    return Snapshot.Get(Position(FromX, FromY)).Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成将帅的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateJiangMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::JIANG && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断将帅的攻击范围。
bool UChessBoard2P::CanJiangAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::JIANG && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成士仕的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateShiMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::SHI && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断士仕的攻击范围。
bool UChessBoard2P::CanShiAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::SHI && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成象相的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateXiangMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::XIANG && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断象相的攻击范围。
bool UChessBoard2P::CanXiangAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::XIANG && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成马的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateMaMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::MA && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断马的攻击范围。
bool UChessBoard2P::CanMaAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::MA && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成车的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateJvMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::JV && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断车的攻击范围。
bool UChessBoard2P::CanJvAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::JV && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成炮的伪合法走法，保留旧接口。
void UChessBoard2P::GeneratePaoMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::PAO && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断炮的攻击范围。
bool UChessBoard2P::CanPaoAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::PAO && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}

// 使用统一规则生成兵卒的伪合法走法，保留旧接口。
void UChessBoard2P::GenerateBingMoves(int32 X, int32 Y, EChessColor Color, TArray<FChessMove2P>& Moves) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(X, Y));
    if (Piece.Type == EChessType::BING && Piece.Color == Color) Snapshot.GeneratePieceMoves(Position(X, Y), Moves);
}

// 使用统一规则判断兵卒的攻击范围。
bool UChessBoard2P::CanBingAttack(int32 FromX, int32 FromY, int32 ToX, int32 ToY, EChessColor Color) const
{
    const FChessPosition Snapshot = CreateSnapshot();
    const FChessPiece2P Piece = Snapshot.Get(Position(FromX, FromY));
    return Piece.Type == EChessType::BING && Piece.Color == Color && Snapshot.CanReach(Position(FromX, FromY), Position(ToX, ToY));
}
