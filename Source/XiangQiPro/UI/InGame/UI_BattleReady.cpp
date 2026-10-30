#include "UI_BattleReady.h"
#include "Components/BackgroundBlur.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/UI/XQPUIStyle.h"

void UUI_BattleReady::NativeConstruct()
{
    Super::NativeConstruct();
    XQPUIStyle::SetFactionText(RedStatus, EChessColor::REDCHESS);
    XQPUIStyle::SetFactionText(BlackStatus, EChessColor::BLACKCHESS);
    if (auto* State = GetWorld()->GetGameState<AXQPGameStateBase>())
    {
        State->BattleStart->OnSetupChanged.RemoveAll(this);
        State->BattleStart->OnSetupChanged.AddUObject(this, &UUI_BattleReady::RefreshPresentation);
    }
    FSlateFontInfo CountdownFont = StartCountdown->GetFont();
    CountdownFont.OutlineSettings.OutlineSize = 5;
    CountdownFont.OutlineSettings.OutlineColor = FLinearColor(.008f, .004f, .006f, 1.f);
    CountdownFont.OutlineSettings.bSeparateFillAlpha = true;
    StartCountdown->SetFont(CountdownFont);
    StartCountdown->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .92f, .72f)));
    StartCountdown->SetShadowOffset(FVector2D(0.f, 3.f));
    StartCountdown->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .8f));
    if (auto* Button = XQPButtonBinding::GetButton(ConfirmButton))
    {
        Button->OnClicked.AddUniqueDynamic(this, &UUI_BattleReady::Confirm);
    }
    RefreshPresentation();
}

void UUI_BattleReady::NativeDestruct()
{
    if (auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr)
        State->BattleStart->OnSetupChanged.RemoveAll(this);
    Super::NativeDestruct();
}

void UUI_BattleReady::Confirm()
{
    if (auto* State = GetWorld()->GetGameState<AXQPGameStateBase>()) State->BattleStart->ConfirmLocal();
}

void UUI_BattleReady::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshPresentation();
}

void UUI_BattleReady::RefreshPresentation()
{
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    const auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    const auto* Start = State ? State->BattleStart.Get() : nullptr;
    const bool Visible = Start && Start->IsActive() && !Start->HasStarted() && !State->IsGameOver() &&
        (!State->IsLANGame() || Room->GetState() != EXQPRoomState::Disconnected);
    SetVisibility(Visible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (!Visible) return;
    const float Time = Start->GetCountdown();
    // 先收起整个确认区域，再显示数字，确认状态变化时同步执行。
    ConfirmationCard->SetVisibility(Time < 0.f ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StartCountdown->SetVisibility(Time >= 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ReadyBlur->SetBlurStrength(Time < 0.f ? 18.f : 18.f * FMath::SmoothStep(0.f, 1.f, Time / 3.f));
    if (Time >= 0.f) StartCountdown->SetText(FText::AsNumber(FMath::Max(1, FMath::CeilToInt(Time))));
    RedStatus->SetText(FText::FromString(Start->IsConfirmed(EChessColor::REDCHESS) ? TEXT("红方 · 已确认") : TEXT("红方 · 等待确认")));
    BlackStatus->SetText(FText::FromString(Start->IsConfirmed(EChessColor::BLACKCHESS) ? TEXT("黑方 · 已确认") : TEXT("黑方 · 等待确认")));
    const EChessColor Local = State->IsLANGame() ? Room->GetLocalColor() : EChessColor::REDCHESS;
    const bool Ready = Start->IsConfirmed(Local);
    ConfirmButton->SetIsEnabled(!Ready);
    XQPButtonBinding::SetCaption(ConfirmButton, FText::FromString(Ready ? TEXT("已确认，等待对方") : TEXT("确认开始")));
}
