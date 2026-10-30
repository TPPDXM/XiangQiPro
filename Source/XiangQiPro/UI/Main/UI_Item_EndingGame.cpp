// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_EndingGame.h"
#include "EndingGameData.h"
#include "UI_Introduce_Ending.h"
#include "XiangQiPro/UI/XQPMobile.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"

#include "XiangQiPro/Util/EndingLibrary.h"
#include "XiangQiPro/Util/Logger.h"

// 构造界面并初始化控件与事件绑定。
void UUI_Item_EndingGame::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(ItemButton)) ItemButton->OnClicked.AddUniqueDynamic(this, &UUI_Item_EndingGame::OnItemClicked);
}

// 接收列表数据并刷新复用条目的显示。
void UUI_Item_EndingGame::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (UEndingGameData* Obj = Cast<UEndingGameData>(ListItemObject))
	{
		bIsInitItem = true;
        Index = Obj->Index;
		SelectionOwner = Obj->SelectionOwner;
        OnItemClickedDelegate = Obj->OnItemClickedDelegate;

        FString LevelText = FString::Printf(TEXT("第 %d 关"), Index + 1);
        FString Title = Obj->Title; // 获取关卡名称
        if (!Title.Equals(TEXT(""))) // 如果有标题则追加关卡名称
        {
            LevelText.Append(TEXT(" - "));
            LevelText.Append(Title);
        }
		if (IsValid(IndexText)) IndexText->SetText(FText::FromString(LevelText));
        SetSelected(false);

        if (SelectionOwner.IsValid())
        {
            if (SelectionOwner->GetSelectedIndex() == Index)
            {
                OnItemClicked();
            }
        }
        else
        {
            ULogger::LogError(TEXT("UUI_Item_EndingGame::NativeOnListItemObjectSet"), TEXT("UserSelectedIndex is nullptr!"));
        }
	}
}

// 列表条目被回收时清理状态和引用。
void UUI_Item_EndingGame::NativeOnEntryReleased()
{
    IUserObjectListEntry::NativeOnEntryReleased();
    SetSelected(false);
    SelectionOwner.Reset();
    OnItemClickedDelegate.Clear();
}

// 广播列表条目的点击事件。
void UUI_Item_EndingGame::OnItemClicked()
{
    OnItemClickedDelegate.Broadcast(this);
}

// 更新条目选中颜色和轮廓。
void UUI_Item_EndingGame::SetSelected(bool bIsSelected)
{
    bSelected = bIsSelected;
    if (!IsValid(SelectedIndicator) || !IsValid(IndexText)) return;

    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Type::RoundedBox; // 绘制类型
    brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::Type::FixedRadius; // 轮廓圆角
    brush.OutlineSettings.CornerRadii = FVector4(20, 20, 20, 20); // 圆角角度
    brush.OutlineSettings.Width = 5.f; // 轮廓宽度
    if (XQPMobile::IsPortrait(this))
    {
        brush.OutlineSettings.CornerRadii = FVector4(6, 6, 6, 6);
        brush.OutlineSettings.Width = .8f;
        brush.OutlineSettings.Color = bSelected ? FLinearColor(.69f, .53f, .28f, 1.f) : FLinearColor(.35f, .29f, .23f, .65f);
        brush.TintColor = bSelected ? FLinearColor(.075f, .035f, .03f, .9f) : FLinearColor(.025f, .018f, .02f, .35f);
        SelectedIndicator->SetColorAndOpacity(FLinearColor::White);
        SelectedIndicator->SetBrush(brush);
        IndexText->SetColorAndOpacity(bSelected ? FLinearColor(.82f, .67f, .42f, 1.f) : FLinearColor(.72f, .67f, .59f, 1.f));
        return;
    }
    if (bSelected)
    {
        brush.OutlineSettings.Color = SelectedTextColor; // 轮廓颜色
        SelectedIndicator->SetColorAndOpacity(SelectedColor);
        SelectedIndicator->SetBrush(brush);
        IndexText->SetColorAndOpacity(SelectedTextColor);
    }
    else
    {
        brush.OutlineSettings.Color = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f); // 轮廓颜色
        SelectedIndicator->SetColorAndOpacity(DefaultColor);
        SelectedIndicator->SetBrush(brush);
        IndexText->SetColorAndOpacity(DefaultTextColor);
    }
}
