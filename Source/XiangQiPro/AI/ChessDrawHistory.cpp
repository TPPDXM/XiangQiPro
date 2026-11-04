// Copyright 2026 Ultimate Player All Rights Reserved.
#include "ChessDrawHistory.h"

FString FChessDrawHistory::PositionKey(const FChessPosition& Board, EChessColor Turn)
{
    FString Key;
    Key.Reserve(91);
    for (const FChessPiece2P& Piece : Board.Squares)
        Key.AppendChar(TCHAR('A' + (Piece.Type == EChessType::EMPTY ? 0 : uint8(Piece.Type) + 8 * uint8(Piece.Color))));
    Key.AppendChar(TCHAR('0' + uint8(Turn)));
    return Key;
}

void FChessDrawHistory::Reset(const FChessPosition& Board, EChessColor Turn)
{
    NoCapturePlies = 0;
    QuietPositions = {PositionKey(Board, Turn)};
}

bool FChessDrawHistory::CreatesCaptureThreat(FChessPosition Before, FChessPosition After, EChessColor Side)
{
    // 保守排除本手新增的合法吃子威胁，不把单方长捉误判成闲着循环。
    // 棋盘远处原已存在的吃子选择不影响闲着判定，例如开局炮隔炮吃马。
    // 不在这里重写比赛规则的交换、兑子与长捉细则；复杂攻势仍由自然限着收束。
    TSet<int32> Existing;
    const auto Key = [](const FChessMove2P& Move) { return (Move.from.X * 9 + Move.from.Y) * 90 + Move.to.X * 9 + Move.to.Y; };
    for (const FChessMove2P& Move : Before.GenerateLegalMoves(Side))
        if (Before.Get(Move.to).Type != EChessType::EMPTY) Existing.Add(Key(Move));
    for (const FChessMove2P& Move : After.GenerateLegalMoves(Side))
        if (After.Get(Move.to).Type != EChessType::EMPTY && !Existing.Contains(Key(Move))) return true;
    return false;
}

void FChessDrawHistory::Record(const FChessPosition& Before, const FChessMove2P& Move)
{
    if (!Move.IsValid() || Before.Get(Move.from).Type == EChessType::EMPTY) return;
    const FChessPiece2P Piece = Before.Get(Move.from);
    FChessPosition After = Before;
    const bool bCapture = After.MakeMove(Move).Type != EChessType::EMPTY;
    NoCapturePlies = bCapture ? 0 : FMath::Min(NoCaptureLimit, NoCapturePlies + 1);
    // 兵卒前进不可逆，重新建立循环窗口，但不重置象棋的“无吃子”计数。
    const bool bIrreversible = bCapture || (Piece.Type == EChessType::BING && Move.from.X != Move.to.X);
    const bool bAttack = After.IsInCheck(FChessPosition::Opponent(Piece.Color)) || CreatesCaptureThreat(Before, After, Piece.Color);
    if (bIrreversible || bAttack) QuietPositions.Reset();
    QuietPositions.Add(PositionKey(After, FChessPosition::Opponent(Piece.Color)));
    if (QuietPositions.Num() > NoCaptureLimit + 1) QuietPositions.RemoveAt(0);
}

EClassicDrawReason FChessDrawHistory::Evaluate(const FChessPosition& Board) const
{
    // 吃将、将杀和困毙须由调用方优先处理，缺将或正在被将军不能提前判和。
    for (EChessColor Side : {EChessColor::REDCHESS, EChessColor::BLACKCHESS})
        if (!FChessPosition::IsInside(Board.FindKing(Side))) return EClassicDrawReason::None;
    bool bOnlyDefenders = true;
    for (const FChessPiece2P& Piece : Board.Squares)
        if (Piece.Type != EChessType::EMPTY && Piece.Type != EChessType::JIANG &&
            Piece.Type != EChessType::SHI && Piece.Type != EChessType::XIANG) bOnlyDefenders = false;
    if (bOnlyDefenders && !Board.IsInCheck(EChessColor::REDCHESS) && !Board.IsInCheck(EChessColor::BLACKCHESS))
        return EClassicDrawReason::InsufficientMaterial;
    if (NoCapturePlies >= NoCaptureLimit) return EClassicDrawReason::NoCapture;

    // 必须是连续的同一循环重复，不能把 A-B-A-C-A 这类不同分支当作重复循环。
    const int32 Last = QuietPositions.Num() - 1;
    for (int32 Period = 2; Period * 2 <= Last; Period += 2)
    {
        bool bRepeated = true;
        for (int32 Offset = 0; Offset <= Period; ++Offset)
            if (QuietPositions[Last - Offset] != QuietPositions[Last - Period - Offset]) { bRepeated = false; break; }
        if (bRepeated) return EClassicDrawReason::Repetition;
    }
    return EClassicDrawReason::None;
}

bool FChessDrawHistory::IsValid() const
{
    if (NoCapturePlies < 0 || NoCapturePlies > NoCaptureLimit || QuietPositions.Num() > NoCaptureLimit + 1) return false;
    for (const FString& Key : QuietPositions)
    {
        if (Key.Len() != 91 || (Key[90] != '0' && Key[90] != '1')) return false;
        for (int32 Index = 0; Index < 90; ++Index) if (Key[Index] < 'A' || Key[Index] > 'P') return false;
    }
    return true;
}

bool FChessDrawHistory::Matches(const FChessPosition& Board, EChessColor Turn) const
{
    return IsValid() && !QuietPositions.IsEmpty() && QuietPositions.Last() == PositionKey(Board, Turn);
}
