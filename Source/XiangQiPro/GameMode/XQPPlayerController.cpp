// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPPlayerController.h"
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XQPGameStateBase.h"

// 进入场景后初始化本对象依赖。
void AXQPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 显示鼠标、启用点击事件、启用滑动事件
	this->bEnableClickEvents = true;
	this->bEnableMouseOverEvents = true;
	this->bShowMouseCursor = true;
	this->bEnableTouchEvents = true;
	this->bEnableTouchOverEvents = true;
}

// 绑定可在暂停期间使用的退出操作。
void AXQPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 绑定Esc按键
	if (IsValid(InputComponent)) InputComponent->BindAction(TEXT("Escape"), IE_Pressed, this, &AXQPPlayerController::OnEscapePressed).bExecuteWhenPaused = true;
}

// 切换顶层界面或暂停菜单。
void AXQPPlayerController::OnEscapePressed()
{
	if (!GetGameInstance()) return;
	ULogger::Log(TEXT("AXQPPlayerController::OnEscapePressed"));
	if (UUIManager* UIManager = GetGameInstance()->GetSubsystem<UUIManager>())
	{
		// 图标和Esc共用菜单栈；GameState在联机模式下不会暂停棋盘。
		UIManager->FinishUI();
	}
}
