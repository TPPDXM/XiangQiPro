// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_GameMode.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "GameModeData.h"
#include "XiangQiPro/Util/Logger.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI_GameModeSetup.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/Button.h"

void UUI_Item_GameMode::NativeConstruct()
{
	Super::NativeConstruct();
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ItemBtn"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Item_GameMode::OpenModeIntroduction);
    }
}

void UUI_Item_GameMode::OpenModeIntroduction()
{
    if (!SetupClass) return;
    auto* Setup = XQPMobile::CreateWidget<UUI_GameModeSetup>(GetOwningPlayer(), SetupClass);
    if (!Setup) return;
    Setup->SetMode(GameModeID);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Setup, false);
}

void UUI_Item_GameMode::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	UGameModeData* ModeData = Cast<UGameModeData>(ListItemObject);		// 读取通用玩法条目数据

	if (!ModeData)
	{
		ULogger::LogError(TEXT("UUI_Item_GameMode::NativeOnListItemObjectSet"), TEXT("Cast failed! ModeData is nullptr!"));
		return;
	}

    SetModeInfo(ModeData->GetModeTitle(), ModeData->GetModeCover(), ModeData->GetGameModeID(), ModeData->GetSetupClass());
}

void UUI_Item_GameMode::SetModeInfo(const FString& Title, UTexture2D* Cover, int32 ModeId, TSubclassOf<UUI_GameModeSetup> Adapter)
{
    if (Adapter) SetupClass = Adapter;
    if (ItemText) ItemText->SetText(FText::FromString(Title));
    if (ItemCover) ItemCover->SetBrush(UWidgetBlueprintLibrary::MakeBrushFromTexture(Cover, 256.f, 192.f));
    GameModeID = ModeId;
}
