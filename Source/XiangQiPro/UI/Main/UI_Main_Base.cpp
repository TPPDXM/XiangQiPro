// Copyright 2026 Ultimate Player All Rights Reserved.
#include "UI_Main_Base.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"

void UUI_Main_Base::Init(TObjectPtr<ACameraMainActor> InCamera)
{
    MainCamera = InCamera;
}

void UUI_Main_Base::NativeConstruct()
{
    Super::NativeConstruct();
    if (WelcomeButton)
    {
        WelcomeButton->SetVisibility(ESlateVisibility::Collapsed);
        WelcomeButton->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::EditPlayerName);
        UpdateCanTick();
        if (WelcomeText) WelcomeText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        RefreshWelcome();
    }
    if (!GetClass()->GetPathName().StartsWith(PATH_UI_MOBILE_ROOT)) return;
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_StartGame")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::OpenClassic);
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_GameModes")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::OpenGameModes);
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_Settings")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::OpenSettings);
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_Exit")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::ExitGame);
}

void UUI_Main_Base::OpenMenu(const TCHAR* ClassPath)
{
    if (UClass* Class = LoadClass<UUserWidget>(nullptr, ClassPath))
        if (UUserWidget* Widget = XQPMobile::CreateWidget<UUserWidget>(GetOwningPlayer(), Class))
            GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Widget);
}
void UUI_Main_Base::OpenClassic() { OpenMenu(PATH_UI_CLASSIC_MENU); }
void UUI_Main_Base::OpenGameModes() { OpenMenu(PATH_UI_GAMEMODE_MENU); }
void UUI_Main_Base::OpenSettings() { OpenMenu(PATH_UI_SETTINGS_MENU); }
void UUI_Main_Base::ExitGame() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }

// 标题及祖先容器的有效透明度包含点击开始后的蓝图入场动画。
void UUI_Main_Base::RefreshWelcome()
{
    if (!WelcomeButton || !WelcomeText) return;
    const UWidget* Title = GetWidgetFromName(TEXT("TextBlock_121"));
    float Alpha = Cast<UTextBlock>(Title) ? Cast<UTextBlock>(Title)->GetColorAndOpacity().GetSpecifiedColor().A : 1.f;
    for (const UWidget* Node = Title; Node && Node != this; Node = Node->GetParent())
    {
        if (Node->GetVisibility() == ESlateVisibility::Collapsed || Node->GetVisibility() == ESlateVisibility::Hidden) Alpha = 0.f;
        Alpha *= Node->GetRenderOpacity();
    }
    const bool Visible = Title && Alpha > .01f && !XQPMobile::IsPortrait(this);
    WelcomeButton->SetVisibility(Visible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    WelcomeButton->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
    const auto* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
    if (Profile && DisplayedPlayerName != Profile->GetPlayerName())
    {
        DisplayedPlayerName = Profile->GetPlayerName();
        WelcomeText->SetText(FText::Format(NSLOCTEXT("MainMenu", "WelcomePlayer", "欢迎 {0}"), FText::FromString(DisplayedPlayerName)));
    }
}

void UUI_Main_Base::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshWelcome();
}

// 名称按钮直达同一份玩家资料设置，返回主菜单后自动回显新名称。
void UUI_Main_Base::EditPlayerName()
{
    OpenMenu(PATH_UI_PLAYER_SETTINGS);
}
