#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_BattleReady.generated.h"
class UBackgroundBlur;
class UTextBlock;

UCLASS()
class XIANGQIPRO_API UUI_BattleReady : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBackgroundBlur> ReadyBlur;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> ConfirmationCard;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RedStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BlackStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StartCountdown;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> ConfirmButton;
    UFUNCTION() void Confirm();
    void RefreshPresentation();
};
