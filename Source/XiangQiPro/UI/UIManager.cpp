// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UIManager.h"
#include <XiangQiPro/Util/Logger.h>
#include <Kismet/GameplayStatics.h>
#include <Blueprint/WidgetBlueprintLibrary.h>

// 设置基础界面和暂停界面来源，并清空显示栈。
void UUIManager::Init(UUserWidget* InBasicUI)
{
	ResetUI();

	BasicUI = InBasicUI;
	PauseUI.Reset();
	PauseWidgetClass = nullptr;

	if (IsValid(BasicUI))
		BasicUI->AddToPlayerScreen();
}

// 设置基础界面和暂停界面来源，并清空显示栈。
void UUIManager::Init(UUserWidget* InBasicUI, UUserWidget* InPauseUI)
{
	ResetUI();
	BasicUI = InBasicUI;
	PauseUI = InPauseUI;
	PauseWidgetClass = nullptr;
}

// 设置基础界面和暂停界面来源，并清空显示栈。
void UUIManager::Init(UUserWidget* InBasicUI, TSubclassOf<UUserWidget> InPauseWidgetClass)
{
	ResetUI();

	BasicUI = InBasicUI;
	PauseUI.Reset();
	PauseWidgetClass = InPauseWidgetClass;

	if (IsValid(BasicUI))
		BasicUI->AddToPlayerScreen();
}

// 将界面压入显示栈并按需隐藏上一层。
void UUIManager::AddUI(UUserWidget* InUI, bool bHideLastOne)
{
	if (!IsValid(InUI) || ui_stack.Contains(InUI))
	{
		return; // 空指针保护
	}

	if (bHideLastOne)
	{
		if (!ui_stack.IsEmpty())
		{
			SetUIVisibility(ui_stack.Top(), false); // 隐藏最上层UI
		}
		else
		{
			SetUIVisibility(BasicUI, false);		// 隐藏基本UI
		}
	}
	InUI->AddToPlayerScreen(); // 添加UI到玩家屏幕
	ui_stack.Add(InUI);		   // 把UI加入栈
}

// 移除指定界面并恢复上一层显示。
void UUIManager::RemoveUI(UUserWidget* TargetUI)
{
	if (!IsValid(TargetUI))
	{
		return; // 空指针保护
	}

	int32 Index; // 目标在UI栈中的索引
	if (!ui_stack.Find(TargetUI, Index)) // 如果目标不在栈中
	{
		return;
	}

	ui_stack[Index]->RemoveFromParent(); // 从屏幕中移除
	ui_stack.RemoveAt(Index); // 从栈中移除

	if (ui_stack.IsEmpty())
	{
		SetUIVisibility(BasicUI, true); // 重新展示基础UI
	}
	else
	{
		SetUIVisibility(ui_stack.Top(), true); // 重新显示上一层UI
	}
}

// 关闭顶层界面或打开暂停菜单。
void UUIManager::FinishUI()
{
	if (ui_stack.IsEmpty())
	{
		if (!PauseUI.IsValid() && IsValid(PauseWidgetClass))
		{
			PauseUI = CreateWidget<UUserWidget>(GetWorld(), PauseWidgetClass);
		}

		if (PauseUI.IsValid()) // 仅当暂停界面存在时才允许隐藏基础UI
		{
			SetUIVisibility(BasicUI, false); // 隐藏基础UI
			AddUI(PauseUI.Get()); // 添加暂停界面
			ULogger::Log(TEXT("OnGamePause"));
			EXEC_GAMEPAUSE(); // 调用暂停事件
		}
	}
	else
	{
		if (UUserWidget* pop_ui = ui_stack.Pop(); IsValid(pop_ui))
		{
			if (pop_ui == PauseUI)
			{
				EXEC_GAMERESUME(); // 调用恢复游戏事件
			}

			pop_ui->RemoveFromParent(); // 移除顶层UI
		}

		if (ui_stack.IsEmpty())
		{
			SetUIVisibility(BasicUI, true); // 重新展示基础UI
		}
		else
		{
			SetUIVisibility(ui_stack.Top(), true); // 重新显示上一层UI
		}
	}
}

// 统一设置界面交互和可见状态。
void UUIManager::SetUIVisibility(UUserWidget* ui, bool bVisible)
{
	if (IsValid(ui))
	{
		ui->SetIsEnabled(bVisible);
		ui->SetColorAndOpacity(bVisible ? FLinearColor(1, 1, 1, 1) : FLinearColor(0, 0, 0, 0));
	}
}

// 切换关卡前清理界面，避免GameInstance持有旧世界的Widget。
void UUIManager::ResetUI()
{
    for (const auto& Widget : ui_stack)
        if (IsValid(Widget)) Widget->RemoveFromParent();
    if (PauseUI.IsValid()) PauseUI->RemoveFromParent();
    if (IsValid(BasicUI)) BasicUI->RemoveFromParent();
    ui_stack.Reset();
    BasicUI = nullptr;
    PauseUI.Reset();
    PauseWidgetClass = nullptr;
}

// 释放子系统保留的界面资源。
void UUIManager::Deinitialize()
{
    ResetUI();
    Super::Deinitialize();
}
