#include "UI_ClassicAISetup.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/UI/Components/UI_ToggleSwitch.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"

namespace
{
    void ConfigureChallengeRules(UUI_CommonDialog* Dialog)
    {
        Dialog->Configure(FText::FromString(TEXT("AI 连战挑战")),
            FText::FromString(TEXT("AI 会在连战中学习你的用子习惯、进攻路线和应对方式，调整策略，并随着连胜逐步增强。\n\n对局进度与执棋回合会在每次落子时自动保存。\n结束挑战后结算最高连胜，本次学习记录将清除。")),
            FText::FromString(TEXT("知道了")), FText::GetEmpty());
    }
}

void UUI_ClassicAISetup::NativeConstruct()
{
    Super::NativeConstruct();
    // 先让旧蓝图完成绑定，再统一替换选项；保存旧值，避免ClearOptions的临时空选择污染难度。
    if (auto* Selector = Cast<UUI_XQP_ComboBox>(GetWidgetFromName(TEXT("WB_ComboBox_Difficult"))); Selector && Selector->ComboBox)
    {
        auto* GI = GetGameInstance<UXQPGameInstance>();
        const int32 Selected = GI ? FMath::Clamp<int32>(GI->AIDifficulty, 0, 4) : 2;
        Selector->ComboBox->ClearOptions();
        for (const TCHAR* Name : {TEXT("简单"), TEXT("普通"), TEXT("困难"), TEXT("大师"), TEXT("棋圣")}) Selector->ComboBox->AddOption(Name);
        Selector->ComboBox->SetSelectedIndex(Selected);
        if (GI) GI->AIDifficulty = Selected;
    }
    UpdateCanTick();
    if (!ChallengeSwitch || !HighestStreakText)
        return;
    // Designer中的横排与颜色不在运行时修改，只连接已保存的控件。
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    ChallengeSwitch->OnChanged.AddUniqueDynamic(this, &UUI_ClassicAISetup::OnChallengeChanged);
    ApplyChallengeSelection(Profile->HasActiveRun());
    ChallengeSwitch->SetIsChecked(Profile->HasActiveRun(), false);
    // 页面进入时构建Slate控件，主动开关时复用，避免创建弹窗抢占首帧。
    if (XQPMobile::IsPortrait(this))
        if (auto* Dialog = GetChallengeDialog())
        {
            ConfigureChallengeRules(Dialog);
            PreparedChallengeSlate = Dialog->TakeWidget();
            Dialog->ForceLayoutPrepass();
        }
}

void UUI_ClassicAISetup::NativeDestruct()
{
    if (ChallengeSwitch) ChallengeSwitch->OnChanged.RemoveDynamic(this, &UUI_ClassicAISetup::OnChallengeChanged);
    PreparedChallengeSlate.Reset();
    Super::NativeDestruct();
}

UUI_CommonDialog* UUI_ClassicAISetup::GetChallengeDialog()
{
    if (!ChallengeDialog && CommonDialogClass)
        ChallengeDialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
    return ChallengeDialog;
}

void UUI_ClassicAISetup::OnChallengeChanged(bool bChecked)
{
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    const bool bJustEnabled = bChecked && !Profile->bChallengeSelected;
    if (!bChecked && Profile->HasActiveRun())
    {
        ChallengeSwitch->SetIsChecked(true, true);
        if (ChallengeDialog && ChallengeDialog->IsInViewport()) return;
        if (!CommonDialogClass) return;
        auto* Dialog = GetChallengeDialog();
        if (!Dialog) return;
        Dialog->Configure(FText::FromString(TEXT("结束连战挑战？")),
            FText::FromString(FString::Printf(TEXT("当前连胜 %d 局。结束后将结算本次挑战并保留最高纪录，当前对局进度将清除。"), Profile->GetCurrentStreak())),
            FText::FromString(TEXT("结束并结算")), FText::FromString(TEXT("继续挑战")));
        GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
        Dialog->OnPrimaryAction = FSimpleDelegate::CreateWeakLambda(this, [this]()
        {
            GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->EndChallenge();
            ApplyChallengeSelection(false);
            if (ChallengeDialog) ChallengeDialog->Dismiss();
        });
        return;
    }
    ApplyChallengeSelection(bChecked);
    if (bJustEnabled && CommonDialogClass)
    {
        // 仅在主动开启时说明学习与续局规则，恢复已有挑战不会重复打断玩家。
        auto* Dialog = GetChallengeDialog();
        if (!Dialog) return;
        ConfigureChallengeRules(Dialog);
        GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
        Dialog->OnPrimaryAction = FSimpleDelegate::CreateWeakLambda(Dialog, [Dialog]() { Dialog->Dismiss(); });
    }
}

void UUI_ClassicAISetup::ApplyChallengeSelection(bool bChecked)
{
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    Profile->bChallengeSelected = bChecked;
    ChallengeSwitch->SetIsChecked(bChecked, true);
    // 有未完成的连战时锁定起始难度，关闭需先确认结算。
    if (UWidget* Difficulty = GetWidgetFromName(TEXT("WB_ComboBox_Difficult")))
        Difficulty->SetIsEnabled(!(bChecked && Profile->HasActiveRun()));
    // 连战区域只呈现最高连胜，不拼接当前连胜、玩法说明等额外文案。
    HighestStreakText->SetText(FText::FromString(FString::Printf(TEXT("最高连胜 %d 局"), Profile->GetBestStreak())));
    HighestStreakText->SetVisibility(bChecked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
