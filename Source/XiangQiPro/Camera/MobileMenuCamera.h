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
    // 手势位移按视口宽度归一化：横向环绕，纵向小幅倾斜，棋盘与演示棋子共用枢轴。
    void RotateBoard(const FVector2D& ScreenDelta);
protected:
    virtual void BeginPlay() override;
private:
    // 相机始终挂在 SceneRoot；只有棋盘、棋子及其贴花挂在这个展示枢轴下。
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneComponent> BoardPivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Board;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UMenuChessDemoComponent> Demo;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UAudioComponent> Music;
    UPROPERTY() TObjectPtr<class UTextBlock> Caption;
    FVector2D LastViewport = FVector2D::ZeroVector;
    FMargin LastInsets;
    FVector2D LensShift = FVector2D::ZeroVector;
    void FitBoard();
};
