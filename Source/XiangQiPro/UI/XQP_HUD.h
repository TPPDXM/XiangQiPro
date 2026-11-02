// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once
#include "XiangQiPro/Util/AssetPaths.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/ListView.h"

#include "InGame/UI_Battle2P_Base.h"
#include "InGame/UI_InGamePause.h"
#include "Main/UI_Main_Base.h"
#include "Util/UI_TransitionScreen.h"
#include "UIManager.h"

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "XQP_HUD.generated.h"


/**
 * 
 */
UCLASS()
class XIANGQIPRO_API AXQP_HUD : public AHUD
{
	GENERATED_BODY()
	
public:

	AXQP_HUD();

	TSubclassOf<UI_Battle2P_Base> Class_Battle2P_Base;

	TSubclassOf<UI_Main_Base> Class_Main_Base;

	TSubclassOf<UI_TransitionScreen> Class_TransitionScreen;

	TSubclassOf<UI_InGamePause> Class_InGamePause;

protected:

	virtual void BeginPlay() override;

	// 初始化用户界面
	void InitUI();
	
};
