// Copyright 2026 Ultimate Player All Rights Reserved.
#include "ChessMoveAssessment.h"

namespace
{
    constexpr int32 Mate = 100000;
    constexpr int32 NodeLimit = 24000;

    struct FAssessmentSearch
    {
        FChessPosition Board;
        int32 Nodes = 0;
        bool bAborted = false;

        int32 Search(EChessColor Side, int32 Depth, int32 Captures, int32 Ply, int32 Alpha, int32 Beta)
        {
            if (++Nodes > NodeLimit) { bAborted = true; return 0; }
            if (!FChessPosition::IsInside(Board.FindKing(Side))) return -Mate;
            if (!FChessPosition::IsInside(Board.FindKing(FChessPosition::Opponent(Side)))) return Mate;
            auto Moves = Board.GenerateLegalMoves(Side);
            if (Moves.IsEmpty()) return -Mate;
            const bool InCheck = Board.IsInCheck(Side);
            int32 Best = -Mate;
            if (Depth <= 0)
            {
                const int32 StandPat = Board.Evaluate(Side);
                if (Ply >= 6 || (!InCheck && Captures <= 0)) return StandPat;
                if (!InCheck)
                {
                    Best = StandPat;
                    Alpha = FMath::Max(Alpha, Best);
                    if (Alpha >= Beta) return Best;
                    Moves.RemoveAll([&](const FChessMove2P& M) { return Board.Get(M.to).Type == EChessType::EMPTY; });
                }
            }
            for (const auto& M : Moves)
            {
                const auto Captured = Board.MakeMove(M);
                const int32 Value = -Search(FChessPosition::Opponent(Side), Depth - 1,
                    Depth <= 0 ? Captures - 1 : Captures, Ply + 1, -Beta, -Alpha);
                Board.UndoMove(M, Captured);
                if (bAborted) return 0;
                Best = FMath::Max(Best, Value);
                Alpha = FMath::Max(Alpha, Value);
                if (Alpha >= Beta) break;
            }
            return Best;
        }
    };
}

FChessMoveAssessment FChessMoveAssessor::Assess(const FChessPosition& Before, const FChessMove2P& Move)
{
    FChessMoveAssessment Result;
    if (!Move.IsValid() || Before.Get(Move.from).Type == EChessType::EMPTY) return Result;
    FAssessmentSearch Search{Before};
    const EChessColor Side = Before.Get(Move.from).Color;
    const auto Moves = Search.Board.GenerateLegalMoves(Side);
    const int32 PlayedIndex = Moves.IndexOfByPredicate([&](const FChessMove2P& M) { return M.from == Move.from && M.to == Move.to; });
    if (PlayedIndex == INDEX_NONE) return Result;
    TArray<int32> Values;
    // 完整静态层作为确定性回退；达到节点上限时绝不混用未完成的搜索层。
    for (const auto& M : Moves)
    {
        const auto Captured = Search.Board.MakeMove(M);
        Values.Add(Captured.Type == EChessType::JIANG ? Mate : Search.Board.Evaluate(Side));
        Search.Board.UndoMove(M, Captured);
    }
    for (int32 Depth = 1; Depth <= 2; ++Depth)
    {
        TArray<int32> Layer;
        for (const auto& M : Moves)
        {
            const auto Captured = Search.Board.MakeMove(M);
            Layer.Add(-Search.Search(FChessPosition::Opponent(Side), Depth - 1, 2, 1, -Mate - 1, Mate + 1));
            Search.Board.UndoMove(M, Captured);
            if (Search.bAborted) break;
        }
        if (Search.bAborted) break;
        Values = MoveTemp(Layer);
    }
    Result.bValid = true;
    Result.Nodes = Search.Nodes;
    Result.PlayedValue = Values[PlayedIndex];
    Result.BestValue = Values[0];
    for (int32 Value : Values) Result.BestValue = FMath::Max(Result.BestValue, Value);
    // 最佳选择+20；每损失5点棋力少得1分，明显失误会扣分，累计总分可为负。
    const int32 Loss = FMath::Max(0, Result.BestValue - Result.PlayedValue);
    Result.Points = FMath::Clamp(20 - FMath::DivideAndRoundUp(Loss, 5), -200, 20);
    return Result;
}
