// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_TransitionScreen.h"

#include "XiangQiPro/UI/XQP_HUD.h"

// 构造界面并初始化控件与事件绑定。
void UUI_TransitionScreen::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateCanTick();
    bFadeFinished = false;

	PlayTransition = FDoOnce<void()>([this]() {
		if (StayTime > 0 && GetWorld())
		{
			FTimerDelegate Delegate_EndStay;
			Delegate_EndStay.BindUObject(this, &UUI_TransitionScreen::EndStay);
			GetWorld()->GetTimerManager().SetTimer(Handle_EndStay, Delegate_EndStay, StayTime, false);
			return;
		}

		EndFade();
		});

	Reverse = FDoOnce<void()>([this](){
		NewFadeTime = 0;
		FadeAlpha = FVector2D(FadeAlpha.Y, FadeAlpha.X);
		PlayTransition.SetExecutable(true); // 重新启用
		});

	if (IsValid(Image_Background)) Image_Background->SetColorAndOpacity(FLinearColor(FadeColor.R, FadeColor.G, FadeColor.B, FadeAlpha.X));
}

// 移除界面时清理定时器和回调。
void UUI_TransitionScreen::NativeDestruct()
{
	// 清理资源
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(Handle_EndStay);
	}

	PlayTransition.Reset();
	Reverse.Reset();
	Super::NativeDestruct();
}

// 更新界面每帧状态。
void UUI_TransitionScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
    if (bFadeFinished) return;
	NewFadeTime += InDeltaTime;
    NewFadeTime = FMath::Min(NewFadeTime, FadeTime);
    // Paint the terminal black frame before delegates decide whether to travel.
    if (IsValid(Image_Background)) Image_Background->SetColorAndOpacity(FLinearColor(FadeColor.R, FadeColor.G, FadeColor.B,
        FMath::Lerp(FadeAlpha.X, FadeAlpha.Y, NewFadeTime / FadeTime)));
    OnFadingDelegate.ExecuteIfBound(NewFadeTime / FadeTime);
	if (NewFadeTime >= FadeTime)
	{
		NewFadeTime = FadeTime;

		// 屏幕转场完毕，执行回调函数
		if (PlayTransition.IsExecutable())
		{
			PlayTransition.Execute();
		}
	}

}

// 配置过渡透明度、时长、颜色和结束通知。
void UUI_TransitionScreen::Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, float InStayTime, bool bInReverse)
{
	UIManager = InUIManager;
	FadeAlpha = FVector2D(FromAlpha, ToAlpha);
	FadeTime = FMath::Max(InFadeTime, KINDA_SMALL_NUMBER);
	NewFadeTime = 0.f;
	FadeColor = InFadeColor;
	StayTime = InStayTime;
	bReverse = bInReverse;
}

// 配置过渡透明度、时长、颜色和结束通知。
void UUI_TransitionScreen::Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, FOnFadeFinished InDelegate, float InStayTime, bool bInReverse)
{
	UIManager = InUIManager;
	FadeAlpha = FVector2D(FromAlpha, ToAlpha);
	FadeTime = FMath::Max(InFadeTime, KINDA_SMALL_NUMBER);
	NewFadeTime = 0.f;
	FadeColor = InFadeColor;
	Delegate = InDelegate;
	StayTime = InStayTime;
	bReverse = bInReverse;
}

// 配置过渡透明度、时长、颜色和结束通知。
UUI_TransitionScreen* UUI_TransitionScreen::Init(UUIManager* InUIManager, float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, FOnFadeFinishedDynamic InDelegate, float InStayTime, bool bInReverse)
{
	UIManager = InUIManager;
	FadeAlpha = FVector2D(FromAlpha, ToAlpha);
	FadeTime = FMath::Max(InFadeTime, KINDA_SMALL_NUMBER);
	NewFadeTime = 0.f;
	FadeColor = InFadeColor;
	Delegate_DYNAMIC = InDelegate;
	StayTime = InStayTime;
	bReverse = bInReverse;
	return this;
}

// 设置过渡动画的进度通知。
void UUI_TransitionScreen::SetOnFadingDelegate(FOnFadingDelegate InDelegate)
{
	OnFadingDelegate = InDelegate;
}

// 停留结束后继续过渡流程。
void UUI_TransitionScreen::EndStay()
{
	StayTime = 0; // 清除停留时间
	EndFade();    // 执行淡化结束回调
}

// 完成过渡并执行结束回调。
void UUI_TransitionScreen::EndFade()
{
    if (bFadeFinished) return;
	if (bReverse && Reverse.IsExecutable())
	{
		Reverse.Execute(); // 执行倒放操作
		return;
	}
    bFadeFinished = true;
	Delegate.ExecuteIfBound();
	Delegate_DYNAMIC.ExecuteIfBound();

	if (UIManager.IsValid())
	{
		UIManager->RemoveUI(this); // 移除过渡界面
	}
}

