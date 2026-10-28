// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPGameInstance.h"
#include "../UI/XQP_HUD.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"

// 初始化默认属性和组件。
UXQPGameInstance::UXQPGameInstance()
{
}

// 注册关卡加载事件并获取UI子系统。
void UXQPGameInstance::Init()
{
	Super::Init();

	ULogger::Log(TEXT("UXQPGameInstance::Init: Game initializing..."));

	UIManager = GetSubsystem<UUIManager>(); // 获取UIManager实例
	if (!UIManager)
	{
		ULogger::LogError(TEXT("UXQPGameInstance::Init: Can't get UIManager instance!"));
	}

	FirstLoad = FDoOnce<void(FLoadingScreenAttributes&)>([this](FLoadingScreenAttributes& LoadingScreen) {
		// 第一次加载
		if (FirstLoadingWidget != nullptr)
		{
			CurrentWidget = CreateWidget<UI_TransitionScreen>(this, FirstLoadingWidget);

			if (UI_TransitionScreen* Widget = Cast<UI_TransitionScreen>(CurrentWidget))
			{
				Widget->Init(nullptr, 0, 1, 1, FLinearColor::White);	// 初始化淡化UI
			}
			if (IsValid(CurrentWidget)) LoadingScreen.WidgetLoadingScreen = CurrentWidget->TakeWidget();
		}
		else
		{
			ULogger::LogError(TEXT("UXQPGameInstance::Init: FirstLoadingWidget is nullptr!"));
		}});

	//PreLoadMap，开始加载map
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UXQPGameInstance::BeginLoadMap);
	//PostLoadMapWithWorld， 加载完成后
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UXQPGameInstance::EndLoadMap);
}

// 开始加载关卡时配置过渡界面。
void UXQPGameInstance::BeginLoadMap(const FString& MapName)
{
    if (MapName.Contains(TEXT("/Main")) && GetSubsystem<UXQPLANSubsystem>()->GetState() != EXQPRoomState::Idle)
    {
        GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
        MyGameMode = EXQPGameMode::Default;
    }
	if (IsValid(UIManager)) UIManager->ResetUI();
	ULogger::Log(TEXT("Begin loading map..."));
	bIsLoadingLevel = true;

	FLoadingScreenAttributes LoadingScreen;

	LoadingScreen.bMoviesAreSkippable = true;

	LoadingScreen.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget(); // movie不存在时，显示的widget

	if (FirstLoad.IsExecutable())
	{
		LoadingScreen.bWaitForManualStop = false;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
		LoadingScreen.MinimumLoadingScreenDisplayTime = 2.f; // movie最少播放时间
		FirstLoad.Execute(LoadingScreen); // 执行第一次加载(开屏加载)的界面显示
	}
	else
	{
		LoadingScreen.bWaitForManualStop = false;
		LoadingScreen.bAllowEngineTick = false;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
		LoadingScreen.MinimumLoadingScreenDisplayTime = 5.f; // movie最少播放时间
		// 非第一次加载
		if (LoadingWidget != nullptr)
		{
			CurrentWidget = CreateWidget<UI_LoadingScreen>(this, LoadingWidget);
			if (IsValid(CurrentWidget)) LoadingScreen.WidgetLoadingScreen = CurrentWidget->TakeWidget();
		}
		else
		{
			ULogger::LogError(TEXT("UXQPGameInstance::BeginLoadMap: LoadingWidget is nullptr!"));
		}
	}

	if (GetMoviePlayer()) GetMoviePlayer()->SetupLoadingScreen(LoadingScreen); // 添加到屏幕
}

// 关卡加载结束后停止加载画面。
void UXQPGameInstance::EndLoadMap(UWorld* LoadedWorld)
{
	ULogger::Log(TEXT("End load map"));
	bIsLoadingLevel = false;
	if (GetMoviePlayer()) GetMoviePlayer()->StopMovie();

}

// 停止当前加载影片。
void UXQPGameInstance::StopMovie()
{
	if (GetMoviePlayer()) GetMoviePlayer()->StopMovie();
}

// 设置下一局使用的玩法。
void UXQPGameInstance::SetGameMode(EXQPGameMode InGameMode)
{
	MyGameMode = InGameMode;
}

// 获取当前玩法。
EXQPGameMode UXQPGameInstance::GetGameMode() const
{
	return MyGameMode;
}
