// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once
#include "XiangQiPro/Util/AssetPaths.h"

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


struct FChessMove2P;
class UChessMoveEffectsComponent;

UCLASS()
class XIANGQIPRO_API AChesses : public APawn, public IIF_GameState
{
	GENERATED_BODY()

protected:

	// 棋子名字
	FString MyName;

	// 棋子颜色(阵营)
	EChessColor MyColor = EChessColor::REDCHESS;

	EChessColor MyRealColor = EChessColor::REDCHESS;

	// 棋子类型
	EChessType MyType = EChessType::EMPTY;

	EChessType MyRealType = EChessType::EMPTY;

	// 棋子当前的简化坐标
	Position Pos;

	// 目标移动的位置
	Position TargetPos;

	TWeakObjectPtr<AXQPGameStateBase> GameState;

	TWeakObjectPtr<UChessBoard2P> Board2P;

	bool bAnimationing = false;

	// 棋子选中状态
	bool bSelected = false;

	// 棋子可以被选中
	bool bSelectable = true;

	bool bDead = false;

    // 暂停保存播放方向与 Tick 状态，恢复时从同一位置继续，不能 PlayFromStart。
    bool bMovementPaused = false;
    bool bResumeActorTick = false;
    bool bResumeMoveTimeline = false;
    bool bResumeFadeTimeline = false;
    bool bReverseMoveTimeline = false;
    bool bReverseFadeTimeline = false;
    TArray<TWeakObjectPtr<UNiagaraComponent>> PausedMoveEffects;
    bool bFengHuoMode = false;
    bool bRealtimeTravelling = false;
    bool bCaptureFadeComplete = false;
    float FengHuoMoveTime = 0.f;
    float FengHuoMoveDuration = 1.f;
    float CaptureDeathTime = 0.f;
    FVector FengHuoMoveStart = FVector::ZeroVector;
    TWeakObjectPtr<AChesses> CaptureSupport;
    TWeakObjectPtr<AChesses> PendingCapture;
    bool bCaptureApproach = false;
    bool bCaptureLanding = false;
    float CaptureHoverRemaining = 0.f;
    float CaptureHoverHeight = 0.f;
    float CaptureHoverOffset = 0.f;
    FVector CaptureApproachLocation(AChesses* Victim, float Alpha) const;

	UPROPERTY()
	TObjectPtr<UCurveFloat> CF_ChessMove = nullptr;

public:
    /** 编辑器置棋使用独立动画，不提交走法、不切换回合、不触发胜负。 */
    void PlayArrangementSpawn();
private:
    bool bArrangementSpawn = false;
    float ArrangementSpawnTime = 0.f;
    FVector ArrangementTarget = FVector::ZeroVector;
public:
    void SetMovementPaused(bool bPaused);
	// Sets default values for this pawn's properties
	AChesses();

    /** 所有棋子继承的两段式特效配置；基类资源默认为空，派生棋子按需赋值。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chess|Effects")
    TObjectPtr<UChessMoveEffectsComponent> MoveEffects;
	// 初始化棋子身份、棋盘引用和动画回调。
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
	// 离开场景时停止本对象的运行逻辑。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 响应重开事件并清理上一局状态。
	virtual void GamePlayAgain(UObject* OwnerObject) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 点击事件处理函数
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;
	// 触摸结束时处理棋子选择。
	virtual void NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex) override;

	// 鼠标悬停
	virtual void NotifyActorBeginCursorOver() override;

	// 鼠标离开
	virtual void NotifyActorEndCursorOver() override;
	// 触摸开始时显示棋子反馈。
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
	// 修改棋子阵营并更新局面修订号。
	void SetColor(EChessColor InColor);

	// 获取棋子类型
	UFUNCTION(BlueprintPure, Category = "Chess")
	EChessType GetType() const;
	// 修改棋子类型并更新局面修订号。
	void SetType(EChessType InType);

	// 获取简化坐标
	Position GetPosition() const;

	// 获取是否阵亡
	bool IsDead() const;
    void EnableFengHuoMode();
    void PrepareCapture(AChesses* Victim) { PendingCapture = Victim; }
    void BeginFengHuoMove(const FChessMove2P& Move, float Duration);
    void ArriveFengHuoMove(FIntPoint Destination, AChesses* Captured);
    void StopFengHuoMove();
    bool IsRealtimeMoving() const { return bRealtimeTravelling; }
    bool IsSelectedForMove() const { return bSelected; }
    // 按下即选择并显示合法落点；已选棋子不会在按下时被切换为未选中。
    bool BeginTouchMoveSelection();
    void ClearMoveSelection();
    float GetSurfaceZ() const;
    /** 特效落点使用棋盘目标格的世界坐标，连续叠放吃子时冲击波也保持贴在棋盘表面。 */
    FVector GetEffectImpactLocation() const;
	// 根据玩法与阵营更新棋子的可选状态。
	void UpdateSelectable();
	// 生成选中棋子的合法走法并显示提示。
	virtual void GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target);

	// 应用棋子移动
	virtual void ApplyMove(FChessMove2P Move);
	// 播放落子动画，缺失曲线时直接完成落子。
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
