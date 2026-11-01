// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_GameModeMenu.generated.h"

// 从玩法数据表创建通用条目；每个条目通过适配窗口进入专用玩法。
UCLASS()
class XIANGQIPRO_API UUI_GameModeMenu : public UXQPMobileWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(meta = (BindWidgetOptional))
	class UTileView* ModeList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UVerticalBox> ModeOptions;

    UPROPERTY(EditDefaultsOnly, Category = "Menu")
    TSubclassOf<class UUI_Item_GameMode> ModeItemClass;

public:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;

};
