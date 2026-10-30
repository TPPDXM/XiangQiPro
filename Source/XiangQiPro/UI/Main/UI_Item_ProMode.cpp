// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_ProMode.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "ProModeData.h"
#include "XiangQiPro/Util/Logger.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI_ProModeSetup.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/Button.h"

void UUI_Item_ProMode::NativeConstruct()
{
	Super::NativeConstruct();
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ItemBtn"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Item_ProMode::OpenModeIntroduction);
    }
}

void UUI_Item_ProMode::OpenModeIntroduction()
{
    if (!SetupClass) return;
    auto* Setup = XQPMobile::CreateWidget<UUI_ProModeSetup>(GetOwningPlayer(), SetupClass);
    if (!Setup) return;
    Setup->SetMode(GameModeID);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Setup, false);
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

    SetModeInfo(ModeData->GetModeTitle(), ModeData->GetModeCover(), ModeData->GetGameModeID());
}

void UUI_Item_ProMode::SetModeInfo(const FString& Title, UTexture2D* Cover, int32 ModeId)
{
    if (ItemText) ItemText->SetText(FText::FromString(Title));
    if (ItemCover) ItemCover->SetBrush(UWidgetBlueprintLibrary::MakeBrushFromTexture(Cover, 256.f, 192.f));
    GameModeID = ModeId;
}
