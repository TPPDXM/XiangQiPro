// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_VideoSettings.h"
#include "XiangQiPro/UI/XQP_HUD.h"
#include <Kismet/KismetSystemLibrary.h>
#include <XiangQiPro/Util/Logger.h>

// 构造界面并初始化控件与事件绑定。
void UUI_VideoSettings::NativeConstruct()
{
	Super::NativeConstruct();
	if (UserSettings)
	{
		InitDisplayMode();
		InitResolution();
		InitVsync();
		InitResolutionRatio();
	}
}

// 填充窗口模式并选中当前设置。
void UUI_VideoSettings::InitDisplayMode()
{
	if (!IsValid(UserSettings) || !IsValid(DisplayMode)) return;
	for (const FString& displayMode : DisplayModeList)
	{
		DisplayMode->AddOption(displayMode);
	}

	// 将下拉列表定位到当前显示模式
	DisplayMode->SetSelectedIndex(WindowMode.Find(UserSettings->GetFullscreenMode()));
}

// 填充分辨率列表并选中当前设置。
void UUI_VideoSettings::InitResolution()
{
	if (!IsValid(UserSettings) || !IsValid(Resolution)) return;
	Resolutions.Empty();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	for (const FIntPoint& resolution : Resolutions)
	{
		// 获取所有可用的分辨率 例如 1920 × 1080
		FString option = FString();
		option.Append(FString::FromInt(resolution.X));
		option.Append(UTF8_TO_TCHAR("×"));
		option.Append(FString::FromInt(resolution.Y));
		Resolution->AddOption(option);
	}

	// 将下拉列表定位到当前分辨率
	Resolution->SetSelectedIndex(Resolutions.Find(UserSettings->GetScreenResolution()));
}

// 填充垂直同步选项并同步当前设置。
void UUI_VideoSettings::InitVsync()
{
	if (!IsValid(UserSettings) || !IsValid(VSyncMode)) return;
	for (const FString& vsyncMode : VsyncModeList)
	{
		VSyncMode->AddOption(vsyncMode);
	}
	VSyncMode->SetSelectedIndex(UserSettings->IsVSyncEnabled());
}

// 初始化分辨率缩放滑条和百分比。
void UUI_VideoSettings::InitResolutionRatio()
{
	if (!IsValid(UserSettings) || !IsValid(ResolutionRatioSlider) || !IsValid(ResolutionRatioText)) return;
	float CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue;

	UserSettings->GetResolutionScaleInformationEx(CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
	ResolutionRatioSlider->SetValue(CurrentScaleNormalized);

	int32 TextNum = CurrentScaleValue;
	ResolutionRatioText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), TextNum)));
}
