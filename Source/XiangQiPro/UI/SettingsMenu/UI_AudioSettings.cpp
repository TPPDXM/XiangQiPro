// Copyright 2026 Ultimate Player All Rights Reserved.
#include "UI_AudioSettings.h"
#include "XiangQiPro/Audio/XQPAudioSettingsSubsystem.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UUI_AudioSettings::NativeConstruct()
{
    Super::NativeConstruct();
    UGameInstance* Game = GetGameInstance();
    if (!Game) return;
    AudioSettings = Game->GetSubsystem<UXQPAudioSettingsSubsystem>();
    if (!AudioSettings.IsValid()) return;
    AudioSettings->ApplyToWorld(GetWorld());

    // 始终使用工程自己的 WB_ComboBox / WB_ComboBox_Mobile；选项重建时抑制初始化回调。
    bRefreshing = true;
    for (UUI_XQP_ComboBox* Combo : {MusicEnabledCombo.Get(), EffectsEnabledCombo.Get()})
    {
        if (!Combo || !Combo->ComboBox) continue;
        Combo->ComboBox->ClearOptions();
        Combo->ComboBox->AddOption(TEXT("关闭"));
        Combo->ComboBox->AddOption(TEXT("开启"));
    }
    if (MusicEnabledCombo && MusicEnabledCombo->ComboBox)
        MusicEnabledCombo->ComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::MusicSelectionChanged);
    if (EffectsEnabledCombo && EffectsEnabledCombo->ComboBox)
        EffectsEnabledCombo->ComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::EffectsSelectionChanged);
    if (MusicVolumeSlider) MusicVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::MusicVolumeChanged);
    if (EffectsVolumeSlider) EffectsVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::EffectsVolumeChanged);
    for (USlider* Slider : {MusicVolumeSlider.Get(), EffectsVolumeSlider.Get()})
    {
        if (!Slider) continue;
        Slider->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::SaveVolume);
        Slider->OnControllerCaptureEnd.AddUniqueDynamic(this, &ThisClass::SaveVolume);
    }
    if (UButton* Button = XQPButtonBinding::GetButton(BackButton)) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::GoBack);
    RefreshControls();
    bRefreshing = false;
}

void UUI_AudioSettings::RefreshControls()
{
    if (!AudioSettings.IsValid()) return;
    if (MusicEnabledCombo && MusicEnabledCombo->ComboBox) MusicEnabledCombo->ComboBox->SetSelectedIndex(AudioSettings->IsMusicEnabled() ? 1 : 0);
    if (EffectsEnabledCombo && EffectsEnabledCombo->ComboBox) EffectsEnabledCombo->ComboBox->SetSelectedIndex(AudioSettings->IsEffectsEnabled() ? 1 : 0);
    if (MusicVolumeSlider) MusicVolumeSlider->SetValue(AudioSettings->GetMusicVolume());
    if (EffectsVolumeSlider) EffectsVolumeSlider->SetValue(AudioSettings->GetEffectsVolume());
    RefreshVolumes();
}

void UUI_AudioSettings::RefreshVolumes()
{
    if (!AudioSettings.IsValid()) return;
    // Collapse 整行而非只隐藏滑块，关闭后不留空白；重新开启恢复此前保存的音量。
    if (MusicVolumeRow) MusicVolumeRow->SetVisibility(AudioSettings->IsMusicEnabled() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (EffectsVolumeRow) EffectsVolumeRow->SetVisibility(AudioSettings->IsEffectsEnabled() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (MusicVolumeFill) MusicVolumeFill->SetPercent(AudioSettings->GetMusicVolume());
    if (EffectsVolumeFill) EffectsVolumeFill->SetPercent(AudioSettings->GetEffectsVolume());
    if (MusicVolumeText) MusicVolumeText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(AudioSettings->GetMusicVolume() * 100))));
    if (EffectsVolumeText) EffectsVolumeText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(AudioSettings->GetEffectsVolume() * 100))));
}

void UUI_AudioSettings::MusicSelectionChanged(FString Selected, ESelectInfo::Type Method)
{
    if (bRefreshing || Method == ESelectInfo::Direct || !AudioSettings.IsValid()) return;
    AudioSettings->SetMusicEnabled(MusicEnabledCombo->ComboBox->GetSelectedIndex() == 1);
    RefreshVolumes();
}

void UUI_AudioSettings::EffectsSelectionChanged(FString Selected, ESelectInfo::Type Method)
{
    if (bRefreshing || Method == ESelectInfo::Direct || !AudioSettings.IsValid()) return;
    AudioSettings->SetEffectsEnabled(EffectsEnabledCombo->ComboBox->GetSelectedIndex() == 1);
    RefreshVolumes();
}

void UUI_AudioSettings::MusicVolumeChanged(float Value)
{
    if (bRefreshing || !AudioSettings.IsValid()) return;
    AudioSettings->SetMusicVolume(Value);
    RefreshVolumes();
}

void UUI_AudioSettings::EffectsVolumeChanged(float Value)
{
    if (bRefreshing || !AudioSettings.IsValid()) return;
    AudioSettings->SetEffectsVolume(Value);
    RefreshVolumes();
}

void UUI_AudioSettings::SaveVolume()
{
    if (AudioSettings.IsValid()) AudioSettings->SaveSettings();
}

void UUI_AudioSettings::GoBack()
{
    SaveVolume();
    if (UGameInstance* Game = GetGameInstance()) Game->GetSubsystem<UUIManager>()->RemoveUI(this);
}

void UUI_AudioSettings::NativeDestruct()
{
    // 返回、关闭整个设置栈、切关卡和键盘调节后离开，都能提交最后一次预览值。
    SaveVolume();
    Super::NativeDestruct();
}
