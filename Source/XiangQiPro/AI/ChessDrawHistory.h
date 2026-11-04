// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "ChessPosition.h"
#include "ChessDrawHistory.generated.h"

/** 经典 AI 自动和棋的原因。长将与追子不能套用闲着循环和棋。 */
UENUM()
enum class EClassicDrawReason : uint8 { None, Repetition, NoCapture, InsufficientMaterial };

/**
 * 记录实际提交的棋局，搜索试走不修改本结构。完整局面串包含执棋方，避免仅凭
 * 棋子落点或散列碰撞误判；数据上限与自然限着一致，允许随连胜棋局存档恢复。
 */
USTRUCT()
struct XIANGQIPRO_API FChessDrawHistory
{
    GENERATED_BODY()
    static constexpr int32 NoCaptureLimit = 120; // 双方各 60 着，即 120 个半回合。
    UPROPERTY(SaveGame) int32 NoCapturePlies = 0;
    UPROPERTY(SaveGame) TArray<FString> QuietPositions;

    void Reset(const FChessPosition& Board, EChessColor Turn);
    void Record(const FChessPosition& Before, const FChessMove2P& Move);
    EClassicDrawReason Evaluate(const FChessPosition& Board) const;
    bool IsValid() const;
    bool Matches(const FChessPosition& Board, EChessColor Turn) const;
private:
    static FString PositionKey(const FChessPosition& Board, EChessColor Turn);
    static bool CreatesCaptureThreat(FChessPosition Before, FChessPosition After, EChessColor Side);
};
