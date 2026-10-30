// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Util/DoOnce.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_TransitionScreen.generated.h"

DECLARE_DELEGATE(FOnFadeFinished);
DECLARE_DYNAMIC_DELEGATE(FOnFadeFinishedDynamic);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnFadingDelegate, float, value);

typedef UUI_TransitionScreen UI_TransitionScreen;

class UUIManager;
class UImage;

/**
 * 转场用渐变UserWidget
 */
UCLASS()
class XIANGQIPRO_API UUI_TransitionScreen : public UUserWidget
{
	GENERATED_BODY()

private:

	FTimerHandle Handle_EndStay;

	FDoOnce<void()> PlayTransition;

	FDoOnce<void()> Reverse;

	TWeakObjectPtr<UUIManager> UIManager;

	FVector2D FadeAlpha;

	bool bReverse = false;
    bool bFadeFinished = false;

	float FadeTime;

	float NewFadeTime;

	float StayTime;

	FLinearColor FadeColor;

	FOnFadeFinished Delegate = FOnFadeFinished();

	FOnFadeFinishedDynamic Delegate_DYNAMIC = FOnFadeFinishedDynamic();

	FOnFadingDelegate OnFadingDelegate = FOnFadingDelegate();

public:

	UPROPERTY(meta = (BindWidget))
	UImage* Image_Background;

protected:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	// 移除界面时清理定时器和回调。
	virtual void NativeDestruct() override;

public:
	// 更新界面每帧状态。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	* @brief 初始化转场屏幕
	* @param InUIManager 用户界面管理器
	* @param FromAlpha 初始透明度
	* @param ToAlpha 结束透明度
	* @param InFadeTime 淡化时间
	* @param InFadeColor 淡化颜色
	* @param InStayTime 淡化停留时间
	* @param bInReverse 倒放恢复
	* @return 两数之和
	*/
	void Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, float InStayTime = 0, bool bInReverse = false);

	/**
	* @brief 初始化转场屏幕
	* @param InUIManager 用户界面管理器
	* @param FromAlpha 初始透明度
	* @param ToAlpha 结束透明度
	* @param InFadeTime 淡化时间
	* @param InFadeColor 淡化颜色
	* @param InDelegate 淡化结束回调
	* @param InStayTime 淡化停留时间
	* @param bInReverse 倒放恢复
	*/
	void Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, FOnFadeFinished InDelegate, float InStayTime = 0, bool bInReverse = false);
	// 配置过渡透明度、时长、颜色和结束通知。
	UFUNCTION(BlueprintCallable)
	UUI_TransitionScreen* Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, FOnFadeFinishedDynamic InDelegate, float InStayTime = 0, bool bInReverse = false);
	// 设置过渡动画的进度通知。
	UFUNCTION(BlueprintCallable)
	void SetOnFadingDelegate(FOnFadingDelegate InDelegate);

	/* 结束淡化停留回调 */
	UFUNCTION()
	void EndStay();
	// 完成过渡并执行结束回调。
	void EndFade();

};
