// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "../Interface/IF_GameState.h"

#include "../Util/ObjectManager.h"
#include "../Util/ChessInfo.h"
#include "../Util/ChessMove.h"

#include "../GameObject/ChessBoard2P.h"

#include "../GameMode/XQPGameStateBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/TimelineComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Chesses.generated.h"

#define PATH_SM_CHESS TEXT("/Script/Engine.StaticMesh'/Game/Mesh/Chess/SM_Chess.SM_Chess'")
#define PATH_M_CHESSMASK TEXT("/Script/Engine.Material'/Game/Material/Decal/ChessMask/M_ChessMask.M_ChessMask'")
#define PATH_MI_STROKE_CHESS TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Material/Chess/MI_Stroke_Chess.MI_Stroke_Chess'")
#define PATH_CF_CHESSMOVE TEXT("/Script/Engine.CurveFloat'/Game/Curve/Chess/CF_ChessMove.CF_ChessMove'")
#define PATH_NS_FADE TEXT("/Script/Niagara.NiagaraSystem'/Game/Niagara/NiagaraSystem/NS_LeakParticles.NS_LeakParticles'")

struct FChessMove2P;

UCLASS()
class XIANGQIPRO_API AChesses : public APawn, public IIF_GameState
{
	GENERATED_BODY()

protected:

	// 棋子名字
	FString MyName;

	// 棋子颜色(阵营)
	EChessColor MyColor;

	EChessColor MyRealColor;

	// 棋子类型
	EChessType MyType;

	EChessType MyRealType;

	// 棋子当前的简化坐标
	Position Pos;

	// 目标移动的位置
	Position TargetPos;

	AXQPGameStateBase* GameState;

	TWeakObjectPtr<UChessBoard2P> Board2P;

	bool bAnimationing = false;

	// 棋子选中状态
	bool bSelected = false;

	// 棋子可以被选中
	bool bSelectable = true;

	bool bDead = false;

	UCurveFloat* CF_ChessMove;

public:
	// Sets default values for this pawn's properties
	AChesses();

	virtual void Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P);
	 
	// 棋子静态网格体组件
	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* ChessMesh;

	// 棋子上文字的贴花组件
	UPROPERTY(EditAnywhere)
	UDecalComponent* ChessMask;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* MI_ChessMask;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* MI_Stroke;

	UPROPERTY(EditAnywhere)
	UNiagaraComponent* FadeNiagara;

	UPROPERTY(EditAnywhere)
	UTimelineComponent* TimeLine_ChessMove;

	UPROPERTY(EditAnywhere)
	UTimelineComponent* Timeline_Fade;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GamePlayAgain(UObject* OwnerObject) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// 点击事件处理函数
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;

	virtual void NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex) override;

	// 鼠标悬停
	virtual void NotifyActorBeginCursorOver() override;

	// 鼠标离开
	virtual void NotifyActorEndCursorOver() override;

	virtual void NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex) override;

	// 游戏结束事件
	virtual void GameOver(UObject* OwnerObject) override;

	// 处理点击事件
	void HandleClick();

	// 棋子战败
	virtual void Defeated();

	// 获取棋子名称
	UFUNCTION(BlueprintPure, Category = "Chess")
	FString GetChessName() const;

	// 获取棋子颜色类型
	UFUNCTION(BlueprintPure, Category = "Chess")
	EChessColor GetColor() const;

	void SetColor(EChessColor InColor);

	// 获取棋子类型
	UFUNCTION(BlueprintPure, Category = "Chess")
	EChessType GetType() const;

	void SetType(EChessType InType);

	// 获取简化坐标
	Position GetPosition() const;

	// 获取是否阵亡
	bool IsDead() const;

	void UpdateSelectable();

	virtual void GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target);

	// 应用棋子移动
	virtual void ApplyMove(FChessMove2P Move);

	virtual void PlayMoveAnim();

	/*
	* 贝塞尔公式计算棋子移动位置
	* @param Start 起始世界坐标
	* @param Vertex 顶点世界坐标
	* @param End 终点世界坐标
	* @param T 曲线位置百分比
	*/
	FVector CalculateParabolicPosition(const FVector& Start, const FVector& Vertex, const FVector& End, float T);
	
	// 切换至真实身份，兵不厌诈游戏使用
	void SwitchToRealIdentity();
};
