// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChessSearch.h"
#include "AI2P.generated.h"

class UChessBoard2P;

UENUM(BlueprintType)
enum class EAI2PDifficulty : uint8
{
    Easy = 0,   // 简单
    Normal = 1, // 普通
    Hard = 2,   // 困难
    Master = 3  // 大师
};

// AI入口只负责预算与快照，具体规则和搜索独立于UObject。
UCLASS()
class XIANGQIPRO_API UAI2P : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI2P")
    EAI2PDifficulty AIDifficulty = EAI2PDifficulty::Hard;

    // 在游戏线程生成快照并创建独立搜索请求。
    TSharedPtr<FChessSearch, ESPMode::ThreadSafe> CreateSearch(TWeakObjectPtr<UChessBoard2P> Board, EChessColor Color, EAI2PDifficulty Difficulty);
    // 保留同步入口；耗时搜索应优先通过GameState异步运行。
    FChessMove2P GetBestMove(TWeakObjectPtr<UChessBoard2P> Board, EChessColor Color, EAI2PDifficulty Difficulty);
    // 取消最近一次请求，不阻塞游戏线程。
    UFUNCTION(BlueprintCallable, Category = "Chess AI")
    void StopThinkingImmediately();
    // 设置兼容接口所用的棋盘引用，只能在游戏线程调用。
    void SetBoard(TWeakObjectPtr<UChessBoard2P> NewBoard);
    // 判断指定阵营是否无合法走法，包含将死和困毙。
    bool IsJueSha(EChessColor Color);
    // 子系统销毁时取消尚未结束的请求。
    virtual void Deinitialize() override;

private:
    TWeakObjectPtr<UChessBoard2P> CurrentBoard;
    TSharedPtr<FChessSearch, ESPMode::ThreadSafe> ActiveSearch;
};
