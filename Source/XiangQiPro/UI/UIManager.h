// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "../Interface/IF_GameState.h"

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UIManager.generated.h"


/**
 * 玩家UI管理器, 从玩家屏幕添加或移除UI
 */
UCLASS()
class XIANGQIPRO_API UUIManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private:

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BasicUI;

	TWeakObjectPtr<UUserWidget> PauseUI;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> PauseWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> ui_stack;
	// 初始化默认属性和组件。
	UUIManager() : BasicUI(nullptr), PauseUI(nullptr), ui_stack() {};

	/*
	* 设置UI的可视性
	* @param ui UserWidget实例
	* @param bVisible 可视性
	*/
	void SetUIVisibility(UUserWidget* ui, bool bVisible);

public:

    UUserWidget* GetBasicUI() const { return BasicUI; }
    void CloseOverlays();
    void CloseDialogs(UUserWidget* Except = nullptr);

    // 切关前移除所有界面并释放强引用。
    void ResetUI();
    // 子系统结束时清理界面栈。
    virtual void Deinitialize() override;
	// 设置基础界面和暂停界面来源，并清空显示栈。
	void Init(UUserWidget* InBasicUI);
	// 设置基础界面和暂停界面来源，并清空显示栈。
	void Init(UUserWidget* InBasicUI, UUserWidget* InPauseUI);
	// 设置基础界面和暂停界面来源，并清空显示栈。
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void Init(UUserWidget* InBasicUI, TSubclassOf<UUserWidget> InPauseWidgetClass);

	/*
	* 添加UI到玩家屏幕
	* @param InUI 要添加到玩家屏幕的UserWidget实例
	* @param bHideLastOne 是否隐藏上一层UI
	*/
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void AddUI(UUserWidget* InUI, bool bHideLastOne = true);
	/*
	* 从玩家屏幕移除指定的UI
	* @param TargetUI 要从玩家屏幕移除的UserWidget实例
	*/
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void RemoveUI(UUserWidget* TargetUI);

	// 结束正在显示的UserWidget实例, 当仅剩基础界面显示时尝试添加暂停界面到屏幕
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void FinishUI();

};
