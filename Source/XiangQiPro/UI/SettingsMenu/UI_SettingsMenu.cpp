#include "UI_SettingsMenu.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"

void UUI_SettingsMenu::NativeConstruct()
{
    Super::NativeConstruct();
    if (XQPMobile::IsPortrait(this))
        if (auto* Video = GetWidgetFromName(TEXT("WB_Btn_VideoSettings"))) Video->SetVisibility(ESlateVisibility::Collapsed);
    // 原有视频、画面和返回按钮的蓝图接线保持有效，只连接新增的玩家设置入口。
    if (PlayerSettingsButton)
        XQPButtonBinding::GetButton(PlayerSettingsButton)->OnClicked.AddUniqueDynamic(this, &UUI_SettingsMenu::OpenPlayerSettings);
    // 复用设置菜单自身的按钮样式；两端仅布局不同，共用 UIManager 的压栈和返回行为。
    if (UButton* Button = XQPButtonBinding::GetButton(WB_Btn_AudioSettings))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_SettingsMenu::OpenAudioSettings);
}

void UUI_SettingsMenu::OpenAudioSettings()
{
    if (!AudioSettingsClass || !GetGameInstance()) return;
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(
        XQPMobile::CreateWidget<UUserWidget>(GetGameInstance(), AudioSettingsClass));
}

void UUI_SettingsMenu::OpenPlayerSettings()
{
    if (!PlayerSettingsClass)
        return;
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(
        XQPMobile::CreateWidget<UUserWidget>(GetGameInstance(), PlayerSettingsClass));
}
