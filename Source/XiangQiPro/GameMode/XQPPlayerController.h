// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "XQPPlayerController.generated.h"

typedef AXQPPlayerController PC;

/**
 *
 */
UCLASS()
class XIANGQIPRO_API AXQPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 进入场景后初始化本对象依赖。
	virtual void BeginPlay() override;
	// 绑定可在暂停期间使用的退出操作。
	virtual void SetupInputComponent() override;
	// 切换顶层界面或暂停菜单。
	UFUNCTION()
	void OnEscapePressed();
};
