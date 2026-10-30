#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "UI_CommonDialog.generated.h"

class UTextBlock;
class UBorder;

// 所有对局结果和确认提示共用此资产；调用方提供文案和操作。
UCLASS()
class XIANGQIPRO_API UUI_CommonDialog : public UXQPMobileWidget
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
    void SetMatchOutcome(bool bWon, bool bDraw = false);
    UFUNCTION(BlueprintCallable, Category="Dialog")
    void SetMessageFaction(EChessColor Faction);
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
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> OutcomeAccent;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> MessageFactionPanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MessageFactionLabel;
    FSlateFontInfo DefaultTitleFont;
    FSlateFontInfo DefaultMessageFont;
    FSlateColor DefaultTitleColor;
    FSlateColor DefaultMessageColor;
    bool bAppearanceCached = false;
    void CacheDefaultAppearance();
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> PrimaryButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> SecondaryButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> DismissButton;
    UFUNCTION() void PrimaryClicked();
    UFUNCTION() void SecondaryClicked();
    UFUNCTION() void DismissClicked();
};
