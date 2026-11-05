#include "UI_ToggleSwitch.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

void UUI_ToggleSwitch::NativePreConstruct()
{
    Super::NativePreConstruct();
    // Designer预览与游戏初始化均使用同一个默认状态，不需要每个页面额外移动滑块。
    SetIsChecked(bIsChecked, false);
}

void UUI_ToggleSwitch::NativeConstruct()
{
    Super::NativeConstruct();
    if (Toggle)
    {
        Toggle->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleUserChanged);
        Toggle->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleUserChanged);
    }
    SetIsChecked(bIsChecked, false);
    UpdateCanTick();
}

void UUI_ToggleSwitch::NativeDestruct()
{
    if (Toggle) Toggle->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleUserChanged);
    Super::NativeDestruct();
}

void UUI_ToggleSwitch::SetIsChecked(bool bChecked, bool bAnimate)
{
    bIsChecked = bChecked;
    // UCheckBox当前不会在SetIsChecked时广播，这个保护也兼容以后改成蓝图驱动的实现。
    TGuardValue<bool> SyncGuard(bSynchronizing, true);
    if (Toggle) Toggle->SetIsChecked(bChecked);
    StartPosition = Position;
    TargetPosition = bChecked ? 1.f : 0.f;
    Elapsed = 0.f;
    if (!bAnimate) Position = TargetPosition;
    UpdateThumb();
}

void UUI_ToggleSwitch::HandleUserChanged(bool bChecked)
{
    if (bSynchronizing || bChecked == bIsChecked) return;
    SetIsChecked(bChecked, true);
    // 父页面可以在回调内拒绝本次操作并同步回旧状态，组件不会产生递归事件。
    OnChanged.Broadcast(bChecked);
}

void UUI_ToggleSwitch::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!FMath::IsNearlyEqual(Position, TargetPosition))
    {
        Elapsed = FMath::Min(Elapsed + DeltaTime, .18f);
        const float T = Elapsed / .18f;
        Position = FMath::Lerp(StartPosition, TargetPosition, T * T * (3.f - 2.f * T));
    }
    // 尺寸可能因DPI或安全区改变，每帧用当前几何更新行程，不缓存固定像素距离。
    UpdateThumb();
}

void UUI_ToggleSwitch::UpdateThumb()
{
    if (!SwitchTrack || !SwitchThumb || !SwitchThumb->GetParent()) return;
    const UWidget* ThumbFrame = SwitchThumb->GetParent();
    float TrackWidth = SwitchTrack->GetCachedGeometry().GetLocalSize().X;
    float ThumbWidth = ThumbFrame->GetCachedGeometry().GetLocalSize().X;
    // 首帧尚未排版时沿用UMG的设计尺寸，避免先显示一帧错误的关闭位置。
    if (TrackWidth <= 0.f)
        if (const auto* Box = Cast<USizeBox>(SwitchTrack)) TrackWidth = Box->GetWidthOverride();
    if (ThumbWidth <= 0.f)
        if (const auto* Box = Cast<USizeBox>(ThumbFrame)) ThumbWidth = Box->GetWidthOverride();
    const float Travel = FMath::Max(0.f, TrackWidth - ThumbWidth);
    SwitchThumb->SetRenderTranslation(FVector2D(Travel * FMath::Clamp(Position, 0.f, 1.f), 0.f));
}
