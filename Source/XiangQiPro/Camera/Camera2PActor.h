// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Interface/IF_GameState.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera2PActor.generated.h"

class AXQP_HUD;
class UUI_Battle2P_Base;
class UUI_InGamePause;

/**
 *
 */
UCLASS()
class XIANGQIPRO_API ACamera2PActor : public APawn, public IIF_GameState
{
	GENERATED_BODY()

private:



private:

    // 触摸状态
    bool bIsSingleTouchActive;
    bool bIsTwoFingerTouchActive;
    FVector2D InitialTouchLocation;
    FVector2D PreviousTouchLocation;

    // 双指触摸状态
    FVector2D FirstFingerLocation;
    FVector2D SecondFingerLocation;
    float InitialFingerDistance;

    // 目标值
    FRotator TargetRotation;

    bool bIsRotatingCamera;



public:
	// 初始化默认属性和组件。
	ACamera2PActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* GameCamera;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
    UAudioComponent* BattleAudio;

    UPROPERTY(EditAnywhere, Category = "Music")
    USoundBase* BattleMusic;

    UPROPERTY(EditAnywhere, Category = "Camera")
    float TargetArmLength = 90.f;

    // 基准FOV
    UPROPERTY(EditAnywhere, Category = "Camera")
    float BaseFOV = 90.f;

    // 平滑插值速度
    UPROPERTY(EditAnywhere, Category = "Touch Control")
    float RotationInterpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Touch Control")
    float ZoomInterpSpeed = 5.0f;

    // 鼠标滚轮缩放灵敏度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MouseWheelZoomSensitivity = 50.0f;

    // 触摸控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Control")
    float RotationSensitivity = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Control")
    float ZoomSensitivity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinZoomDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxZoomDistance = 200.0f;

    // 最小捏合距离，避免误触
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Control")
    float MinPinchDistance = 20.0f;

    // 旋转控制变量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float RotationSpeed = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinPitchAngle = -89.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxPitchAngle = -10.0f;

protected:
	// 进入场景后初始化本对象依赖。
	virtual void BeginPlay() override;
    // 离开场景时停止本对象的运行逻辑。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 初始化相机
    void InitCamera();

    // 初始化音频
    void InitAudio();

public:
    // 如果需要每帧更新，可以重写Tick函数
    virtual void Tick(float DeltaTime) override;
    // 响应游戏暂停事件。
    virtual void GamePause(UObject* OwnerObject) override;
    // 响应游戏恢复事件。
    virtual void GameResume(UObject* OwnerObject) override;
    // 绑定鼠标、触摸和相机输入事件。
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // 输入处理函数
    void OnMouseRightButtonPressed();
    // 停止相机旋转并恢复棋子点击。
    void OnMouseRightButtonReleased();
    // 根据鼠标水平位移更新相机偏航。
    void OnMouseX(float Value);
    // 根据鼠标垂直位移更新相机俯仰。
    void OnMouseY(float Value);
    // 鼠标滚轮缩放处理函数
    void OnMouseWheel(float Value);

    // 触摸输入处理
    void OnTouchBegin(ETouchIndex::Type FingerIndex, FVector Location);
    // 处理触摸移动，更新旋转或缩放。
    void OnTouchMove(ETouchIndex::Type FingerIndex, FVector Location);
    // 结束触摸并同步剩余手指状态。
    void OnTouchEnd(ETouchIndex::Type FingerIndex, FVector Location);

    // 双指手势处理
    void HandleTwoFingerGesture();

private:

    // 双指捏合距离是否合法
    bool IsValidTwoFingerGesture();

};
