// Copyright 2026 Ultimate Player All Rights Reserved.


#include "GraphicsSettings.h"

// 构造界面并初始化控件与事件绑定。
void UGraphicsSettings::NativeConstruct()
{
	Super::NativeConstruct();
	if (GEngine)
	{
		UserSettings = GEngine->GetGameUserSettings();
	}
}

// 保存并应用用户画面设置。
void UGraphicsSettings::SaveAndApply()
{
	if (!IsValid(UserSettings)) return;
	UserSettings->SaveSettings();
	UserSettings->ApplySettings(true);
}
