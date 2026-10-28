#pragma once

#include "CoreMinimal.h"
#include "UI_Battle2P_Base.h"
#include "UI_ProBattle.generated.h"

class UProgressBar;

UCLASS()
class XIANGQIPRO_API UUI_ProBattle : public UUI_Battle2P_Base
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void ShowMatchResult(EChessColor Winner) override;
private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Countdown;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> LocalSupplyText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> LocalSupplyPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> EnemySupplyPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> EnemySupplyText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DoubleSupplyBanner;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> LocalSegments;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> EnemySegments;
    float PulseTime = 0.f;
    float DoubleBannerTime = 4.f;
    bool bWasDoubleSupply = false;
    float BattleFadeTime = 0.f;
    void UpdateBattlePresentation(float Alpha);
    void UpdateSupply(const TArray<TObjectPtr<UProgressBar>>& Bars, float Value, int32 Max, FLinearColor Color);
};
