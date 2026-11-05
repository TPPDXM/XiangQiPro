// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_InGamePause.generated.h"

class UUI_CommonDialog;

typedef UUI_InGamePause UI_InGamePause;

/**
 * 对局中的暂停界面
 */
UCLASS()
class XIANGQIPRO_API UUI_InGamePause : public UXQPMobileWidget
{
	GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
private:
    // 返回转场开始后保留最后一帧菜单布局，不能再用已清理的玩法/房间状态刷新按钮。
    bool bLeavingToMainMenu = false;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> RestartButton;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> MainMenuButton;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> ResignButton;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> UndoButton;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> EditArrangementButton;
    UFUNCTION() void UndoMove();
    UFUNCTION() void EditArrangement();
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UUI_CommonDialog> CommonDialogClass;
    TWeakObjectPtr<UUI_CommonDialog> ResignDialog;
    UFUNCTION() void RestartOrShowResult();
    UFUNCTION() void LeaveToMainMenu();
    UFUNCTION() void AskToResign();
    void ConfirmResign();
    void RefreshActions();
};
