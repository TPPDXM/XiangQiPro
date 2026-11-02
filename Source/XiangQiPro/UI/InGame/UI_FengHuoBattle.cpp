#include "UI_FengHuoBattle.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "UI_FengHuoResult.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "Blueprint/WidgetTree.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/FengHuoBattleComponent.h"
#include "XiangQiPro/Camera/Camera2PActor.h"
#include "EngineUtils.h"

void UUI_FengHuoBattle::NativeConstruct()
{
    Super::NativeConstruct();
    LocalSegments.Reset(); EnemySegments.Reset();
    ObservedDepartures.Reset();
    if (!EnemySupplyPanel) EnemySupplyPanel = GetWidgetFromName(TEXT("EnemySupplyPanel"));
    for (int32 I = 0; I < 20; ++I)
    {
        LocalSegments.Add(Cast<UProgressBar>(GetWidgetFromName(FName(*FString::Printf(TEXT("Supply_Local_%d"), I)))));
        EnemySegments.Add(Cast<UProgressBar>(GetWidgetFromName(FName(*FString::Printf(TEXT("Supply_Enemy_%d"), I)))));
    }
    ConfigureSpectatorSupply();
    if (Countdown) Countdown->SetRenderTransformPivot(FVector2D(0.5f));
    BattleFadeTime = 0.f;
    UpdateBattlePresentation(0.f);
    if (const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>(); State && State->IsSpectator())
        if (auto* Prices = GetWidgetFromName(TEXT("PiecePricePanel"))) Prices->SetVisibility(ESlateVisibility::Collapsed);
}

void UUI_FengHuoBattle::ConfigureSpectatorSupply()
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!State || !State->IsSpectator()) return;
    // 文本收起后一起移除其布局占位。双方保留同样的 14 高度粮草条与上下 8 的内边距。
    for (auto* Label : {LocalSupplyText.Get(), EnemySupplyText.Get()})
        if (Label) Label->SetVisibility(ESlateVisibility::Collapsed);
    if (!XQPMobile::IsPortrait(this))
    {
        // PC 的粮草条放在姓名卡片内：姓名行按内容排布，粮草区域占满剩余
        // 高度并垂直居中。去掉固定小格宽度，让任意粮草上限都能填满卡片。
        for (const FName Name : {FName(TEXT("RedInfoPanel")), FName(TEXT("BlackInfoPanel"))})
            if (auto* Frame = Cast<UBorder>(GetWidgetFromName(Name))) Frame->SetVerticalAlignment(VAlign_Fill);
        for (const FName Name : {FName(TEXT("LocalSupplyPanel")), FName(TEXT("EnemySupplyPanel"))})
            if (auto* Body = GetWidgetFromName(Name))
                if (auto* LayoutSlot = Cast<UVerticalBoxSlot>(Body->Slot))
                {
                    LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                    LayoutSlot->SetVerticalAlignment(VAlign_Center);
                    LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
                    LayoutSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
                }
        if (auto* Frame = Cast<UBorder>(LocalSupplyPanel))
        {
            Frame->SetPadding(FMargin(0.f));
            Frame->SetVerticalAlignment(VAlign_Center);
            Frame->SetHorizontalAlignment(HAlign_Fill);
        }
        for (const FName Name : {FName(TEXT("LocalSegmentRow")), FName(TEXT("EnemySegmentRow"))})
            if (auto* Row = GetWidgetFromName(Name))
                if (auto* LayoutSlot = Cast<UVerticalBoxSlot>(Row->Slot))
                {
                    LayoutSlot->SetPadding(FMargin(0.f));
                    LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                    LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
                    LayoutSlot->SetVerticalAlignment(VAlign_Center);
                }
        for (const auto* Bars : {&LocalSegments, &EnemySegments})
            for (const auto& Bar : *Bars)
                if (Bar)
                {
                    if (auto* Holder = Cast<USizeBox>(Bar->GetParent()))
                    {
                        Holder->ClearWidthOverride();
                        Holder->SetHeightOverride(14.f);
                        if (auto* LayoutSlot = Cast<UHorizontalBoxSlot>(Holder->Slot))
                        {
                            LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                            LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
                            LayoutSlot->SetVerticalAlignment(VAlign_Center);
                        }
                    }
                    if (auto* LayoutSlot = Cast<USizeBoxSlot>(Bar->Slot))
                    {
                        LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
                        LayoutSlot->SetVerticalAlignment(VAlign_Fill);
                    }
                }
        return;
    }
    for (const FName Name : {FName(TEXT("LocalSupplyBody")), FName(TEXT("EnemySupplyPanel"))})
        if (auto* Body = GetWidgetFromName(Name))
            if (auto* LayoutSlot = Cast<UBorderSlot>(Body->Slot))
            {
                LayoutSlot->SetPadding(FMargin(12.f, 8.f));
                LayoutSlot->SetVerticalAlignment(VAlign_Center);
            }
    for (const FName Name : {FName(TEXT("LocalSupplyPanel")), FName(TEXT("SpectatorEnemySupplyFrame"))})
        if (auto* Frame = Cast<UBorder>(GetWidgetFromName(Name)))
        {
            Frame->SetPadding(FMargin(12.f, 8.f));
            Frame->SetVerticalAlignment(VAlign_Center);
        }
    // 双方按内容自适应，避免父容器拉伸后只在底部留下多余空白。
    for (const FName Name : {FName(TEXT("LocalSegmentRow")), FName(TEXT("EnemySegmentRow"))})
        if (auto* Row = GetWidgetFromName(Name))
            if (auto* LayoutSlot = Cast<UVerticalBoxSlot>(Row->Slot))
            {
                LayoutSlot->SetPadding(FMargin(0.f));
                LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                LayoutSlot->SetVerticalAlignment(VAlign_Center);
            }
    if (LocalSupplyPanel)
        if (auto* LayoutSlot = Cast<UCanvasPanelSlot>(LocalSupplyPanel->Slot))
            LayoutSlot->SetOffsets(FMargin(12.f, -42.f, 12.f, 30.f));
}

void UUI_FengHuoBattle::UpdateBattlePresentation(float Alpha)
{
    if (auto* Root = GetPresentationRoot())
        for (auto* Child : Root->GetAllChildren())
            if (Child && Child->GetFName() != TEXT("BattleReadyOverlay") && Child != RecordPanel)
            {
                Child->SetRenderOpacity(Alpha);
                Child->SetIsEnabled(Alpha > 0.f);
                Child->SetVisibility(Alpha > 0.f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
            }
}

void UUI_FengHuoBattle::UpdateSupply(const TArray<TObjectPtr<UProgressBar>>& Bars, float Value, int32 Max, FLinearColor Color)
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

void UUI_FengHuoBattle::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (State && State->IsFengHuoBattle() && !State->IsSpectator() && !XQPMobile::IsPortrait(this)) FitDesktopCamera(Geometry);
    if (!State || !State->IsFengHuoBattle() || !State->FengHuoBattle->IsActive()) return;
    const auto* Battle = State->FengHuoBattle.Get();
    const bool bWatching = State->IsSpectator();
    if (bWatching && !XQPMobile::IsPortrait(this)) UpdateSpectatorRecords();
    BattleFadeTime = State->BattleStart->HasStarted() || bWatching ? FMath::Min(0.45f, BattleFadeTime + DeltaTime) : 0.f;
    UpdateBattlePresentation(FMath::SmoothStep(0.f, 1.f, BattleFadeTime / 0.45f));
    if (bWatching)
        if (auto* Prices = GetWidgetFromName(TEXT("PiecePricePanel"))) Prices->SetVisibility(ESlateVisibility::Collapsed);
    if (Text_AIThinking) Text_AIThinking->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_RoundMark_P1) Image_RoundMark_P1->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_RoundMark_P2) Image_RoundMark_P2->SetVisibility(ESlateVisibility::Collapsed);
    if (ModeStatus) ModeStatus->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Background = GetWidgetFromName(TEXT("ModeStatusBackground"))) Background->SetVisibility(ESlateVisibility::Collapsed);
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
    const bool ShowEnemySupply = bWatching || !State->IsLANGame();
    if (EnemySupplyPanel) EnemySupplyPanel->SetVisibility(ShowEnemySupply ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (XQPMobile::IsPortrait(this)) EnemySupplyText->SetVisibility(ShowEnemySupply ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!XQPMobile::IsPortrait(this))
    {
        for (const FName Name : {FName(TEXT("RedInfoPanel")), FName(TEXT("BlackInfoPanel"))})
            if (auto* Panel = GetWidgetFromName(Name))
                if (auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
                    PanelSlot->SetSize(FVector2D(400.f, bWatching ? 96.f : (Name == TEXT("BlackInfoPanel") && !State->IsLANGame()) ? 142.f : 78.f));
    }
    LocalSupplyText->SetText(FText::FromString(FString::Printf(TEXT("我的粮草  %d / %d%s"), FMath::FloorToInt(Local + 0.0001f), Max, Battle->IsDoubleSupply() ? TEXT("  ·  双倍恢复") : TEXT(""))));
    EnemySupplyText->SetText(FText::FromString(FString::Printf(TEXT("粮草 %d / %d"), FMath::FloorToInt(Black + 0.0001f), Max)));
    if (bWatching)
    {
        LocalSupplyText->SetVisibility(ESlateVisibility::Collapsed);
        EnemySupplyText->SetVisibility(ESlateVisibility::Collapsed);
    }
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

void UUI_FengHuoBattle::FitDesktopCamera(const FGeometry& Geometry)
{
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D Viewport = XQPMobile::ViewportSize(this);
    if (Size.X <= 0 || Size.Y <= 0 || Viewport.X <= 0 || Viewport.Y <= 0) return;
    if (!BattleCamera.IsValid())
    {
        TActorIterator<ACamera2PActor> It(GetWorld());
        if (It) BattleCamera = *It;
    }
    if (!BattleCamera.IsValid()) return;
    const FVector2D PixelScale = Viewport / Size;
    FBox2D Rect(FVector2D(16, 16) * PixelScale, Viewport - FVector2D(16, 16) * PixelScale);
    // 开场HUD尚未绘制时先按Canvas锚点预留空间，显示后再以真实几何校正。
    const auto Reserve = [&](const UWidget* Widget, bool bFooter)
    {
        if (!Widget) return;
        const FGeometry& G = Widget->GetCachedGeometry();
        float TopY, BottomY;
        if (!G.GetLocalSize().IsNearlyZero())
        {
            TopY = Geometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)).Y;
            BottomY = Geometry.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())).Y;
        }
        else if (const auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            const FAnchors Anchors = Slot->GetAnchors();
            const FMargin Offsets = Slot->GetOffsets();
            if (!FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y)) return;
            const float Height = Slot->GetAutoSize() ? Widget->GetDesiredSize().Y : Offsets.Bottom;
            TopY = Size.Y * Anchors.Minimum.Y + Offsets.Top - Slot->GetAlignment().Y * Height;
            BottomY = TopY + Height;
        }
        else return;
        if (bFooter) Rect.Max.Y = FMath::Min(Rect.Max.Y, (TopY - 16.f) * PixelScale.Y);
        else Rect.Min.Y = FMath::Max(Rect.Min.Y, (BottomY + 12.f) * PixelScale.Y);
    };
    Reserve(LocalSupplyPanel, true);
    Reserve(GetWidgetFromName(TEXT("ClockFrame")), false);
    BattleCamera->FitProBoardToViewport(Viewport, Rect);
}

void UUI_FengHuoBattle::ShowMatchResult(EChessColor Winner)
{
    auto* Result = Cast<UUI_FengHuoResult>(OpenResultDialog());
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!Result || !State) return;
    Result->ShowBattle(State->FengHuoBattle, Text_Name_P1 ? Text_Name_P1->GetText() : FText::FromString(TEXT("红方")),
        Text_Name_P2 ? Text_Name_P2->GetText() : FText::FromString(TEXT("黑方")), Winner);
    RefreshRematchAction();
}

void UUI_FengHuoBattle::UpdateSpectatorRecords()
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!State || !RecordRows) return;
    const auto& Units = State->FengHuoBattle->GetUnits();
    const bool bFirstFrame = ObservedDepartures.Num() != Units.Num();
    if (bFirstFrame) ObservedDepartures.Init(-1.f, Units.Num());
    TArray<int32> Changed;
    for (int32 Id = 0; Id < Units.Num(); ++Id)
    {
        const auto& Unit = Units[Id];
        // 快照每秒多次刷新，使用出发时间而非到达状态去重。第一次进入时
        // 只记正在出动的棋子，避免把无序的各棋子最后位置伪装成完整历史。
        if (Unit.From != Unit.To && (!bFirstFrame || Unit.bMoving) &&
            Unit.DepartAt != ObservedDepartures[Id]) Changed.Add(Id);
        ObservedDepartures[Id] = Unit.DepartAt;
    }
    Changed.StableSort([&Units](int32 A, int32 B) { return Units[A].DepartAt < Units[B].DepartAt; });
    for (int32 Id : Changed)
    {
        const auto& Unit = Units[Id];
        if (auto* Chess = State->FengHuoBattle->GetUnitActor(Id))
        {
            const FString Notation = GetMoveNotation(Chess, {Unit.From, Unit.To});
            const auto* Label = Unit.Color == EChessColor::REDCHESS ? Text_Name_P1 : Text_Name_P2;
            AddRecordRow(Unit.Color, Notation, Label ? Label->GetText().ToString() : FString(), {});
        }
    }
}
