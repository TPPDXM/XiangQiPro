// Copyright 2026 Ultimate Player All Rights Reserved.
// 提供扩展玩法的元数据

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameModeLibrary.generated.h"

/*
* 扩展玩法元数据结构体
*/
USTRUCT(BlueprintType)
struct FGameModeInfo : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Title = FString();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Covers = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int GameModeID = 3;

    // 列表只消费适配窗口类型，后续玩法可以在数据表中提供自己的设置页。
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftClassPtr<class UUI_GameModeSetup> SetupWidgetClass;
};

/**
 * 提供扩展玩法的元数据获取
 */
UCLASS()
class XIANGQIPRO_API UGameModeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// 获取模式数据的独立副本。
	static TArray<FGameModeInfo> GetGameModeInfos();

};
