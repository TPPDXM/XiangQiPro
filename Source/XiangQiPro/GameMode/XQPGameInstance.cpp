// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPGameInstance.h"
#include "../UI/XQP_HUD.h"

UXQPGameInstance::UXQPGameInstance()
{
}

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
			LoadingScreen.WidgetLoadingScreen = CurrentWidget->TakeWidget();
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

void UXQPGameInstance::BeginLoadMap(const FString& MapName)
{
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
			LoadingScreen.WidgetLoadingScreen = CurrentWidget->TakeWidget();
		}
		else
		{
			ULogger::LogError(TEXT("UXQPGameInstance::BeginLoadMap: LoadingWidget is nullptr!"));
		}
	}

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen); // 添加到屏幕
}

void UXQPGameInstance::EndLoadMap(UWorld* LoadedWorld)
{
	ULogger::Log(TEXT("End load map"));
	bIsLoadingLevel = false;
	GetMoviePlayer()->StopMovie();
	
}

void UXQPGameInstance::StopMovie()
{
	GetMoviePlayer()->StopMovie();
}

void UXQPGameInstance::SetGameMode(EXQPGameMode InGameMode)
{
	MyGameMode = InGameMode;
}

EXQPGameMode UXQPGameInstance::GetGameMode() const
{
	return MyGameMode;
}
