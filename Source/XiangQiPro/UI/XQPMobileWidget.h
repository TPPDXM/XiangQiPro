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
    /** 公共界面构造时设置桌面/移动端的控件声音，嵌套按钮也一并处理。 */
    virtual void NativeConstruct() override;
    UPanelWidget* GetPresentationRoot() const;
    // Menus and transitions control the active scene's audio without a desktop camera reference.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void SetSceneMusicVolume(float VolumeMultiplier);
};
