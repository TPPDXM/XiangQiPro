// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Util/DoOnce.h"
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/UI/Util/UI_TransitionScreen.h"
#include "XiangQiPro/UI/UIManager.h"
#include "MoviePlayer.h"

#include "CoreMinimal.h"
#include "FengHuoBattleTypes.h"
#include "XQPGameTypes.h"
#include "Engine/GameInstance.h"
#include "XQPGameInstance.generated.h"

/**
 * 游戏实例类
 */
UCLASS()
class XIANGQIPRO_API UXQPGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:

	UPROPERTY()
	TObjectPtr<UUIManager> UIManager = nullptr;

    bool bHasLoadedInitialMap = false;
    bool bTravelPending = false;
    bool bPendingAbsolute = true;
    FName PendingLevel;
    FString PendingOptions;
    UPROPERTY(Transient) TObjectPtr<UUI_TransitionScreen> TravelTransition;
    UPROPERTY() TSubclassOf<UUI_TransitionScreen> TravelTransitionClass;
    void PerformPendingTravel();
    void FadeInLoadedWorld();

	EXQPGameMode MyGameMode = EXQPGameMode::Default;

public:
	// 初始化默认属性和组件。
	UXQPGameInstance();

	UPROPERTY(BlueprintReadWrite)
	uint8 AIDifficulty = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FengHuo Battle")
    FFengHuoSettings FengHuoSettings;

	UPROPERTY(BlueprintReadOnly, Category = "MoviePlayer")
	bool bIsLoadingLevel = false;

public:
	// 注册关卡加载事件并获取UI子系统。
	virtual void Init() override;
	virtual void Shutdown() override;
    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", AdvancedDisplay="2"), Category="Game")
    static void OpenGameLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute = true, FString Options = TEXT(""));
	void AllowMobileSleep();
	void PreventMobileSleep();
	// 开始加载关卡时配置过渡界面。
	void BeginLoadMap(const FString& MapName);
	// 关卡加载结束后停止加载画面。
	void EndLoadMap(UWorld* LoadedWorld);
	// 停止当前加载影片。
	UFUNCTION(BlueprintCallable, Category = "MoviePlayer")
	void StopMovie();
	// 设置下一局使用的玩法。
	UFUNCTION(BlueprintCallable)
	void SetGameMode(EXQPGameMode InGameMode);
	// 获取当前玩法。
	UFUNCTION(BlueprintCallable)
	EXQPGameMode GetGameMode() const;

};
