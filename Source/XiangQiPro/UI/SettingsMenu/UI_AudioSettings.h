// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_AudioSettings.generated.h"

class UXQPAudioSettingsSubsystem;
class UUI_XQP_ComboBox;
class USlider;
class UProgressBar;
class UTextBlock;

/** PC/移动端各自保存布局和样式，共用音乐、音效的立即预览与保存行为。 */
UCLASS()
class XIANGQIPRO_API UUI_AudioSettings : public UXQPMobileWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUI_XQP_ComboBox> MusicEnabledCombo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUI_XQP_ComboBox> EffectsEnabledCombo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> MusicVolumeRow;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> EffectsVolumeRow;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> MusicVolumeSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> EffectsVolumeSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> MusicVolumeFill;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> EffectsVolumeFill;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> MusicVolumeText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> EffectsVolumeText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUserWidget> BackButton;
    TWeakObjectPtr<UXQPAudioSettingsSubsystem> AudioSettings;
    bool bRefreshing = false;

    void RefreshControls();
    void RefreshVolumes();
    UFUNCTION() void MusicSelectionChanged(FString Selected, ESelectInfo::Type Method);
    UFUNCTION() void EffectsSelectionChanged(FString Selected, ESelectInfo::Type Method);
    UFUNCTION() void MusicVolumeChanged(float Value);
    UFUNCTION() void EffectsVolumeChanged(float Value);
    UFUNCTION() void SaveVolume();
    UFUNCTION() void GoBack();
};
