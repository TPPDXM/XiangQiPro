// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_Main_Base.generated.h"

typedef UUI_Main_Base UI_Main_Base;

class ACameraMainActor;

/**
 * 主场景基本UI
 */
UCLASS()
class XIANGQIPRO_API UUI_Main_Base : public UUserWidget
{
	GENERATED_BODY()

public:


	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ACameraMainActor> MainCamera;

public:

	void Init(TObjectPtr<ACameraMainActor> InCamera);
	
};
