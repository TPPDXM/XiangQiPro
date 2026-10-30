// Copyright 2026 Ultimate Player All Rights Reserved.
#include "UI_Main_Base.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Kismet/KismetSystemLibrary.h"

void UUI_Main_Base::Init(TObjectPtr<ACameraMainActor> InCamera)
{
    MainCamera = InCamera;
}

void UUI_Main_Base::NativeConstruct()
{
    Super::NativeConstruct();
    if (!GetClass()->GetPathName().StartsWith(TEXT("/Game/UMG/Mobile/"))) return;
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_StartGame")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::OpenClassic);
    if (auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_ProMode")))))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Main_Base::OpenPro);
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
void UUI_Main_Base::OpenClassic() { OpenMenu(TEXT("/Game/UMG/Main/BP_UI_ClassicGameMenu.BP_UI_ClassicGameMenu_C")); }
void UUI_Main_Base::OpenPro() { OpenMenu(TEXT("/Game/UMG/Main/BP_UI_ProModeMenu.BP_UI_ProModeMenu_C")); }
void UUI_Main_Base::OpenSettings() { OpenMenu(TEXT("/Game/UMG/Settings/SettingsMenu.SettingsMenu_C")); }
void UUI_Main_Base::ExitGame() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }
