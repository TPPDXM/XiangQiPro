// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SaveStorage.h"
#include "XiangQiPro/AI/ChessDrawHistory.h"
#include "SavePlayerProfile.generated.h"

USTRUCT()
struct FXQPChallengeProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 BestStreak = 0;
    UPROPERTY(SaveGame) int32 CurrentStreak = 0;
    UPROPERTY(SaveGame) bool bRunActive = false;
    UPROPERTY(SaveGame) uint8 RunDifficulty = 1;
    UPROPERTY(SaveGame) TArray<uint8> SavedBoard;
    UPROPERTY(SaveGame) uint8 SavedTurn = 0;
    UPROPERTY(SaveGame) int32 SavedRedMoveScore = 0;
    UPROPERTY(SaveGame) int32 SavedBlackMoveScore = 0;
    UPROPERTY(SaveGame) FChessDrawHistory SavedDrawHistory;
};

USTRUCT()
struct FXQPModeLearningData
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TArray<int32> PieceCounts;
    UPROPERTY(SaveGame) TArray<int32> FileCounts;
    UPROPERTY(SaveGame) int32 ObservedMoves = 0;
    UPROPERTY(SaveGame) int32 CaptureMoves = 0;
    UPROPERTY(SaveGame) int32 ForwardMoves = 0;
    UPROPERTY(SaveGame) int32 CheckMoves = 0;
    UPROPERTY(SaveGame) TMap<FString, int32> PositionMoveCounts;
    UPROPERTY(SaveGame) TMap<FString, int32> AICaution;
    UPROPERTY(SaveGame) TMap<FString, int32> HarmfulReplies;
    UPROPERTY(SaveGame) FString LastAIMoveKey;
    UPROPERTY(SaveGame) FString LastPlayerReplyKey;
    UPROPERTY(SaveGame) int32 LastExpectedScore = 0;
};

/**
 * 玩家游戏档案的存档类, 保存ID和偏好设置
 */
UCLASS()
class XIANGQIPRO_API USavePlayerProfile : public UXQPSaveData
{
	GENERATED_BODY()
public:
    UPROPERTY(SaveGame) FString PlayerName = TEXT("棋友");
    // 通用档案、模式进度和模式学习数据各自拥有明确的数据类型。
    UPROPERTY(SaveGame) TMap<EXQPGameMode, FXQPChallengeProgress> ModeProgress;
    UPROPERTY(SaveGame) TMap<EXQPGameMode, FXQPModeLearningData> ModeLearning;
    void MigrateLegacyData();

    // 只供v1存档迁移读取；正式逻辑不再访问这些扁平字段。
    UPROPERTY(SaveGame) int32 BestStreak = 0;
    UPROPERTY(SaveGame) int32 CurrentStreak = 0;
    UPROPERTY(SaveGame) bool bRunActive = false;
    UPROPERTY(SaveGame) uint8 RunDifficulty = 1;
    UPROPERTY(SaveGame) TArray<uint8> SavedBoard;
    UPROPERTY(SaveGame) uint8 SavedTurn = 0;
    UPROPERTY(SaveGame) int32 SavedRedMoveScore = 0;
    UPROPERTY(SaveGame) int32 SavedBlackMoveScore = 0;
    UPROPERTY(SaveGame) TArray<int32> PieceCounts;
    UPROPERTY(SaveGame) TArray<int32> FileCounts;
    UPROPERTY(SaveGame) int32 ObservedMoves = 0;
    UPROPERTY(SaveGame) int32 CaptureMoves = 0;
    UPROPERTY(SaveGame) int32 ForwardMoves = 0;
    UPROPERTY(SaveGame) int32 CheckMoves = 0;
    UPROPERTY(SaveGame) TMap<FString, int32> PositionMoveCounts;
    UPROPERTY(SaveGame) TMap<FString, int32> AICaution;
    UPROPERTY(SaveGame) TMap<FString, int32> HarmfulReplies;
    UPROPERTY(SaveGame) FString LastAIMoveKey;
    UPROPERTY(SaveGame) FString LastPlayerReplyKey;
    UPROPERTY(SaveGame) int32 LastExpectedScore = 0;
	
};
