#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_SettingsMenu.generated.h"

UCLASS()
class XIANGQIPRO_API UUI_SettingsMenu : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    UPROPERTY(EditDefaultsOnly, Category = "Player Settings") TSubclassOf<UUserWidget> PlayerSettingsClass;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> PlayerSettingsButton;
    UFUNCTION() void OpenPlayerSettings();
};
