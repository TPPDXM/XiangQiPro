// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Templates/PimplPtr.h"
#include "Components/ActorComponent.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "MenuChessDemoComponent.generated.h"

struct FMenuChessGame
{
    FChessPosition Position;
    TArray<FChessMove2P> Moves;
    EChessColor FirstSide = EChessColor::REDCHESS;
};

struct FMenuScatterSimulation;

/** Deterministic menu replays with an isolated board-and-background scatter effect. */
UCLASS()
class XIANGQIPRO_API UMenuChessDemoComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMenuChessDemoComponent();
    virtual ~UMenuChessDemoComponent() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    static FMenuChessGame MakeGame(int32 Index);
    // 移动主界面传入可旋转的棋盘枢轴；未指定时保持原有演示的世界坐标行为。
    void SetPresentationRoot(class USceneComponent* InRoot) { PresentationRoot = InRoot; }
    // 快速旋转只触发一次整盘飞散；旋转速度使用世界空间的弧度/秒。
    void ScatterPieces(class UStaticMeshComponent* InBoard, const FVector& AngularVelocity, class UStaticMeshComponent* InBackground = nullptr);
    bool HasScatteredPieces() const { return bPiecesScattered; }
    // 平滑回正完成后才重建下一盘；普通旋转回正不会调用此方法。
    void AdvanceAfterScatter();
    float GetScatterTriggerSpeed() const { return ScatterTriggerSpeed; }

    // 参数保存在演示组件实例上，可在关卡中选中菜单相机的 Demo 组件调整。
    // 速度分别使用厘米/秒和弧度/秒；手势触发阈值使用角度/秒，便于策划调节。
    UPROPERTY(EditAnywhere, Category="棋子飞散|触发", meta=(DisplayName="飞散触发转速", ClampMin="0", Units="deg/s"))
    float ScatterTriggerSpeed = 540.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="棋盘切向速度继承比例", ClampMin="0"))
    float TangentialVelocityScale = .20f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="向外抛出速度范围", ClampMin="0", Units="cm/s"))
    FVector2D OutwardSpeedRange = FVector2D(8.f, 16.f);

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="沿棋盘法线抛起速度范围", ClampMin="0", Units="cm/s"))
    FVector2D LiftSpeedRange = FVector2D(20.f, 32.f);

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="初始飞散速度上限", ClampMin="0", Units="cm/s"))
    float InitialLinearSpeedLimit = 90.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="棋盘自转速度继承比例", ClampMin="0"))
    float InheritedSpinScale = .08f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="随机自转速度范围", ClampMin="0", Units="rad/s"))
    FVector2D RandomSpinSpeedRange = FVector2D(.8f, 2.f);

    UPROPERTY(EditAnywhere, Category="棋子飞散|初始速度", meta=(DisplayName="初始自转速度上限", ClampMin="0", Units="rad/s"))
    float InitialAngularSpeedLimit = 4.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|运动", meta=(DisplayName="飞行速度上限", ClampMin="0", Units="cm/s"))
    float FlightLinearSpeedLimit = 140.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|运动", meta=(DisplayName="飞行自转速度上限", ClampMin="0", Units="rad/s"))
    float FlightAngularSpeedLimit = 8.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|运动", meta=(DisplayName="下落重力加速度（厘米/秒²）", ClampMin="0"))
    float ScatterGravity = 300.f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|运动", meta=(DisplayName="平移阻尼", ClampMin="0"))
    float ScatterLinearDamping = .35f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|运动", meta=(DisplayName="自转阻尼", ClampMin="0"))
    float ScatterAngularDamping = .8f;

    UPROPERTY(EditAnywhere, Category="棋子飞散|碰撞", meta=(DisplayName="碰撞分离速度上限", ClampMin="0", Units="cm/s"))
    float MaxSeparationSpeed = 120.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY() TObjectPtr<class USceneComponent> PresentationRoot;
    // 棋谱坐标及走子弧线都保留在同一空间，旋转中或重置棋谱后也不会把棋子写回旧世界位置。
    void SetPieceLocation(class UStaticMeshComponent* Piece, const FVector& Location) const;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
    UPROPERTY() TArray<TObjectPtr<class UDecalComponent>> Labels;
    UPROPERTY() TObjectPtr<class UStaticMesh> PieceMesh;
    // 菜单独立的 Chaos 刚体场景：棋子用模型凸壳，棋盘和背景用模型三角面。
    // 可见组件仍不加入世界物理；只注册棋子与两个表面的碰撞对，不占项目碰撞通道。
    TPimplPtr<FMenuScatterSimulation> ScatterSimulation;
    TWeakObjectPtr<class UStaticMeshComponent> ScatterBoard;
    TWeakObjectPtr<class UStaticMeshComponent> ScatterBackground;
    bool bPiecesScattered = false;
    void TickScatteredPieces(float DeltaTime);
    FMenuChessGame Game;
    FChessPosition Position;
    FChessMove2P Moving;
    int32 Episode = 0;
    int32 Ply = 0;
    float Elapsed = 0.f;
    float Wait = 2.5f;
    bool bMoving = false;
    bool bFinished = false;
    EChessColor Side = EChessColor::REDCHESS;
    void ResetBoard();
    void ClearPieces();
    void FinishMove();
    FVector SquareLocation(FIntPoint Square) const;
};
