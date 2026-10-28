#include "UI_SettingsMenu.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"

void UUI_SettingsMenu::NativeConstruct()
{
    Super::NativeConstruct();
    // 原有视频、画面和返回按钮的蓝图接线保持有效，只连接新增的玩家设置入口。
    if (PlayerSettingsButton)
        XQPButtonBinding::GetButton(PlayerSettingsButton)->OnClicked.AddUniqueDynamic(this, &UUI_SettingsMenu::OpenPlayerSettings);
}

void UUI_SettingsMenu::OpenPlayerSettings()
{
    if (!PlayerSettingsClass)
        return;
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(
        CreateWidget<UUserWidget>(GetGameInstance(), PlayerSettingsClass));
}
