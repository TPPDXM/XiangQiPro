#pragma once
#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_SettingsMenu.generated.h"

UCLASS()
class XIANGQIPRO_API UUI_SettingsMenu : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    UPROPERTY(EditDefaultsOnly, Category = "Player Settings") TSubclassOf<UUserWidget> PlayerSettingsClass;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> PlayerSettingsButton;
    UFUNCTION() void OpenPlayerSettings();
};
