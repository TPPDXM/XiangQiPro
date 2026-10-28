// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Util/DoOnce.h"
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/UI/Util/UI_LoadingScreen.h"
#include "XiangQiPro/UI/UIManager.h"
#include "MoviePlayer.h"

#include "CoreMinimal.h"
#include "ProBattleTypes.h"
#include "Engine/GameInstance.h"
#include "XQPGameInstance.generated.h"

UENUM()
enum class EXQPGameMode : uint8
{
	Default = 0,
	AI2P = 1,
	Ending = 2,
	SoloRide = 3,
	GuessWho = 4,
    LAN = 5,
    ProBattleAI = 6,
    ProBattleLAN = 7
};

/**
 * 游戏实例类
 */
UCLASS()
class XIANGQIPRO_API UXQPGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:

	FDoOnce<void(FLoadingScreenAttributes&)> FirstLoad;

	UPROPERTY()
	TObjectPtr<UUIManager> UIManager = nullptr;

	EXQPGameMode MyGameMode = EXQPGameMode::Default;

public:
	// 初始化默认属性和组件。
	UXQPGameInstance();

	UPROPERTY(BlueprintReadWrite)
	uint8 AIDifficulty = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pro Battle")
    FProBattleSettings ProSettings;

	UPROPERTY()
	UUserWidget* CurrentWidget;

	UPROPERTY(BlueprintReadOnly, Category = "MoviePlayer")
	bool bIsLoadingLevel = false;

	// loading widget
	UPROPERTY(EditAnywhere)
	TSubclassOf<UUI_LoadingScreen> LoadingWidget;

	// loading widget
	UPROPERTY(EditAnywhere)
	TSubclassOf<UUserWidget> FirstLoadingWidget;

public:
	// 注册关卡加载事件并获取UI子系统。
	virtual void Init() override;
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
