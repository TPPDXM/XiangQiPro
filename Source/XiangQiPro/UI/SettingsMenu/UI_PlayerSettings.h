#pragma once
#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_PlayerSettings.generated.h"
class UEditableTextBox;
class UTextBlock;

UCLASS()
class XIANGQIPRO_API UUI_PlayerSettings : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> NameInput;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SaveStatusText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> SaveNameButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> BackButton;
    UFUNCTION() void SaveName();
    UFUNCTION() void OnNameCommitted(const FText& Text, ETextCommit::Type Method);
    UFUNCTION() void GoBack();
};
