// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "QualitySettingObject.h"

#include "CoreMinimal.h"
#include "GraphicsSettings.h"
#include "UI_QualitySettings.generated.h"

#define ADD_QUALITY_OPTION(QualityDisplayName, QualityName) \
UQualitySettingObject* QualityName = NewObject<UQualitySettingObject>(this);\
QualityName->Init(UserSettings->Get##QualityName##Quality(),\
				FText::FromString(UTF8_TO_TCHAR(QualityDisplayName)),\
				[WeakThis = TWeakObjectPtr<UUI_QualitySettings>(this)](int32 Index){\
					if (WeakThis.IsValid() && IsValid(WeakThis->UserSettings)) { WeakThis->UserSettings->Set##QualityName##Quality(Index); WeakThis->SaveAndApply(); } \
				});\
QualitySettingsList->AddItem(QualityName);

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
