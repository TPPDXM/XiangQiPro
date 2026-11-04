// Copyright 2026 Ultimate Player All Rights Reserved.
#include "XQPAudioSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr const TCHAR* AudioSection = TEXT("XQP.Audio");
}

float UXQPAudioSettingsSubsystem::SanitizeVolume(float Value)
{
    // 配置文件可能被手动编辑；NaN/无穷不能进入音频线程的增益计算。
    return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 1.f;
}

void UXQPAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GConfig)
    {
        GConfig->GetBool(AudioSection, TEXT("MusicEnabled"), bMusicEnabled, GGameUserSettingsIni);
        GConfig->GetBool(AudioSection, TEXT("EffectsEnabled"), bEffectsEnabled, GGameUserSettingsIni);
        GConfig->GetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
        GConfig->GetFloat(AudioSection, TEXT("EffectsVolume"), EffectsVolume, GGameUserSettingsIni);
    }
    MusicVolume = SanitizeVolume(MusicVolume);
    EffectsVolume = SanitizeVolume(EffectsVolume);
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::ApplyToWorld);
    ApplyToWorld(GetWorld());
}

bool UXQPAudioSettingsSubsystem::LoadMixResources()
{
    // 只在确实有本机音频设备时加载。资源在 /Game/Audio 内，由项目既有 Cook 目录保留。
    if (!MusicClass) MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Settings/SC_XQPMusic.SC_XQPMusic"));
    if (!EffectsClass) EffectsClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Settings/SC_XQPEffects.SC_XQPEffects"));
    if (!SettingsMix) SettingsMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/Settings/SM_XQPUserSettings.SM_XQPUserSettings"));
    return MusicClass && EffectsClass && SettingsMix;
}

void UXQPAudioSettingsSubsystem::ApplyToWorld(UWorld* World)
{
    // 多个 PIE/GameInstance 可以共存，不能把另一局的世界误登记到本实例。
    if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_DedicatedServer) return;
    FAudioDeviceHandle Device = World->GetAudioDevice();
    if (!Device.IsValid() || !LoadMixResources()) return;
    if (!AppliedDevices.ContainsByPredicate([&Device](const FAudioDeviceHandle& Existing)
        { return Existing.GetDeviceID() == Device.GetDeviceID(); }))
    {
        // 首帧就提交偏好，再推入混音，防止启动音乐时短暂以默认全音量播放。
        Device->SetSoundMixClassOverride(SettingsMix, MusicClass, bMusicEnabled ? MusicVolume : 0.f, 1.f, 0.f, true);
        Device->SetSoundMixClassOverride(SettingsMix, EffectsClass, bEffectsEnabled ? EffectsVolume : 0.f, 1.f, 0.f, true);
        Device->PushSoundMixModifier(SettingsMix);
        AppliedDevices.Add(Device);
    }
    ApplyVolumes();
}

void UXQPAudioSettingsSubsystem::ApplyVolumes()
{
    // 更新的是分类总线，不遍历或重复缩放 AudioComponent；循环声和已播放的声音也能响应。
    for (FAudioDeviceHandle& Device : AppliedDevices)
    {
        if (!Device.IsValid()) continue;
        Device->SetSoundMixClassOverride(SettingsMix, MusicClass, bMusicEnabled ? MusicVolume : 0.f, 1.f, .05f, true);
        Device->SetSoundMixClassOverride(SettingsMix, EffectsClass, bEffectsEnabled ? EffectsVolume : 0.f, 1.f, .05f, true);
    }
}

void UXQPAudioSettingsSubsystem::SetMusicEnabled(bool bEnabled)
{
    bMusicEnabled = bEnabled;
    bDirty = true;
    ApplyToWorld(GetWorld());
    SaveSettings();
}

void UXQPAudioSettingsSubsystem::SetEffectsEnabled(bool bEnabled)
{
    bEffectsEnabled = bEnabled;
    bDirty = true;
    ApplyToWorld(GetWorld());
    SaveSettings();
}

void UXQPAudioSettingsSubsystem::SetMusicVolume(float Volume)
{
    MusicVolume = SanitizeVolume(Volume);
    bDirty = true;
    ApplyToWorld(GetWorld());
}

void UXQPAudioSettingsSubsystem::SetEffectsVolume(float Volume)
{
    EffectsVolume = SanitizeVolume(Volume);
    bDirty = true;
    ApplyToWorld(GetWorld());
}

void UXQPAudioSettingsSubsystem::SaveSettings()
{
    if (!bDirty || !GConfig) return;
    GConfig->SetBool(AudioSection, TEXT("MusicEnabled"), bMusicEnabled, GGameUserSettingsIni);
    GConfig->SetBool(AudioSection, TEXT("EffectsEnabled"), bEffectsEnabled, GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection, TEXT("EffectsVolume"), EffectsVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    bDirty = false;
}

void UXQPAudioSettingsSubsystem::Deinitialize()
{
    SaveSettings();
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    for (FAudioDeviceHandle& Device : AppliedDevices)
        if (Device.IsValid() && SettingsMix) Device->PopSoundMixModifier(SettingsMix);
    AppliedDevices.Reset();
    Super::Deinitialize();
}
