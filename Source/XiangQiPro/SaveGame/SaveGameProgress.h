// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SaveStorage.h"
#include "SaveGameProgress.generated.h"

USTRUCT()
struct FXQPLevelProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 CurrentLevel = 0;
    UPROPERTY(SaveGame) int32 MaxUnlockedLevel = 0;
    UPROPERTY(SaveGame) TArray<int32> BestSteps;
    UPROPERTY(SaveGame) TArray<FName> LevelOrder;
};

/**
 * 玩家游戏进度的存档类
 */
UCLASS()
class XIANGQIPRO_API USaveGameProgress : public UXQPSaveData
{
	GENERATED_BODY()

public:

	USaveGameProgress();

    UPROPERTY(SaveGame) FXQPLevelProgress Progress;
    void MigrateLegacyData();

    // 保留旧属性名读取历史存档；新进度类型不再绑定残局玩法。

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
