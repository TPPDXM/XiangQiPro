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
        TEXT("0726 6252 2124 9776 0818 9172 3646 7778")
    };

    FChessPosition StartingPosition()
    {
        FChessPosition Board;
        constexpr EChessType Back[] = {EChessType::JV, EChessType::MA, EChessType::XIANG,
            EChessType::SHI, EChessType::JIANG, EChessType::SHI, EChessType::XIANG, EChessType::MA, EChessType::JV};
        for (int32 Y = 0; Y < 9; ++Y)
        {
            Board.Set({0, Y}, {Back[Y], EChessColor::REDCHESS});
            Board.Set({9, Y}, {Back[Y], EChessColor::BLACKCHESS});
            if (Y % 2 == 0)
            {
                Board.Set({3, Y}, {EChessType::BING, EChessColor::REDCHESS});
                Board.Set({6, Y}, {EChessType::BING, EChessColor::BLACKCHESS});
            }
        }
        for (int32 Y : {1, 7})
        {
            Board.Set({2, Y}, {EChessType::PAO, EChessColor::REDCHESS});
            Board.Set({7, Y}, {EChessType::PAO, EChessColor::BLACKCHESS});
        }
        return Board;
    }
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
                FChessPosition Board = StartingPosition();
                EChessColor Turn = EChessColor::REDCHESS;
                TArray<FString> Moves;
                FString(Line).ParseIntoArrayWS(Moves);
                for (const FString& Code : Moves)
                {
                    FChessMove2P Move({Code[0] - '0', Code[1] - '0'}, {Code[2] - '0', Code[3] - '0'});
                    if (bMirror) { Move.from.Y = 8 - Move.from.Y; Move.to.Y = 8 - Move.to.Y; }
                    const auto Legal = Board.GenerateLegalMoves(Turn);
                    if (!ensureMsgf(Legal.ContainsByPredicate([&](const FChessMove2P& M)
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
