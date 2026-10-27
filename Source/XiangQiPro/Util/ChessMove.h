// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChessMove.generated.h"

typedef FIntPoint Position;

 // 走法结构
USTRUCT(BlueprintType)
struct FChessMove2P
{
    GENERATED_USTRUCT_BODY()
    Position from;
    Position to;
    int32 score;  // 用于评估走法的得分
    bool bIsValid = false;

    // 默认走法无效，避免取消或无棋可走时误落子。
    FChessMove2P(Position f = Position(-1, -1), Position t = Position(-1, -1), int32 s = 0)
        : from(f), to(t), score(s), bIsValid(f != t && f.X >= 0 && f.X < 10 && f.Y >= 0 && f.Y < 9 && t.X >= 0 && t.X < 10 && t.Y >= 0 && t.Y < 9)
    {
    }

    // 同时检查有效标志和坐标范围。
    bool IsValid() const
    {
        return bIsValid && from != to && from.X >= 0 && from.X < 10 && from.Y >= 0 && from.Y < 9 && to.X >= 0 && to.X < 10 && to.Y >= 0 && to.Y < 9;
    }

    // 按走法分数比较。
    bool operator<(const FChessMove2P& other) const
    {
        return score < other.score;
    }

    // 比较走法坐标与评分。
    bool operator==(const FChessMove2P& other) const
    {
        return from == other.from && to == other.to && score == other.score;
    }
};

UCLASS()
class XIANGQIPRO_API UMove : public UObject
{
    GENERATED_BODY()
};
