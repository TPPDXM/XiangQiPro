// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "XiangQiPro/GameMode/XQPGameTypes.h"
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

    static USaveGameProgress* LoadProgress(EXQPGameMode Mode, int32 UserIndex = 0);
    static bool SaveProgress(USaveGameProgress* Progress, EXQPGameMode Mode, int32 UserIndex = 0);

    static int32 GetEndingGameLevel();

    static int32 GetEndingGameLevel_Max();

    static void SetEndingGameLevel(int32);

    static void SetEndingGameLevel_Max(int32);

    static void UpdateEndingGameLevelData();

    // 0 means this level has no completed run yet. Steps count red moves only.
    static int32 GetEndingBestSteps(int32 LevelIndex);
    static bool SaveEndingBestSteps(int32 LevelIndex, int32 Steps);
    static bool CompleteEndingLevel(int32 LevelIndex, int32 Steps, int32& OutBestSteps, bool& bOutRecordBroken);
	
};
