// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_ProModeMenu.generated.h"

/**
 *
 */
UCLASS()
class XIANGQIPRO_API UUI_ProModeMenu : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(meta = (BindWidget))
	class UTileView* ModeList;

public:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;

};
