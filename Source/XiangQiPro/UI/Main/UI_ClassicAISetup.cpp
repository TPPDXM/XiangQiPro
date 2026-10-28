#include "UI_ClassicAISetup.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"

void UUI_ClassicAISetup::NativeConstruct()
{
    Super::NativeConstruct();
    UpdateCanTick();
    if (!ChallengeSwitch || !HighestStreakText)
        return;
    // Designer中的横排与颜色不在运行时修改，只连接已保存的控件。
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    ChallengeSwitch->OnCheckStateChanged.AddUniqueDynamic(this, &UUI_ClassicAISetup::OnChallengeChanged);
    ApplyChallengeSelection(Profile->HasActiveRun());
    SwitchPosition = SwitchTarget;
    if (SwitchThumb) SwitchThumb->SetRenderTranslation(FVector2D(30.f * SwitchPosition, 0.f));
}

void UUI_ClassicAISetup::OnChallengeChanged(bool bChecked)
{
    UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    if (!bChecked && Profile->HasActiveRun())
    {
        ChallengeSwitch->SetIsChecked(true);
        if (ChallengeDialog.IsValid() && ChallengeDialog->IsInViewport()) return;
        if (!CommonDialogClass) return;
        auto* Dialog = CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
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
    if (!SwitchThumb || FMath::IsNearlyEqual(SwitchPosition, SwitchTarget)) return;
    SwitchElapsed = FMath::Min(SwitchElapsed + DeltaTime, 0.18f);
    const float T = SwitchElapsed / 0.18f;
    SwitchPosition = FMath::Lerp(SwitchStart, SwitchTarget, T * T * (3.f - 2.f * T));
    SwitchThumb->SetRenderTranslation(FVector2D(30.f * SwitchPosition, 0.f));
}
