#include "UI_ClassicAISetup.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"

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
    ChallengeSwitch->OnCheckStateChanged.AddUniqueDynamic(this, &UUI_ClassicAISetup::OnChallengeChanged);
    ApplyChallengeSelection(Profile->HasActiveRun());
    SwitchPosition = SwitchTarget;
    UpdateSwitchThumb();
}

void UUI_ClassicAISetup::OnChallengeChanged(bool bChecked)
{
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    const bool bJustEnabled = bChecked && !Profile->bChallengeSelected;
    if (!bChecked && Profile->HasActiveRun())
    {
        ChallengeSwitch->SetIsChecked(true);
        if (ChallengeDialog.IsValid() && ChallengeDialog->IsInViewport()) return;
        if (!CommonDialogClass) return;
        auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
        if (!Dialog) return;
        ChallengeDialog = Dialog;
        GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
        Dialog->Configure(FText::FromString(TEXT("结束连战挑战？")),
            FText::FromString(FString::Printf(TEXT("当前连胜 %d 局。结束后将结算本次挑战并保留最高纪录，当前对局进度将清除。"), Profile->GetCurrentStreak())),
            FText::FromString(TEXT("结束并结算")), FText::FromString(TEXT("继续挑战")));
        Dialog->OnPrimaryAction = FSimpleDelegate::CreateWeakLambda(this, [this]()
        {
            GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->EndChallenge();
            ApplyChallengeSelection(false);
            if (ChallengeDialog.IsValid()) ChallengeDialog->Dismiss();
        });
        return;
    }
    ApplyChallengeSelection(bChecked);
    if (bJustEnabled && CommonDialogClass)
    {
        // 仅在主动开启时说明学习与续局规则，恢复已有挑战不会重复打断玩家。
        auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
        if (!Dialog) return;
        ChallengeDialog = Dialog;
        GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
        Dialog->Configure(FText::FromString(TEXT("AI 连战挑战")),
            FText::FromString(TEXT("AI 会在连战中学习你的用子习惯、进攻路线和应对方式，调整策略，并随着连胜逐步增强。\n\n对局进度与执棋回合会在每次落子时自动保存。\n结束挑战后结算最高连胜，本次学习记录将清除。")),
            FText::FromString(TEXT("知道了")), FText::GetEmpty());
        Dialog->OnPrimaryAction = FSimpleDelegate::CreateWeakLambda(Dialog, [Dialog]() { Dialog->Dismiss(); });
    }
}

void UUI_ClassicAISetup::ApplyChallengeSelection(bool bChecked)
{
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    Profile->bChallengeSelected = bChecked;
    ChallengeSwitch->SetIsChecked(bChecked);
    SwitchStart = SwitchPosition;
    SwitchTarget = bChecked ? 1.f : 0.f;
    SwitchElapsed = 0.f;
    // 有未完成的连战时锁定起始难度，关闭需先确认结算。
    if (UWidget* Difficulty = GetWidgetFromName(TEXT("WB_ComboBox_Difficult")))
        Difficulty->SetIsEnabled(!(bChecked && Profile->HasActiveRun()));
    // 连战区域只呈现最高连胜，不拼接当前连胜、玩法说明等额外文案。
    HighestStreakText->SetText(FText::FromString(FString::Printf(TEXT("最高连胜 %d 局"), Profile->GetBestStreak())));
    HighestStreakText->SetVisibility(bChecked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UUI_ClassicAISetup::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);
    if (!FMath::IsNearlyEqual(SwitchPosition, SwitchTarget))
    {
        SwitchElapsed = FMath::Min(SwitchElapsed + DeltaTime, 0.18f);
        const float T = SwitchElapsed / 0.18f;
        SwitchPosition = FMath::Lerp(SwitchStart, SwitchTarget, T * T * (3.f - 2.f * T));
    }
    UpdateSwitchThumb();
}

void UUI_ClassicAISetup::UpdateSwitchThumb()
{
    if (!SwitchThumb) return;
    const UWidget* Track = GetWidgetFromName(TEXT("SwitchTrack"));
    const UWidget* ThumbFrame = SwitchThumb->GetParent();
    if (!Track || !ThumbFrame) return;
    float TrackWidth = Track->GetCachedGeometry().GetLocalSize().X;
    float ThumbWidth = ThumbFrame->GetCachedGeometry().GetLocalSize().X;
    if (TrackWidth <= 0.f)
        if (const auto* Box = Cast<USizeBox>(Track)) TrackWidth = Box->GetWidthOverride();
    if (ThumbWidth <= 0.f)
        if (const auto* Box = Cast<USizeBox>(ThumbFrame)) ThumbWidth = Box->GetWidthOverride();
    const float Travel = FMath::Max(0.f, TrackWidth - ThumbWidth);
    SwitchThumb->SetRenderTranslation(FVector2D(Travel * FMath::Clamp(SwitchPosition, 0.f, 1.f), 0.f));
}
