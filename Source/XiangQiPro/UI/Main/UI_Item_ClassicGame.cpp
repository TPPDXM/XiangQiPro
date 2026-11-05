// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_ClassicGame.h"
#include "XiangQiPro/Audio/XQPFeedbackAudioSubsystem.h"
#include "ClassicGameData.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "XiangQiPro/Util/Logger.h"
#include <Blueprint/WidgetBlueprintLibrary.h>

void UUI_Item_ClassicGame::NativeConstruct()
{
    Super::NativeConstruct();
    // ListView entries configure feedback whenever the recycled widget is constructed.
    UXQPFeedbackAudioSubsystem::ConfigureFor(this);

    if (ItemButton)
    {
        ItemButton->OnClicked.AddDynamic(this, &UUI_Item_ClassicGame::OnItemClicked);
    }
}

void UUI_Item_ClassicGame::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
    if (bIsInitItem)
    {
        return;
    }

    if (UClassicGameData* Obj = Cast<UClassicGameData>(ListItemObject))
    {
        SetIndex(Obj->Index);
        ItemText->SetText(Obj->ItemText);
        OnItemClickedDelegate = Obj->OnItemClickedDelegate;
        bIsInitItem = true;

        FSlateBrush brush = UWidgetBlueprintLibrary::MakeBrushFromTexture(Obj->Icon, 96, 96); // 创建笔刷
        Icon->SetBrush(brush);
    }
}

void UUI_Item_ClassicGame::SetSelected(bool bIsSelected)
{
    bSelected = bIsSelected;

    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Type::RoundedBox; // 绘制类型
    brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::Type::FixedRadius; // 轮廓圆角
    // 移动端动态选中框也遵循全局描边加粗规则。
    brush.OutlineSettings.Width = XQPMobile::IsPortrait(this) ? 10.f : 5.f; // 轮廓宽度
    if (bSelected)
    {
        brush.OutlineSettings.Color = SelectedTextColor; // 轮廓颜色
        SelectedIndicator->SetColorAndOpacity(SelectedColor);
        SelectedIndicator->SetBrush(brush);
        ItemText->SetColorAndOpacity(SelectedTextColor);
    }
    else
    {
        brush.OutlineSettings.Color = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f); // 轮廓颜色
        SelectedIndicator->SetColorAndOpacity(DefaultColor);
        SelectedIndicator->SetBrush(brush);
        ItemText->SetColorAndOpacity(DefaultTextColor);
    }
}

void UUI_Item_ClassicGame::OnItemClicked()
{
    OnItemClickedDelegate.Broadcast(this);
}

