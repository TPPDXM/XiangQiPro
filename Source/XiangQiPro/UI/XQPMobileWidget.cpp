#include "XQPMobileWidget.h"
#include "XiangQiPro/Audio/XQPFeedbackAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"

void UXQPMobileWidget::NativeConstruct()
{
    Super::NativeConstruct();
    UXQPFeedbackAudioSubsystem::ConfigureFor(this);
}

UPanelWidget* UXQPMobileWidget::GetPresentationRoot() const
{
    if (auto* MobileCanvas = Cast<UPanelWidget>(GetWidgetFromName(TEXT("MobileLayoutCanvas")))) return MobileCanvas;
    return WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
}

void UXQPMobileWidget::SetSceneMusicVolume(float VolumeMultiplier)
{
    // 旧版菜单蓝图会尝试淡出音乐，联机选项菜单不改变背景曲音量。
    if (const auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
        State && State->IsLANGame()) return;
    const APlayerController* Player = GetOwningPlayer();
    AActor* ViewTarget = Player ? Player->GetViewTarget() : nullptr;
    if (IsValid(ViewTarget))
        if (UAudioComponent* Audio = ViewTarget->FindComponentByClass<UAudioComponent>())
            Audio->SetVolumeMultiplier(FMath::Max(0.f, VolumeMultiplier));
}

