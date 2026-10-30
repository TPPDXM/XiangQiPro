#include "UI_CommonDialog.h"
#include "XQPButtonBinding.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPUIStyle.h"
#include "Components/Border.h"

void UUI_CommonDialog::NativeConstruct()
{
    Super::NativeConstruct();
    CacheDefaultAppearance();
    ReplayEntrance();
    if (auto* Button = XQPButtonBinding::GetButton(PrimaryButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_CommonDialog::PrimaryClicked);
    if (auto* Button = XQPButtonBinding::GetButton(SecondaryButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_CommonDialog::SecondaryClicked);
    if (auto* Button = XQPButtonBinding::GetButton(DismissButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_CommonDialog::DismissClicked);
}

void UUI_CommonDialog::ReplayEntrance(bool bFadeOnly)
{
    EntranceTime = 0.f;
    bOnlyFade = bFadeOnly;
    SetRenderOpacity(0.f);
    if (DialogCard) DialogCard->SetRenderTransformPivot(FVector2D(0.5f));
    UpdateCanTick();
}

void UUI_CommonDialog::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    EntranceTime = FMath::Min(0.36f, EntranceTime + DeltaTime);
    const float Alpha = EntranceTime / 0.36f;
    SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, Alpha));
    if (DialogCard)
    {
        const float T = Alpha - 1.f;
        const float Ease = 1.f + 2.3f * T * T * T + 1.3f * T * T;
        DialogCard->SetRenderScale(FVector2D(bOnlyFade ? 1.f : FMath::Lerp(0.88f, 1.f, Ease)));
        DialogCard->SetRenderTranslation(FVector2D(0.f, bOnlyFade ? 0.f : 18.f * (1.f - Ease)));
    }
}

void UUI_CommonDialog::Configure(const FText& Title, const FText& Message, const FText& Primary,
                                const FText& Secondary, const FText& DismissCaption)
{
    CacheDefaultAppearance();
    if (DialogTitle)
    {
        DialogTitle->SetFont(DefaultTitleFont);
        DialogTitle->SetColorAndOpacity(DefaultTitleColor);
    }
    if (DialogMessage)
    {
        DialogMessage->SetFont(DefaultMessageFont);
        DialogMessage->SetColorAndOpacity(DefaultMessageColor);
    }
    if (OutcomeAccent) OutcomeAccent->SetVisibility(ESlateVisibility::Collapsed);
    if (MessageFactionPanel) MessageFactionPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (DialogTitle) DialogTitle->SetText(Title);
    SetMessage(Message);
    SetPrimaryAction(Primary, true);
    XQPButtonBinding::SetCaption(SecondaryButton, Secondary);
    XQPButtonBinding::SetVisibility(SecondaryButton, Secondary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    XQPButtonBinding::SetCaption(DismissButton, DismissCaption);
    XQPButtonBinding::SetVisibility(DismissButton, DismissCaption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UUI_CommonDialog::SetPrimaryAction(const FText& Caption, bool bEnabled)
{
    XQPButtonBinding::SetCaption(PrimaryButton, Caption);
    XQPButtonBinding::SetVisibility(PrimaryButton, Caption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (PrimaryButton) PrimaryButton->SetIsEnabled(bEnabled);
}

void UUI_CommonDialog::SetMessage(const FText& Message)
{
    if (DialogMessage) DialogMessage->SetText(Message);
}

void UUI_CommonDialog::CacheDefaultAppearance()
{
    if (bAppearanceCached || !DialogTitle || !DialogMessage) return;
    DefaultTitleFont = DialogTitle->GetFont();
    DefaultMessageFont = DialogMessage->GetFont();
    DefaultTitleColor = DialogTitle->GetColorAndOpacity();
    DefaultMessageColor = DialogMessage->GetColorAndOpacity();
    bAppearanceCached = true;
}

void UUI_CommonDialog::SetMatchOutcome(bool bWon, bool bDraw)
{
    CacheDefaultAppearance();
    const FLinearColor Accent = bDraw ? FLinearColor(.64f, .69f, .75f) : (bWon ? XQPUIStyle::Victory() : XQPUIStyle::Defeat());
    if (DialogTitle)
    {
        FSlateFontInfo Font = DefaultTitleFont;
        Font.Size += 4;
        Font.OutlineSettings.OutlineSize = 2;
        Font.OutlineSettings.OutlineColor = FLinearColor(.025f, .012f, .008f, 1.f);
        DialogTitle->SetFont(Font);
        DialogTitle->SetColorAndOpacity(FSlateColor(Accent));
    }
    if (OutcomeAccent)
    {
        OutcomeAccent->SetBrushColor(Accent);
        OutcomeAccent->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UUI_CommonDialog::SetMessageFaction(EChessColor Faction)
{
    if (MessageFactionLabel)
    {
        MessageFactionLabel->SetText(FText::FromString(Faction == EChessColor::REDCHESS ? TEXT("红方胜利") : TEXT("黑方胜利")));
        XQPUIStyle::SetFactionText(MessageFactionLabel, Faction, true);
    }
    if (MessageFactionPanel)
    {
        MessageFactionPanel->SetBrushColor(XQPUIStyle::Paper());
        MessageFactionPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UUI_CommonDialog::Dismiss()
{
    if (GetGameInstance()) GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this);
}

void UUI_CommonDialog::PrimaryClicked() { OnPrimaryAction.ExecuteIfBound(); }
void UUI_CommonDialog::SecondaryClicked()
{
    if (OnSecondaryAction.IsBound()) OnSecondaryAction.Execute();
    else Dismiss();
}
void UUI_CommonDialog::DismissClicked() { Dismiss(); }
