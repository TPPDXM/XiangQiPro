// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "../UI/XQP_HUD.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Engine/UserInterfaceSettings.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "XiangQiPro/UI/Util/XQPLoadingScreen.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"

namespace
{
    TSharedRef<SWidget> LoadingSlateWidget(const UObject* Context)
    {
        // MoviePlayer bypasses AddToPlayerScreen and its automatic DPI scaling.
        const FVector2D Size = XQPMobile::ViewportSize(Context);
        const float Scale = Size.X > 0.f && Size.Y > 0.f
            ? GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y))) : 1.f;
        return SNew(SDPIScaler).DPIScale(Scale)[MakeXQPLoadingScreen()];
    }
}

// 初始化默认属性和组件。
UXQPGameInstance::UXQPGameInstance()
{
    static ConstructorHelpers::FClassFinder<UUI_TransitionScreen> Transition(TEXT("/Game/UMG/Util/BP_UI_TransitionScreen"));
    TravelTransitionClass = Transition.Class;
}

void UXQPGameInstance::OpenGameLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute, FString Options)
{
    if (XQPMobile::IsPortrait(WorldContextObject))
    {
        if (LevelName == TEXT("Main") || LevelName == TEXT("/Game/Map/Main")) LevelName = TEXT("/Game/Map/MobileMain");
        else if (LevelName == TEXT("Battle2P") || LevelName == TEXT("/Game/Map/Battle2P")) LevelName = TEXT("/Game/Map/MobileBattle");
    }
    auto* GI = Cast<UXQPGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!GI || !World) return;
    if (GI->bTravelPending || GI->bIsLoadingLevel) return;
    GI->bTravelPending = true;
    GI->PendingLevel = LevelName;
    GI->bPendingAbsolute = bAbsolute;
    GI->PendingOptions = MoveTemp(Options);
    // PIE does not send the initial PostLoadMap callback.
    if (World->HasBegunPlay() && !World->GetMapName().EndsWith(TEXT("Entry"))) GI->bHasLoadedInitialMap = true;
    UGameplayStatics::SetGamePaused(WorldContextObject, false);
    // Classic mode already fades to black before it calls this entry point.
    TArray<UUserWidget*> Existing;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, Existing, UUI_TransitionScreen::StaticClass(), true);
    for (auto* Widget : Existing)
        if (auto* Fade = Cast<UUI_TransitionScreen>(Widget); Fade && Fade->Image_Background && Fade->Image_Background->GetColorAndOpacity().A >= .99f)
        {
            // The existing fade removes itself after its callback. Hold a black frame
            // above it until travel begins on the next tick.
            GI->TravelTransition = CreateWidget<UUI_TransitionScreen>(World, GI->TravelTransitionClass);
            if (GI->TravelTransition)
            {
                GI->TravelTransition->Init(nullptr, 1.f, 1.f, .25f, FLinearColor::Black);
                GI->TravelTransition->AddToViewport(10000);
            }
            World->GetTimerManager().SetTimerForNextTick(GI, &UXQPGameInstance::PerformPendingTravel);
            return;
        }
    GI->TravelTransition = CreateWidget<UUI_TransitionScreen>(World, GI->TravelTransitionClass);
    if (!GI->TravelTransition)
    {
        GI->PerformPendingTravel();
        return;
    }
    GI->TravelTransition->Init(nullptr, 0.f, 1.f, .25f, FLinearColor::Black,
        FOnFadeFinished::CreateWeakLambda(GI, [GI]()
        {
            if (GI->GetWorld()) GI->GetWorld()->GetTimerManager().SetTimerForNextTick(GI, &UXQPGameInstance::PerformPendingTravel);
        }));
    GI->TravelTransition->AddToViewport(10000);
}

void UXQPGameInstance::PerformPendingTravel()
{
    if (bTravelPending && GetWorld()) UGameplayStatics::OpenLevel(this, PendingLevel, bPendingAbsolute, PendingOptions);
}

// 注册关卡加载事件并获取UI子系统。
void UXQPGameInstance::Init()
{
	Super::Init();
#if PLATFORM_ANDROID || PLATFORM_IOS
    PreventMobileSleep();
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UXQPGameInstance::AllowMobileSleep);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UXQPGameInstance::PreventMobileSleep);
#endif

	ULogger::Log(TEXT("UXQPGameInstance::Init: Game initializing..."));

	UIManager = GetSubsystem<UUIManager>(); // 获取UIManager实例
	if (!UIManager)
	{
		ULogger::LogError(TEXT("UXQPGameInstance::Init: Can't get UIManager instance!"));
	}

	//PreLoadMap，开始加载map
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UXQPGameInstance::BeginLoadMap);
	//PostLoadMapWithWorld， 加载完成后
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UXQPGameInstance::EndLoadMap);
}

// 开始加载关卡时配置过渡界面。
void UXQPGameInstance::BeginLoadMap(const FString& MapName)
{
    if ((MapName.Contains(TEXT("/Main")) || MapName.Contains(TEXT("/MobileMain"))) && GetSubsystem<UXQPLANSubsystem>()->GetState() != EXQPRoomState::Idle)
    {
        GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
        MyGameMode = EXQPGameMode::Default;
    }
	if (IsValid(UIManager)) UIManager->ResetUI();
	ULogger::Log(TEXT("Begin loading map..."));
	bIsLoadingLevel = true;
    if (TravelTransition) TravelTransition->RemoveFromParent();
    TravelTransition = nullptr;
    // Only the initial OpenMap load skips the loading screen. Leaving OpenMap
    // for either menu is already a subsequent map load.
    const bool bInitialOpenMap = !bHasLoadedInitialMap && FPackageName::GetShortName(MapName) == TEXT("OpenMap");
    if (bInitialOpenMap || IsRunningDedicatedServer())
    {
        ULogger::Log(TEXT("Startup map: skipping loading screen"));
        return;
    }

	FLoadingScreenAttributes LoadingScreen;

	LoadingScreen.bMoviesAreSkippable = false;
	LoadingScreen.bWaitForManualStop = false;
	LoadingScreen.bAllowEngineTick = false;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
	LoadingScreen.MinimumLoadingScreenDisplayTime = .35f;
	LoadingScreen.WidgetLoadingScreen = LoadingSlateWidget(this);
    ULogger::Log(TEXT("Showing XQPLoadingScreen"));

	if (GetMoviePlayer()) GetMoviePlayer()->SetupLoadingScreen(LoadingScreen); // 添加到屏幕
}

// 关卡加载结束后停止加载画面。
void UXQPGameInstance::EndLoadMap(UWorld* LoadedWorld)
{
	ULogger::Log(TEXT("End load map"));
	bIsLoadingLevel = false;
	if (GetMoviePlayer()) GetMoviePlayer()->StopMovie();
    bTravelPending = false;
    if (!LoadedWorld || LoadedWorld->GetGameInstance() != this) return;
    const bool bEntry = LoadedWorld->GetMapName().EndsWith(TEXT("Entry"));
    if (!bEntry) bHasLoadedInitialMap = true;
    const bool bPlayable = !LoadedWorld->GetMapName().EndsWith(TEXT("OpenMap")) && !bEntry;
    if (bPlayable) FadeInLoadedWorld();
}

void UXQPGameInstance::FadeInLoadedWorld()
{
    if (IsRunningDedicatedServer() || !GetWorld() || !GetWorld()->GetFirstPlayerController()) return;
    TravelTransition = CreateWidget<UUI_TransitionScreen>(GetWorld(), TravelTransitionClass);
    if (!TravelTransition) return;
    TravelTransition->Init(nullptr, 1.f, 0.f, .3f, FLinearColor::Black,
        FOnFadeFinished::CreateWeakLambda(this, [this]()
        {
            if (TravelTransition) TravelTransition->RemoveFromParent();
            TravelTransition = nullptr;
        }));
    TravelTransition->AddToViewport(10000);
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

void UXQPGameInstance::PreventMobileSleep() { UKismetSystemLibrary::ControlScreensaver(false); }
void UXQPGameInstance::AllowMobileSleep() { UKismetSystemLibrary::ControlScreensaver(true); }

void UXQPGameInstance::Shutdown()
{
    FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
#if PLATFORM_ANDROID || PLATFORM_IOS
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
    AllowMobileSleep();
#endif
    Super::Shutdown();
}
