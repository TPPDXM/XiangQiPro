// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SavePlayerProfile.generated.h"

/**
 * 玩家游戏档案的存档类, 保存ID和偏好设置
 */
UCLASS()
class XIANGQIPRO_API USavePlayerProfile : public USaveGame
{
	GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) int64 Generation = 0;
    UPROPERTY(SaveGame) FString PlayerName = TEXT("棋友");
    UPROPERTY(SaveGame) int32 BestStreak = 0;
    UPROPERTY(SaveGame) int32 CurrentStreak = 0;
    UPROPERTY(SaveGame) bool bRunActive = false;
    UPROPERTY(SaveGame) uint8 RunDifficulty = 1;
    UPROPERTY(SaveGame) TArray<uint8> SavedBoard;
    UPROPERTY(SaveGame) uint8 SavedTurn = 0;
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
