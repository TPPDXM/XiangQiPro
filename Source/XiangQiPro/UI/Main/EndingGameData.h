// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UI_Item_EndingGame.h"
#include "EndingGameData.generated.h"

/**
 * 残局关卡条目数据对象
 */
UCLASS()
class XIANGQIPRO_API UEndingGameData : public UObject
{
	GENERATED_BODY()

public:

	int32 Index = 0;

	int32* UserSelectedIndex;

	FOnEndingGameListItemClicked OnItemClickedDelegate;

	FString Title;

	void Init(int32 InIndex, int32* InUserSelectedIndex, FOnEndingGameListItemClicked CallBackFunc);
	
};
