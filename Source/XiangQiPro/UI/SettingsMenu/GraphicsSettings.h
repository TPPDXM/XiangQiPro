// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "GraphicsSettings.generated.h"

/**
 * 画面设置和视频设置的父类
 */
UCLASS()
class XIANGQIPRO_API UGraphicsSettings : public UXQPMobileWidget
{
	GENERATED_BODY()

protected:

	UPROPERTY(BlueprintReadOnly)
	UGameUserSettings* UserSettings;

protected:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	// 保存并应用用户画面设置。
	UFUNCTION(BlueprintCallable)
	void SaveAndApply();
};
