// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_ProModeMenu.generated.h"

/**
 *
 */
UCLASS()
class XIANGQIPRO_API UUI_ProModeMenu : public UXQPMobileWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(meta = (BindWidgetOptional))
	class UTileView* ModeList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UVerticalBox> ModeOptions;

    UPROPERTY(EditDefaultsOnly, Category = "Menu")
    TSubclassOf<class UUI_Item_ProMode> ModeItemClass;

public:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;

};
