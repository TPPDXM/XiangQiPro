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
    UPROPERTY(EditDefaultsOnly, Category = "Audio Settings") TSubclassOf<UUserWidget> AudioSettingsClass;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> PlayerSettingsButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> WB_Btn_AudioSettings;
    UFUNCTION() void OpenPlayerSettings();
    UFUNCTION() void OpenAudioSettings();
};
