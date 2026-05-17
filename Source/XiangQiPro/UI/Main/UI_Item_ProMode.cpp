// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_ProMode.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "ProModeData.h"
#include "XiangQiPro/Util/Logger.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UUI_Item_ProMode::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUI_Item_ProMode::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	UProModeData* ModeData = Cast<UProModeData>(ListItemObject);		// 获取Pro模式Item的数据

	if (!ModeData)
	{
		ULogger::LogError(TEXT("UUI_Item_ProMode::NativeOnListItemObjectSet"), TEXT("Cast failed! ModeData is nullptr!"));
		return;
	}

	ItemText->SetText(FText::FromString(ModeData->GetModeTitle()));										    // 设置Item标题
	ItemCover->SetBrush(UWidgetBlueprintLibrary::MakeBrushFromTexture(ModeData->GetModeCover(), 256.0, 192.0));  // 设置Item封面
	GameModeID = ModeData->GetGameModeID();																	// 获取游戏模式ID
}