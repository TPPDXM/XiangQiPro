// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_EndingGame.h"
#include "EndingGameData.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"

#include "XiangQiPro/Util/EndingLibrary.h"
#include "XiangQiPro/Util/Logger.h"

void UUI_Item_EndingGame::NativeConstruct()
{
	Super::NativeConstruct();
	ItemButton->OnClicked.AddDynamic(this, &UUI_Item_EndingGame::OnItemClicked);
}

void UUI_Item_EndingGame::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (UEndingGameData* Obj = Cast<UEndingGameData>(ListItemObject))
	{
		bIsInitItem = true;
        Index = Obj->Index;
		UserSelectedIndex = Obj->UserSelectedIndex;
        OnItemClickedDelegate = Obj->OnItemClickedDelegate;

        FString LevelText = FString::Printf(TEXT("第 %d 关"), Index + 1);
        FString Title = Obj->Title; // 获取关卡名称
        if (!Title.Equals(TEXT(""))) // 如果有标题则追加关卡名称
        {
            LevelText.Append(TEXT(" - "));
            LevelText.Append(Title);
        }
		IndexText->SetText(FText::FromString(LevelText));

        if (UserSelectedIndex)
        {
            if (*UserSelectedIndex == Index)
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

void UUI_Item_EndingGame::NativeOnEntryReleased()
{
    IUserObjectListEntry::NativeOnEntryReleased();
    SetSelected(false);
}

void UUI_Item_EndingGame::OnItemClicked()
{
    OnItemClickedDelegate.Broadcast(this);
}

void UUI_Item_EndingGame::SetSelected(bool bIsSelected)
{
    bSelected = bIsSelected;

    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Type::RoundedBox; // 绘制类型
    brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::Type::FixedRadius; // 轮廓圆角
    brush.OutlineSettings.CornerRadii = FVector4(20, 20, 20, 20); // 圆角角度
    brush.OutlineSettings.Width = 5.f; // 轮廓宽度
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
