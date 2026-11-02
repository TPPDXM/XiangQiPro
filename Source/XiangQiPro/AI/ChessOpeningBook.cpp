// Copyright 2026 Ultimate Player All Rights Reserved.
#include "ChessOpeningBook.h"

namespace
{
    // 手工整理的中炮对屏风马、顺手炮、列手炮、仙人指路、飞相及起马布局。
    // 四位数字依次为起点行列、终点行列。红方第0行，黑方第9行。
    constexpr const TCHAR* Lines[] = {
        TEXT("2124 9776 0726 9172 0818 6656 1814 7778"),
        TEXT("2124 9776 0726 9172 0818 9888 1814 6252 0122 7778"),
        TEXT("2124 7774 0726 9776 0818 9888 1814 6656"),
        TEXT("2124 7174 0726 9172 0818 9080 1814 6252"),
        TEXT("3646 9776 0726 9172 2124 7778 0818 6656"),
        TEXT("3646 6252 0726 9172 2124 9776 0818 9888"),
        TEXT("0224 9776 0726 6252 3646 9172 0818 7778"),
        TEXT("0726 9776 2124 9172 0818 7778 1814 6656"),
        TEXT("2124 6252 0726 9776 0818 9172 3646 7778"),
        TEXT("3646 7175 2124 9776 0726 9172 0818 9888"),
        TEXT("0224 6252 0726 9776 3646 9172 0818 7778"),
        TEXT("0726 6252 2124 9776 0818 9172 3646 7778"),
        // 补充过宫炮、仕角炮、金钩炮、边马、挺兵和左右中炮的应对。
        // 所有坐标谱在建库时逐手验证；左右镜像和不同走子次序自动汇合到同一局面键。
        TEXT("2125 9776 0726 9172 0818 9888 1814 6252 3646 7778"),
        TEXT("2123 9776 0726 9172 0818 6252 3646 9888"),
        TEXT("2726 9776 0122 9172 0010 6252 3040 9888"),
        TEXT("0120 9776 2124 9172 0010 6252 0726 9888"),
        TEXT("0728 9172 2724 9776 0818 6656 0122 9080"),
        TEXT("3040 9776 0122 9172 2724 6252 0010 7778"),
        TEXT("3444 9776 0726 9172 2124 6252 0818 9888"),
        TEXT("3848 9172 0726 9776 2124 6656 0818 9080"),
        TEXT("0314 9776 0726 9172 2124 6252 0818 9888"),
        TEXT("0224 7174 0122 9776 0010 6252 3040 9172"),
        TEXT("0224 7774 0726 9172 0818 6656 3646 9776"),
        TEXT("0726 7174 2124 9776 0818 9172 0122 6252"),
        TEXT("3646 7774 0726 9776 2124 9172 0818 6252"),
        TEXT("3646 6656 0726 9776 2124 9172 0818 6252"),
        TEXT("2124 9776 0122 9172 0010 9888 3040 6252"),
        TEXT("2724 9172 0122 9776 0010 6050 1014 7170"),
        TEXT("2124 9776 0726 9172 0818 6252 1816 9888 0122 7778"),
        TEXT("2124 9172 0122 9776 0010 6656 1012 9080 0726 7170"),
        TEXT("0224 9776 0122 9172 0010 6252 3040 9888 0726 7778"),
        TEXT("0726 9776 0122 9172 0224 6252 0818 9888 3646 7778")
    };


}

TArray<FChessMove2P> FChessOpeningBook::FindMoves(const FChessPosition& Position, EChessColor Side)
{
    if (Position.bVariantRules || !Position.bRequireKing || !Position.bFlyingKingCapture) return {};
    // C++静态初始化保证并发搜索只构建一次，之后所有线程只读。
    static const TMap<uint64, TArray<FChessMove2P>> Book = []
    {
        TMap<uint64, TArray<FChessMove2P>> Result;
        for (const TCHAR* Line : Lines)
            for (bool bMirror : {false, true})
            {
                FChessPosition Board = FChessPosition::Standard();
                EChessColor Turn = EChessColor::REDCHESS;
                TArray<FString> Moves;
                FString(Line).ParseIntoArrayWS(Moves);
                for (const FString& Code : Moves)
                {
                    FChessMove2P Move({Code[0] - '0', Code[1] - '0'}, {Code[2] - '0', Code[3] - '0'});
                    if (bMirror) { Move.from.Y = 8 - Move.from.Y; Move.to.Y = 8 - Move.to.Y; }
                    const auto Legal = Board.GenerateLegalMoves(Turn);
                    if (!ensureAlwaysMsgf(Legal.ContainsByPredicate([&](const FChessMove2P& M)
                        { return M.from == Move.from && M.to == Move.to; }), TEXT("Invalid opening move %s in %s"), *Code, Line)) break;
                    auto& Choices = Result.FindOrAdd(Board.Hash(Turn));
                    Choices.AddUnique(Move);
                    Board.MakeMove(Move);
                    Turn = FChessPosition::Opponent(Turn);
                }
            }
        return Result;
    }();
    const auto* Moves = Book.Find(Position.Hash(Side));
    return Moves ? *Moves : TArray<FChessMove2P>();
}
