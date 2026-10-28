// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once
#include "ChessPosition.h"

// 后台搜索独占的学习快照，不持有UObject引用。
struct XIANGQIPRO_API FChessLearning
{
    int32 Observations = 0;
    int32 PieceCounts[8] = {};
    int32 FileCounts[9] = {};
    int32 Captures = 0;
    int32 Advances = 0;
    int32 Checks = 0;
    TMap<FString, int32> PositionMoves;
    TMap<FString, int32> AICaution;
    TMap<FString, int32> HarmfulReplies;
    int32 AvoidancePenalty(const FChessPosition& Board, const FChessMove2P& Move) const;
    int32 MovePriority(const FChessPosition& Board, const FChessMove2P& Move) const;
    int32 StyleEvaluation(const FChessPosition& Board) const;
    static FString SampleKey(const FChessPosition& Board, const FChessMove2P& Move,
                             EChessColor Side = EChessColor::REDCHESS);
};
