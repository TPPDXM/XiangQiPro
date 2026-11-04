// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "XQPPlayerController.generated.h"

typedef AXQPPlayerController PC;

/**
 *
 */
UCLASS()
class XIANGQIPRO_API AXQPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 进入场景后初始化本对象依赖。
	virtual void BeginPlay() override;
	// 绑定可在暂停期间使用的退出操作。
	virtual void SetupInputComponent() override;
	// 切换顶层界面或暂停菜单。
	UFUNCTION()
	void OnEscapePressed();
    virtual void PlayerTick(float DeltaTime) override;

protected:
    // 在 PlayerInput 已更新接触状态之后接管移动棋盘触摸，避免 Actor 的旧抬手回调重复提交。
    virtual void ProcessTouchHitResult(FTouchId TouchId, ETouchType::Type Type, const FVector2D& TouchLocation,
        const FHitResult& HitResult, bool bHit) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 一次手势始终绑定同一个接触、源棋子和源格；多指加入后整次手势作废，直到所有手指抬起。
    TSet<FTouchId> BoardContacts;
    TOptional<FTouchId> ActiveBoardContact;
    TWeakObjectPtr<class AChesses> TouchChess;
    TWeakObjectPtr<class ASettingPoint> HoveredDropPoint;
    TWeakObjectPtr<class AMobileMenuCamera> TouchMenu;
    FIntPoint TouchSource = FIntPoint(-1, -1);
    FVector2D TouchStart = FVector2D::ZeroVector;
    FVector2D PreviousBoardTouch = FVector2D::ZeroVector;
    TSet<FTouchId> MenuBackgroundContacts;
    bool bBoardTouchMoved = false;
    bool bSelectedBeforeTouch = false;
    bool bTouchBeganOnPiece = false;
    bool bRejectBoardContacts = false;

    bool CanUseMobileBoardInput() const;
    // 用棋盘平面反投影到格子，再读取当前激活落点；吃子目标的网格不会遮住落点判定。
    class ASettingPoint* FindTouchDropPoint(const FVector2D& ScreenPosition) const;
    void SetHoveredDropPoint(class ASettingPoint* Point);
    void CancelMobileBoardGesture(bool bAllowMenuInertia = false);
};
