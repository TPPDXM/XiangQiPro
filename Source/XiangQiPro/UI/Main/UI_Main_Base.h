// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_Main_Base.generated.h"

typedef UUI_Main_Base UI_Main_Base;

class ACameraMainActor;

/**
 * 主场景基本UI
 */
UCLASS()
class XIANGQIPRO_API UUI_Main_Base : public UXQPMobileWidget
{
	GENERATED_BODY()

public:


	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ACameraMainActor> MainCamera;

public:

	void Init(TObjectPtr<ACameraMainActor> InCamera);
protected:
    virtual void NativeConstruct() override;
private:
    UFUNCTION() void OpenClassic();
    UFUNCTION() void OpenPro();
    UFUNCTION() void OpenSettings();
    UFUNCTION() void ExitGame();
    void OpenMenu(const TCHAR* ClassPath);
};
