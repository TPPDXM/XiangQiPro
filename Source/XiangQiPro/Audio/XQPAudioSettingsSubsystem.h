// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "XQPAudioSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;

/**
 * 本机音频偏好及分类音量入口。
 * 使用独立音乐/音效 SoundClass 和长期 SoundMix，覆盖蓝图、UMG 与原生播放入口，
 * 不改写声音资源的原始音量，也不与场景转场时的淡入淡出倍率相互覆盖。
 */
UCLASS()
class XIANGQIPRO_API UXQPAudioSettingsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool IsMusicEnabled() const { return bMusicEnabled; }
    bool IsEffectsEnabled() const { return bEffectsEnabled; }
    float GetMusicVolume() const { return MusicVolume; }
    float GetEffectsVolume() const { return EffectsVolume; }

    /** 开关立即保存；关闭只改变混音增益，保留玩家先前选择的非零音量。 */
    void SetMusicEnabled(bool bEnabled);
    void SetEffectsEnabled(bool bEnabled);
    /** 滑动期间只更新内存和实时混音，拖动结束或页面离开时再统一写入配置。 */
    void SetMusicVolume(float Volume);
    void SetEffectsVolume(float Volume);
    void SaveSettings();

    /** 相机在首次播放前调用；切换地图或 PIE 使用新音频设备时也正确应用已保存偏好。 */
    void ApplyToWorld(UWorld* World);

private:
    bool bMusicEnabled = true;
    bool bEffectsEnabled = true;
    float MusicVolume = 1.f;
    float EffectsVolume = 1.f;
    bool bDirty = false;

    UPROPERTY(Transient) TObjectPtr<USoundClass> MusicClass;
    UPROPERTY(Transient) TObjectPtr<USoundClass> EffectsClass;
    UPROPERTY(Transient) TObjectPtr<USoundMix> SettingsMix;
    /** 同一设备跨地图只 Push 一次，避免每次旅行都叠加 SoundMix 的引用计数。 */
    TArray<FAudioDeviceHandle> AppliedDevices;
    FDelegateHandle MapLoadedHandle;

    bool LoadMixResources();
    void ApplyVolumes();
    static float SanitizeVolume(float Value);
};
