#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_ToggleSwitch.generated.h"

class UCheckBox;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXQPSwitchChanged, bool, bChecked);

/**
 * 通用UMG开关：外观来自WB_ToggleSwitch资产，滑块动画与状态同步集中在组件内。
 * 组件只发出用户操作事件；连战结算、保存AI配置等业务仍由所在页面处理。
 */
UCLASS()
class XIANGQIPRO_API UUI_ToggleSwitch : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    /** 程序同步不广播OnChanged，避免打开页面或拒绝关闭时再次触发业务弹窗。 */
    UFUNCTION(BlueprintCallable, Category="Switch")
    void SetIsChecked(bool bChecked, bool bAnimate = false);

    UFUNCTION(BlueprintPure, Category="Switch")
    bool IsChecked() const { return bIsChecked; }

    UPROPERTY(BlueprintAssignable, Category="Switch")
    FOnXQPSwitchChanged OnChanged;

    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UPROPERTY(EditAnywhere, Category="Switch") bool bIsChecked = false;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Toggle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> SwitchTrack;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> SwitchThumb;

    // 与人机对弈原开关保持一致：0.18秒平滑移动，途中反向从当前位置继续。
    float Position = 0.f;
    float StartPosition = 0.f;
    float TargetPosition = 0.f;
    float Elapsed = 0.f;
    bool bSynchronizing = false;
    void UpdateThumb();
    UFUNCTION() void HandleUserChanged(bool bChecked);
};
