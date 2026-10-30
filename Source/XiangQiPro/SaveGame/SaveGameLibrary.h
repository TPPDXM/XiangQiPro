// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SaveGameLibrary.generated.h"

class USaveGameProgress;

/**
 * 玩家数据处理相关函数库, 包含对存档的读取和保存
 */
UCLASS()
class XIANGQIPRO_API USaveGameLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    static USaveGameProgress* GetSaveGameProgress(const FString&, int32);

    static bool ApplySaveGameProgress(USaveGameProgress*, const FString&, int32);

    static int32 GetEndingGameLevel();

    static int32 GetEndingGameLevel_Max();

    static void SetEndingGameLevel(int32);

    static void SetEndingGameLevel_Max(int32);

    static void UpdateEndingGameLevelData();

    // 0 means this level has no completed run yet. Steps count red moves only.
    static int32 GetEndingBestSteps(int32 LevelIndex);
    static bool SaveEndingBestSteps(int32 LevelIndex, int32 Steps);
	
};
