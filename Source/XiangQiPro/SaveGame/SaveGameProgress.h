// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameProgress.generated.h"

/**
 * 玩家游戏进度的存档类
 */
UCLASS()
class XIANGQIPRO_API USaveGameProgress : public USaveGame
{
	GENERATED_BODY()

public:

	USaveGameProgress();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EndingGameLevel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EndingGameLevel_Max;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> UsingSteps;

    // Stable DataTable row names corresponding to the saved positional fields.
    UPROPERTY(SaveGame)
    TArray<FName> EndingLevelOrder;
	
};
