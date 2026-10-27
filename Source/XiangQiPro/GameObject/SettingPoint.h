// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Interface/IF_GameState.h"
#include "XiangQiPro/Util/ChessMove.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SettingPoint.generated.h"

#define PATH_NS_SETTINGPOINT TEXT("/Script/Niagara.NiagaraSystem'/Game/Niagara/NiagaraSystem/NS_SettingPoint.NS_SettingPoint'")

class AChesses;
class AXQPGameStateBase;

class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;

UCLASS()
class XIANGQIPRO_API ASettingPoint : public APawn, public IIF_GameState
{
	GENERATED_BODY()

private:

	bool bActive = false;

	// 默认世界坐标
	FVector DefaultWorldPosition;

	// 简化坐标
	Position Position2P;

	TWeakObjectPtr<AXQPGameStateBase> GameState;

	// Niagara系统
	UPROPERTY()
	TObjectPtr<UNiagaraSystem> NS_SettingPoint = nullptr;

	// 目标移动的棋子
	TWeakObjectPtr<AChesses> TargetChess;

public:
	// Sets default values for this actor's properties
	ASettingPoint();

	UPROPERTY(EditAnywhere)
	UNiagaraComponent* PointNiagara;

	UPROPERTY(EditAnywhere)
	USphereComponent* Sphere;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	// 响应重开事件并清理上一局状态。
	virtual void GamePlayAgain(UObject* OwnerObject) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// 将触摸进入转换为悬停反馈。
	UFUNCTION()
	void OnInputTouchEnter(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent);
	// 将触摸离开转换为悬停结束反馈。
	UFUNCTION()
	void OnInputTouchLeave(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent);
	// 将触摸结束转换为落子点击。
	UFUNCTION()
	void OnInputTouchEnd(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent);

	// 统一的悬停进入处理
	UFUNCTION()
	void HandleHoverStart(UPrimitiveComponent* TouchedComponent);

	// 统一的悬停离开处理
	UFUNCTION()
	void HandleHoverEnd(UPrimitiveComponent* TouchedComponent);

	// 统一的点击处理
	UFUNCTION()
	void HandleClick(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	// 激活组件
	void SetActivate(bool bInActive);

	// 设置目标棋子
	void SetTargetChess(TWeakObjectPtr<AChesses> targetChess);

	// 获取目标棋子
	TWeakObjectPtr<AChesses> GetTargetChess();
	// 获取落子点棋盘坐标。
	FVector2D GetPosition2P();
	// 设置落子点棋盘坐标。
	void SetPosition2P(Position InPosition2P);

};
