// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_ProModeMenu.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI_Item_ProMode.h"
#include "XiangQiPro/UI/XQPMobile.h"

#include "XiangQiPro/Util/ProModeLibrary.h"
#include "ProModeData.h"

// 构造界面并初始化控件与事件绑定。
void UUI_ProModeMenu::NativeConstruct()
{
	Super::NativeConstruct();
	auto Arr = UProModeLibrary::GetProModeInfos();
    if (ModeOptions && ModeItemClass)
    {
        ModeOptions->ClearChildren();
        for (int32 Index = 0; Index < Arr.Num(); ++Index)
        {
            auto* Item = XQPMobile::CreateWidget<UUI_Item_ProMode>(GetOwningPlayer(), ModeItemClass);
            if (!Item) continue;
            auto* EntrySlot = ModeOptions->AddChildToVerticalBox(Item);
            EntrySlot->SetHorizontalAlignment(XQPMobile::IsPortrait(this) ? HAlign_Fill : HAlign_Center);
            EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, Index + 1 < Arr.Num() ? 14.f : 0.f));
            Item->SetModeInfo(Arr[Index].Title, Arr[Index].Covers, Arr[Index].GameModeID);
        }
        return;
    }
	if (!IsValid(ModeList)) return;
	ModeList->ClearListItems();
	for (const auto& info : Arr)
	{
		UProModeData* item = NewObject<UProModeData>(this);
		if (item)
		{
			item->Init(info.Title, info.Covers, info.GameModeID);
			ModeList->AddItem(item);
		}
	}
}
