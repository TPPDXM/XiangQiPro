#include "XQPMobileWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"

UPanelWidget* UXQPMobileWidget::GetPresentationRoot() const
{
    if (auto* MobileCanvas = Cast<UPanelWidget>(GetWidgetFromName(TEXT("MobileLayoutCanvas")))) return MobileCanvas;
    return WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
}

void UXQPMobileWidget::SetSceneMusicVolume(float VolumeMultiplier)
{
    const APlayerController* Player = GetOwningPlayer();
    AActor* ViewTarget = Player ? Player->GetViewTarget() : nullptr;
    if (IsValid(ViewTarget))
        if (UAudioComponent* Audio = ViewTarget->FindComponentByClass<UAudioComponent>())
            Audio->SetVolumeMultiplier(FMath::Max(0.f, VolumeMultiplier));
}

