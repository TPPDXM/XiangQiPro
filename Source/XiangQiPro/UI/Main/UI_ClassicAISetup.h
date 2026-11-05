#pragma once
#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_ClassicAISetup.generated.h"

class UUI_ToggleSwitch;
class UTextBlock;
class UUI_CommonDialog;

// 布局、配色和控件都来自UMG资产，这里只同步开关状态与存档数据。
UCLASS()
class XIANGQIPRO_API UUI_ClassicAISetup : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUI_ToggleSwitch> ChallengeSwitch;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HighestStreakText;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UUI_CommonDialog> CommonDialogClass;
    UPROPERTY(Transient) TObjectPtr<UUI_CommonDialog> ChallengeDialog;
    TSharedPtr<SWidget> PreparedChallengeSlate;
    UUI_CommonDialog* GetChallengeDialog();
    void ApplyChallengeSelection(bool bChecked);
    UFUNCTION() void OnChallengeChanged(bool bChecked);
};
