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

	TWeakObjectPtr<class UUI_Introduce_Ending> SelectionOwner;

	FOnEndingGameListItemClicked OnItemClickedDelegate;

	FString Title;
	// 保存关卡数据和选择来源的弱引用。
	void Init(int32 InIndex, UUI_Introduce_Ending* InSelectionOwner, FOnEndingGameListItemClicked CallBackFunc);

};
