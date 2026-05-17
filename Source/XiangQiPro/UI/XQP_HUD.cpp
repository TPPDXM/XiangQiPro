// Copyright 2026 Ultimate Player All Rights Reserved.

#include "XQP_HUD.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Util/ObjectManager.h"

AXQP_HUD::AXQP_HUD()
{
	Class_Battle2P_Base = OM::GetConstructorBlueprint<UI_Battle2P_Base>(PATH_UI_BATTLE2P_BASE);
	Class_Main_Base = OM::GetConstructorBlueprint<UI_Main_Base>(PATH_UI_MAIN_BASE);
	Class_TransitionScreen = OM::GetConstructorBlueprint<UI_TransitionScreen>(PATH_UI_TRANSITIONSCREEN);
	Class_InGamePause = OM::GetConstructorBlueprint<UI_InGamePause>(PATH_UI_INGAMEPAUSE);
}

void AXQP_HUD::BeginPlay()
{
	Super::BeginPlay();
	InitUI();
}

void AXQP_HUD::InitUI()
{
	UXQPGameInstance* GameInstance = Cast<UXQPGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}


	UUserWidget* BaseUI = nullptr;
	TSubclassOf<UUserWidget> PauseMenuClass;
	switch (GameInstance->GetGameMode())
	{
	case EXQPGameMode::SoloRide:
	case EXQPGameMode::Ending:
	case EXQPGameMode::AI2P:
		BaseUI = CreateWidget<UI_Battle2P_Base>(GetWorld(), Class_Battle2P_Base);
		PauseMenuClass = Class_InGamePause;

		if (AXQPGameStateBase* _GS = Cast<GS>(GetWorld()->GetGameState()))
		{
			_GS->SetHUD2P(Cast<UUI_Battle2P_Base>(BaseUI)); // 把HUD交给游戏状态
		}
		else
		{
			ULogger::LogError(TEXT("AGameMode_Ending::InitUI: GameState is nullptr!"));
		}
		break;
	default:
		break;
	}

	if (!BaseUI)
	{
		return;
	}

	if (UUIManager* UIManager = GameInstance->GetSubsystem<UUIManager>())
	{
		UIManager->Init(BaseUI, PauseMenuClass); // 初始化用户界面管理器
	}
}
