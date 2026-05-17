// Copyright 2026 Ultimate Player All Rights Reserved.
// 提供了Pro模式的元数据

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProModeLibrary.generated.h"

/*
* Pro模式元数据结构体
*/
USTRUCT(BlueprintType)
struct FProModeInfo : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Title = FString();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Covers = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int GameModeID = 3;
};

/**
 * 提供Pro模式的元数据获取
 */
UCLASS()
class XIANGQIPRO_API UProModeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static auto GetProModeInfos() -> TArray<FProModeInfo*>;
	
};
