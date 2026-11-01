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
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> WelcomeButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> WelcomeText;
    FString DisplayedPlayerName;
    // 欢迎入口与主菜单标题共享入场时机，移动模板无需创建此控件。
    void RefreshWelcome();
    UFUNCTION() void EditPlayerName();
    UFUNCTION() void OpenClassic();
    UFUNCTION() void OpenGameModes();
    UFUNCTION() void OpenSettings();
    UFUNCTION() void ExitGame();
    void OpenMenu(const TCHAR* ClassPath);
};
