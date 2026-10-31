#include "ChessCheckHistory.h"

void FChessCheckHistory::Reset()
{
    Checks[0].Reset();
    Checks[1].Reset();
}

uint64 FChessCheckHistory::CheckKey(const FChessPosition& After, EChessColor Attacker)
{
    const auto King = After.FindKing(FChessPosition::Opponent(Attacker));
    if (!FChessPosition::IsInside(King)) return 0;
    uint64 Key = 1469598103934665603ull;
    bool Checking = false;
    for (int32 I = 0; I < 90; ++I)
    {
        const auto Piece = After.Squares[I];
        if (Piece.Type == EChessType::EMPTY || Piece.Color != Attacker || !After.CanReach({I / 9, I % 9}, King)) continue;
        Key = (Key ^ (uint64(I) * 8 + uint8(Piece.Type))) * 1099511628211ull;
        Checking = true;
    }
    // 记录攻击棋子及将帅落点，不能靠无关棋子挪动规避长将限制。
    return Checking ? (Key ^ uint64(King.X * 9 + King.Y + 1)) : 0;
}

void FChessCheckHistory::Record(const FChessPosition& Before, const FChessMove2P& Move)
{
    if (!Move.IsValid()) return;
    const auto Piece = Before.Get(Move.from);
    if (Piece.Type == EChessType::EMPTY || uint8(Piece.Color) > 1) return;
    // 吃子、兵卒前进改变战局，新的攻势可以重新计算。
    if (Before.Get(Move.to).Type != EChessType::EMPTY || (Piece.Type == EChessType::BING && Move.from.X != Move.to.X)) Reset();
    FChessPosition After = Before;
    After.MakeMove(Move);
    const uint64 Key = CheckKey(After, Piece.Color);
    if (Key) ++Checks[uint8(Piece.Color)].FindOrAdd(Key);
}

bool FChessCheckHistory::WouldRepeat(const FChessPosition& Before, const FChessMove2P& Move) const
{
    const auto Piece = Before.Get(Move.from);
    if (!Move.IsValid() || Piece.Type == EChessType::EMPTY || uint8(Piece.Color) > 1 ||
        Before.Get(Move.to).Type != EChessType::EMPTY || (Piece.Type == EChessType::BING && Move.from.X != Move.to.X)) return false;
    FChessPosition After = Before;
    After.MakeMove(Move);
    const uint64 Key = CheckKey(After, Piece.Color);
    const int32* Count = Checks[uint8(Piece.Color)].Find(Key);
    if (!Key || !Count || *Count < 2) return false;
    // 真正结束对局的将杀不是重复拉扯。
    return !After.GenerateLegalMoves(FChessPosition::Opponent(Piece.Color)).IsEmpty();
}
