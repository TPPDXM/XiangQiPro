// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphicsSettings.h"
#include "UI_VideoSettings.generated.h"

class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;

/**
 * 视频设置界面
 */
UCLASS()
class XIANGQIPRO_API UUI_VideoSettings : public UGraphicsSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* DisplayMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* Resolution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* VSyncMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	USlider* ResolutionRatioSlider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* ResolutionRatioText;

protected:

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> DisplayModeList = {
				FString(UTF8_TO_TCHAR("全屏")),
				FString(UTF8_TO_TCHAR("窗口化全屏")),
				FString(UTF8_TO_TCHAR("窗口化")) };

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> VsyncModeList = {
				FString(UTF8_TO_TCHAR("禁用")),
				FString(UTF8_TO_TCHAR("启用")) };

	UPROPERTY(BlueprintReadOnly)
	TArray<TEnumAsByte<EWindowMode::Type>> WindowMode = {
		EWindowMode::Fullscreen,
		EWindowMode::WindowedFullscreen,
		EWindowMode::Windowed };

	UPROPERTY(BlueprintReadOnly)
	TArray<FIntPoint> Resolutions;
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;

private:

	// 初始化显示模式
	void InitDisplayMode();

	// 初始化分辨率设置
	void InitResolution();

	// 初始化垂直同步
	void InitVsync();

	// 初始化分辨率比例
	void InitResolutionRatio();
};
