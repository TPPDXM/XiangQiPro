#include "UI_PlayerSettings.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"

void UUI_PlayerSettings::NativeConstruct()
{
    Super::NativeConstruct();
    if (!NameInput)
        return;
    // 设置页读取同一份本地资料，联机握手直接使用这里保存的名称。
    NameInput->SetText(FText::FromString(GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->GetPlayerName()));
    NameInput->OnTextCommitted.AddUniqueDynamic(this, &UUI_PlayerSettings::OnNameCommitted);
    XQPButtonBinding::GetButton(SaveNameButton)->OnClicked.AddUniqueDynamic(this, &UUI_PlayerSettings::SaveName);
    XQPButtonBinding::GetButton(BackButton)->OnClicked.AddUniqueDynamic(this, &UUI_PlayerSettings::GoBack);
    SaveStatusText->SetText(FText::GetEmpty());
}

void UUI_PlayerSettings::SaveName()
{
    auto* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    // 验证和持久化交给资料子系统，控件中回显清理后的实际保存值。
    const bool Saved = Profile->SetPlayerName(NameInput->GetText().ToString());
    NameInput->SetText(FText::FromString(Profile->GetPlayerName()));
    SaveStatusText->SetText(FText::FromString(Saved ? TEXT("已保存") : TEXT("名称不能为空，或本地保存失败")));
}

void UUI_PlayerSettings::OnNameCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (Method == ETextCommit::OnEnter)
        SaveName();
}

void UUI_PlayerSettings::GoBack()
{
    GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this);
}
