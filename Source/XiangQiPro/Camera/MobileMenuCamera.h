// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobileMenuCamera.generated.h"

/** Portrait menu composition, independent of the desktop entrance and mouse camera. */
UCLASS()
class XIANGQIPRO_API AMobileMenuCamera : public AActor
{
    GENERATED_BODY()
public:
    AMobileMenuCamera();
    virtual void Tick(float DeltaSeconds) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    // 以实际棋盘包围盒判断触摸起点，标题、按钮和棋盘外的滑动不会启动旋转。
    bool IsBoardTouchPosition(const FVector2D& ScreenPosition) const;
    // 手势按屏幕轴自由旋转，不限制横向或纵向翻转角度；棋子共用展示枢轴。
    void RotateBoard(const FVector2D& ScreenDelta);
    void BeginBoardInteraction();
    void EndBoardInteraction(bool bAllowInertia);
protected:
    virtual void BeginPlay() override;
private:
    // 相机始终挂在 SceneRoot；只有棋盘、棋子及其贴花挂在这个展示枢轴下。
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneComponent> BoardPivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Board;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UMenuChessDemoComponent> Demo;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UAudioComponent> Music;
    FVector2D LastViewport = FVector2D::ZeroVector;
    FMargin LastInsets;
    FVector2D LensShift = FVector2D::ZeroVector;
    // 回正只改变展示枢轴，镜头保持稳定；没有飞散时完整保留棋谱步号和走子进度。
    TWeakObjectPtr<class UStaticMeshComponent> BackgroundSurface;
    FQuat HomeBoardRotation = FQuat::Identity;
    FQuat ReturnStartRotation = FQuat::Identity;
    FVector PendingAngularDisplacement = FVector::ZeroVector;
    FVector2D PendingInputDegrees = FVector2D::ZeroVector;
    FVector2D InertialVelocity = FVector2D::ZeroVector;
    bool bBoardHeld = false;
    float PendingRotationRadians = 0.f;
    float RotationIdleSeconds = 0.f;
    float ReturnSeconds = 0.f;
    bool bHasUserRotation = false;
    bool bReturningBoard = false;
    void ApplyBoardRotation(const FVector2D& AngleDegrees);
    void ConfigureBoardClearance();
    void UpdateBoardInteraction(float DeltaSeconds);
    void FitBoard();
};
