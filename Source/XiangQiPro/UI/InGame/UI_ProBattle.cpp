#include "UI_ProBattle.h"
#include "UI_ProResult.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/ProBattleComponent.h"

void UUI_ProBattle::NativeConstruct()
{
    Super::NativeConstruct();
    LocalSegments.Reset(); EnemySegments.Reset();
    for (int32 I = 0; I < 20; ++I)
    {
        LocalSegments.Add(Cast<UProgressBar>(GetWidgetFromName(FName(*FString::Printf(TEXT("Supply_Local_%d"), I)))));
        EnemySegments.Add(Cast<UProgressBar>(GetWidgetFromName(FName(*FString::Printf(TEXT("Supply_Enemy_%d"), I)))));
    }
    if (Countdown) Countdown->SetRenderTransformPivot(FVector2D(0.5f));
    BattleFadeTime = 0.f;
    UpdateBattlePresentation(0.f);
}

void UUI_ProBattle::UpdateBattlePresentation(float Alpha)
{
    if (auto* Root = Cast<UPanelWidget>(WidgetTree->RootWidget))
        for (auto* Child : Root->GetAllChildren())
            if (Child && Child->GetFName() != TEXT("BattleReadyOverlay"))
            {
                Child->SetRenderOpacity(Alpha);
                Child->SetIsEnabled(Alpha > 0.f);
                Child->SetVisibility(Alpha > 0.f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
            }
}

void UUI_ProBattle::UpdateSupply(const TArray<TObjectPtr<UProgressBar>>& Bars, float Value, int32 Max, FLinearColor Color)
{
    for (int32 I = 0; I < Bars.Num(); ++I)
    {
        if (!Bars[I]) continue;
        if (auto* Holder = Bars[I]->GetParent()) Holder->SetVisibility(I < Max ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        Bars[I]->SetVisibility(I < Max ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        const float Fill = FMath::Clamp(Value - float(I), 0.f, 1.f);
        Bars[I]->SetPercent(Fill);
        Bars[I]->SetFillColorAndOpacity(Fill >= 1.f ? Color : FMath::Lerp(Color, FLinearColor::White, 0.65f));
    }
}

void UUI_ProBattle::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!State || !State->IsProBattle() || !State->ProBattle->IsActive()) return;
    const auto* Battle = State->ProBattle.Get();
    BattleFadeTime = State->BattleStart->HasStarted() ? FMath::Min(0.45f, BattleFadeTime + DeltaTime) : 0.f;
    UpdateBattlePresentation(FMath::SmoothStep(0.f, 1.f, BattleFadeTime / 0.45f));
    if (Text_AIThinking) Text_AIThinking->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_RoundMark_P1) Image_RoundMark_P1->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_RoundMark_P2) Image_RoundMark_P2->SetVisibility(ESlateVisibility::Collapsed);
    if (ModeStatus) ModeStatus->SetVisibility(ESlateVisibility::Collapsed);
    const float Remaining = Battle->GetRemaining();
    const int32 Seconds = FMath::CeilToInt(Remaining);
    PulseTime += DeltaTime;
    if (Countdown)
    {
        Countdown->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
        const float Scale = 1.f + 0.34f * (1.f - Remaining / Battle->GetSettings().Duration);
        Countdown->SetRenderScale(FVector2D(Scale));
        const bool Urgent = Remaining > 0.f && Remaining <= 10.f && !State->IsGameOver();
        const float Flash = (FMath::Sin(PulseTime * PI * 5.f) + 1.f) * 0.5f;
        Countdown->SetColorAndOpacity(FSlateColor(Urgent ? FMath::Lerp(FLinearColor(1.f, 0.04f, 0.03f), FLinearColor::White, Flash)
            : FLinearColor(1.f, 0.86f, 0.58f)));
        Countdown->SetRenderTranslation(Urgent ? FVector2D(FMath::Sin(PulseTime * 67.f) * 2.8f, FMath::Sin(PulseTime * 83.f) * 1.8f) : FVector2D::ZeroVector);
    }
    const int32 Max = Battle->GetSettings().MaxSupply;
    const float Red = Battle->GetSupply(EChessColor::REDCHESS), Black = Battle->GetSupply(EChessColor::BLACKCHESS);
    const bool LocalRed = Battle->GetLocalColor() == EChessColor::REDCHESS;
    const float Local = LocalRed ? Red : Black;
    const FLinearColor LocalColor = LocalRed ? FLinearColor(0.95f, 0.32f, 0.14f) : FLinearColor(0.16f, 0.65f, 0.88f);
    UpdateSupply(LocalSegments, Local, Max, LocalColor);
    UpdateSupply(EnemySegments, Black, Max, FLinearColor(0.16f, 0.65f, 0.88f));
    EnemySupplyPanel->SetVisibility(State->IsLANGame() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (auto* Panel = GetWidgetFromName(TEXT("BlackInfoPanel")))
        if (auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot)) PanelSlot->SetSize(FVector2D(400.f, State->IsLANGame() ? 78.f : 142.f));
    LocalSupplyText->SetText(FText::FromString(FString::Printf(TEXT("我的粮草  %d / %d%s"), FMath::FloorToInt(Local + 0.0001f), Max, Battle->IsDoubleSupply() ? TEXT("  ·  双倍恢复") : TEXT(""))));
    EnemySupplyText->SetText(FText::FromString(FString::Printf(TEXT("粮草 %d / %d"), FMath::FloorToInt(Black + 0.0001f), Max)));
    UpdateScore(Battle->GetStats(EChessColor::REDCHESS).CaptureScore, Battle->GetStats(EChessColor::BLACKCHESS).CaptureScore);
    if (!Battle->IsRunning()) { bWasDoubleSupply = false; DoubleBannerTime = 4.f; }
    if (Battle->IsDoubleSupply() && !bWasDoubleSupply && Battle->IsRunning()) { DoubleBannerTime = 0.f; bWasDoubleSupply = true; }
    DoubleBannerTime += DeltaTime;
    const bool ShowDouble = DoubleBannerTime < 3.f && !State->IsGameOver();
    DoubleSupplyBanner->SetVisibility(ShowDouble ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (ShowDouble)
    {
        DoubleSupplyBanner->SetRenderOpacity(FMath::Clamp(DoubleBannerTime / .2f, 0.f, 1.f) * FMath::Clamp((3.f - DoubleBannerTime) / .6f, 0.f, 1.f));
        DoubleSupplyBanner->SetRenderScale(FVector2D(1.f + .12f * FMath::Exp(-DoubleBannerTime * 5.f)));
    }
}

void UUI_ProBattle::ShowMatchResult(EChessColor Winner)
{
    auto* Result = Cast<UUI_ProResult>(OpenResultDialog());
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!Result || !State) return;
    Result->ShowBattle(State->ProBattle, Text_Name_P1 ? Text_Name_P1->GetText() : FText::FromString(TEXT("红方")),
        Text_Name_P2 ? Text_Name_P2->GetText() : FText::FromString(TEXT("黑方")), Winner);
    RefreshRematchAction();
}
