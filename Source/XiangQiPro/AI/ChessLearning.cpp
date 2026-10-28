#include "ChessLearning.h"

FString FChessLearning::SampleKey(const FChessPosition& Board, const FChessMove2P& Move, EChessColor Side)
{
    // 使用棋子完整分布和执棋方，不能把相似坐标误当成相同局面。
    return FString::Printf(TEXT("%016llx_%d_%d"), static_cast<unsigned long long>(Board.Hash(Side)),
                           Move.from.X * 9 + Move.from.Y, Move.to.X * 9 + Move.to.Y);
}

int32 FChessLearning::MovePriority(const FChessPosition& Board, const FChessMove2P& Move) const
{
    if (Observations == 0)
        return 0;
    const int32 Type = static_cast<uint8>(Board.Get(Move.from).Type);
    if (Type < 1 || Type > 7 || !FChessPosition::IsInside(Move.to))
        return 0;
    const int32* Seen = PositionMoves.Find(SampleKey(Board, Move));
    const int32* Refutation = HarmfulReplies.Find(SampleKey(Board, Move));
    int32 Score = Seen ? FMath::Min(*Seen, 50) * 200 : 0;
    Score += 1000 * PieceCounts[Type] / Observations + 600 * FileCounts[Move.to.Y] / Observations;
    if (Move.to.X > Move.from.X)
        Score += 500 * Advances / Observations;
    if (Board.Get(Move.to).Type != EChessType::EMPTY)
        Score += 500 * Captures / Observations;
    // 曾造成劣势的反击会更早进入搜索，但仍由合法走法生成器过滤。
    return FMath::Min(15000, Score) + (Refutation ? FMath::Min(100, *Refutation) * 500 : 0);
}

int32 FChessLearning::AvoidancePenalty(const FChessPosition& Board, const FChessMove2P& Move) const
{
    const int32* Risk = AICaution.Find(SampleKey(Board, Move, EChessColor::BLACKCHESS));
    // 历史反例影响根节点选择；有限惩罚允许搜索推翻过时经验。
    return Risk ? FMath::Min(120, *Risk * 12) : 0;
}

int32 FChessLearning::StyleEvaluation(const FChessPosition& Board) const
{
    if (Observations < 4)
        return 0;
    int32 Score = 0;
    // 玩家经常使用的进攻棋子/路线获得额外威胁权重。
    for (int32 I = 0; I < 90; ++I)
    {
        const FChessPiece2P& Piece = Board.Squares[I];
        const int32 Type = static_cast<uint8>(Piece.Type);
        if (Piece.Color != EChessColor::REDCHESS || Type < 2 || Type > 7)
            continue;
        Score += PieceCounts[Type] * FMath::Max(0, I / 9 - 3) * 12 / Observations;
        Score += FileCounts[I % 9] * 5 / Observations;
    }
    return FMath::Clamp(Score, 0, 35);
}
