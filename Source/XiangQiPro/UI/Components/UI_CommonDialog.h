#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_CommonDialog.generated.h"

class UTextBlock;

// 所有对局结果和确认提示共用此资产；调用方提供文案和操作。
UCLASS()
class XIANGQIPRO_API UUI_CommonDialog : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    void ReplayEntrance(bool bFadeOnly = false);
    UFUNCTION(BlueprintCallable, Category="Dialog")
    void Configure(const FText& Title, const FText& Message, const FText& Primary,
                   const FText& Secondary, const FText& Dismiss = FText::GetEmpty());
    UFUNCTION(BlueprintCallable, Category="Dialog")
    void SetPrimaryAction(const FText& Caption, bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Dialog")
    void SetMessage(const FText& Message);
    UFUNCTION(BlueprintCallable, Category="Dialog")
    void Dismiss();

    FSimpleDelegate OnPrimaryAction;
    FSimpleDelegate OnSecondaryAction;

private:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> DialogCard;
    float EntranceTime = 0.f;
    bool bOnlyFade = false;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DialogTitle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DialogMessage;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> PrimaryButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> SecondaryButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> DismissButton;
    UFUNCTION() void PrimaryClicked();
    UFUNCTION() void SecondaryClicked();
    UFUNCTION() void DismissClicked();
};
