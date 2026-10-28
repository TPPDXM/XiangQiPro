#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_ClassicAISetup.generated.h"

class UCheckBox;
class UTextBlock;
class UImage;
class UUI_CommonDialog;

// 布局、配色和控件都来自UMG资产，这里只同步开关状态与存档数据。
UCLASS()
class XIANGQIPRO_API UUI_ClassicAISetup : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> ChallengeSwitch;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HighestStreakText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> SwitchThumb;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UUI_CommonDialog> CommonDialogClass;
    TWeakObjectPtr<UUI_CommonDialog> ChallengeDialog;
    void ApplyChallengeSelection(bool bChecked);
    UFUNCTION() void OnChallengeChanged(bool bChecked);
    float SwitchPosition = 0.f;
    float SwitchStart = 0.f;
    float SwitchTarget = 0.f;
    float SwitchElapsed = 0.f;
};
