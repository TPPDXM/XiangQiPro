// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "QualitySettingObject.h"

#include "CoreMinimal.h"
#include "GraphicsSettings.h"
#include "UI_QualitySettings.generated.h"

class UListView;

/**
 * 画质设置菜单
 */
UCLASS()
class XIANGQIPRO_API UUI_QualitySettings : public UGraphicsSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(meta = (BindWidget))
	UListView* QualitySettingsList;

protected:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;

};
