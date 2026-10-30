#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "XQPMobileWidget.generated.h"

class UPanelWidget;

/** Shared presentation behavior. Portrait layouts are authored in /Game/UMG/Mobile. */
UCLASS(Abstract)
class XIANGQIPRO_API UXQPMobileWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPanelWidget* GetPresentationRoot() const;
    // Menus and transitions control the active scene's audio without a desktop camera reference.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void SetSceneMusicVolume(float VolumeMultiplier);
};
