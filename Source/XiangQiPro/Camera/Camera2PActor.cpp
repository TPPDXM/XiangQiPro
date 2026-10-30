// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Camera2PActor.h"
#include "../GameMode/XQPGameStateBase.h"
#include "../UI/XQP_HUD.h"
#include "../Util/Logger.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/GameObject/ChessBoard2PActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// 初始化默认属性和组件。
ACamera2PActor::ACamera2PActor()
{
    PrimaryActorTick.bCanEverTick = true;

	// 创建弹簧臂组件
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->TargetArmLength = 90.0f; // 摄像机距离
	SpringArm->bUsePawnControlRotation = false; // 重要：不使用Pawn控制旋转
	SpringArm->bEnableCameraLag = true; // 启用摄像机延迟
	SpringArm->CameraLagSpeed = 3.0f; // 延迟速度
	SpringArm->bDoCollisionTest = false; // 碰撞检测（可选）
	RootComponent = SpringArm;

	// 设置弹簧臂旋转限制
	SpringArm->SetRelativeRotation(FRotator(-40.0f, 0.0f, 0.0f));

	// 创建摄像机组件
	GameCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	GameCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// 创建音频组件
	BattleAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BattleAudio"));
	BattleAudio->SetupAttachment(GameCamera);
    BattleAudio->bAutoActivate = false;
    BattleAudio->bIsUISound = true;
    static ConstructorHelpers::FObjectFinder<USoundBase> Music(TEXT("/Game/Audio/BattleMusic"));
    BattleMusic = Music.Object;

	// 初始化变量
	bIsRotatingCamera = false;
	TargetRotation = FRotator::ZeroRotator;

	// 自动控制玩家
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

// 进入场景后初始化本对象依赖。
void ACamera2PActor::BeginPlay()
{
	Super::BeginPlay();

    InitCamera();
	InitAudio();
}

// 离开场景时停止本对象的运行逻辑。
void ACamera2PActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// 初始化相机的目标旋转。
void ACamera2PActor::InitCamera()
{
    bPortraitCamera = XQPMobile::IsPortrait(this);
	// 初始化目标旋转为当前旋转
	TargetRotation = SpringArm->GetRelativeRotation();
    // 黑方从自己的底线观察棋盘；保留已有的拖拽和缩放控制。
    const auto* GI = GetGameInstance<UXQPGameInstance>();
    if (GI && (GI->GetGameMode() == EXQPGameMode::LAN || GI->GetGameMode() == EXQPGameMode::ProBattleLAN) && GI->GetSubsystem<UXQPLANSubsystem>()->GetLocalColor() == EChessColor::BLACKCHESS)
    {
        TargetRotation.Yaw += 180.f;
        SpringArm->SetRelativeRotation(TargetRotation);
    }
    HomeRotation = TargetRotation;
    if (bPortraitCamera) EnablePortraitCamera();
}

void ACamera2PActor::EnablePortraitCamera()
{
        bPortraitCamera = true;
        HomeRotation.Pitch = TargetRotation.Pitch = -90.f;
        HomeRotation.Roll = TargetRotation.Roll = 0.f;
        SpringArm->bEnableCameraLag = false;
        SpringArm->bEnableCameraRotationLag = false;
        SpringArm->SocketOffset = FVector::ZeroVector;
        SpringArm->TargetOffset = FVector::ZeroVector;
        GameCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
        GameCamera->bConstrainAspectRatio = false;
        GameCamera->bOverrideAspectRatioAxisConstraint = true;
        GameCamera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
        GameCamera->SetProjectionMode(ECameraProjectionMode::Orthographic);
        GameCamera->bAutoCalculateOrthoPlanes = false;
        GameCamera->OrthoNearClipPlane = 0.1f;
        GameCamera->OrthoFarClipPlane = 10000.f;
        GameCamera->bUpdateOrthoPlanes = false;
        GameCamera->bUseCameraHeightAsViewTarget = false;
        // Perspective focus distances from the level must not blur the orthographic board.
        GameCamera->PostProcessSettings.bOverride_DepthOfFieldScale = true;
        GameCamera->PostProcessSettings.DepthOfFieldScale = 0.f;
        // Orthographic gameplay uses a stable exposure instead of physical lens parameters.
        GameCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
        GameCamera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        GameCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        GameCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
        GameCamera->PostProcessSettings.bOverride_BloomIntensity = true;
        GameCamera->PostProcessSettings.BloomIntensity = .15f;
        FitMobileBoard();
}

void ACamera2PActor::FitMobileBoard()
{
    const FVector2D Viewport = XQPMobile::ViewportSize(this);
    if (Viewport.X <= 0 || Viewport.Y <= 0) return;
    if (!MobileBoard.IsValid())
    {
        TActorIterator<AChessBoard2PActor> It(GetWorld());
        if (It) MobileBoard = *It;
    }
    if (!MobileBoard.IsValid() || !MobileBoard->ChessBoardMesh) return;
    const FBox Bounds = MobileBoard->ChessBoardMesh->Bounds.GetBox();
    if (!Bounds.IsValid || Bounds.GetExtent().IsNearlyZero()) return;
    const FVector Center = Bounds.GetCenter() + FVector(0, 0, Bounds.GetExtent().Z + 2.f);
    const FBox2D Rect = XQPMobile::BoardRect(this);
    const double Aspect = Viewport.X / Viewport.Y;
    TargetRotation = HomeRotation;
    TargetRotation.Pitch = -90.f;
    SpringArm->SetWorldRotation(TargetRotation);
    const FVector Right = FRotationMatrix(TargetRotation).GetScaledAxis(EAxis::Y);
    const FVector Up = FRotationMatrix(TargetRotation).GetScaledAxis(EAxis::Z);
    const FVector Extent = Bounds.GetExtent();
    // Project the bounds onto the current camera axes, including the black-side view.
    const double HalfWidth = FVector::DotProduct(Right.GetAbs(), Extent) + .75f;
    const double HalfHeight = FVector::DotProduct(Up.GetAbs(), Extent) + .75f;
    const double OrthoWidth = FMath::Max(2.0 * HalfWidth * Viewport.X / Rect.GetSize().X,
        2.0 * HalfHeight * Viewport.X / Rect.GetSize().Y);
    GameCamera->SetOrthoWidth(OrthoWidth);
    const FVector2D ScreenCenter = Rect.GetCenter() / Viewport;
    SetActorLocation(Center - Right * ((2.f * ScreenCenter.X - 1.f) * OrthoWidth * .5)
        - Up * ((1.f - 2.f * ScreenCenter.Y) * OrthoWidth * .5 / Aspect));
    TargetArmLength = FMath::Max(90.f, Extent.Z + 20.f);
    GameCamera->OrthoFarClipPlane = FMath::Max(500.f, TargetArmLength + Extent.Z * 2.f + 100.f);
    SpringArm->TargetArmLength = TargetArmLength;
    LastMobileViewport = Viewport;
    LastMobileInsets = XQPMobile::SafeInsets(this);
}

void ACamera2PActor::PlayProResultCamera()
{
    if (bPortraitCamera) { bProResultCamera = true; FitMobileBoard(); return; }
    bProResultCamera = true;
    bIsRotatingCamera = bIsSingleTouchActive = bIsTwoFingerTouchActive = false;
    TargetRotation = HomeRotation;
    TargetRotation.Pitch = -58.f;
    TargetRotation.Roll = 0.f;
    TargetArmLength = FMath::Clamp(135.f, MinZoomDistance, MaxZoomDistance);
    OnMouseRightButtonReleased();
    DisableInput(GetController<APlayerController>());
}

void ACamera2PActor::GamePlayAgain(UObject* OwnerObject)
{
    if (const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>(); State && State->IsProBattle())
    {
        bProResultCamera = false;
        TargetRotation = HomeRotation;
        if (bPortraitCamera) FitMobileBoard();
        else TargetArmLength = 90.f;
        EnableInput(GetController<APlayerController>());
    }
    IIF_GameState::GamePlayAgain(OwnerObject);
}

// 设置并播放场景背景音乐。
void ACamera2PActor::InitAudio()
{
    if (!IsValid(BattleAudio)) return;
    BattleAudio->SetSound(BattleMusic);
    if (BattleMusic) BattleAudio->Play();
}

// 如果需要每帧更新，可以重写Tick函数
void ACamera2PActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    if (!bPortraitCamera && XQPMobile::IsPortrait(this)) EnablePortraitCamera();
    if (bPortraitCamera)
    {
        if (!MobileBoard.IsValid() || !LastMobileViewport.Equals(XQPMobile::ViewportSize(this)) || LastMobileInsets != XQPMobile::SafeInsets(this)) FitMobileBoard();
        return;
    }

	// 平滑应用旋转
	if (!TargetRotation.Equals(SpringArm->GetRelativeRotation()))
	{
		FRotator CurrentRotation = SpringArm->GetRelativeRotation();
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 8.0f);
		SpringArm->SetRelativeRotation(NewRotation);
	}

	// 平滑插值缩放
	if (!FMath::IsNearlyEqual(SpringArm->TargetArmLength, TargetArmLength))
	{
		float CurrentLength = SpringArm->TargetArmLength;
		float NewLength = FMath::FInterpTo(CurrentLength, TargetArmLength, DeltaTime, ZoomInterpSpeed);
		SpringArm->TargetArmLength = NewLength;
	}
}

// 响应游戏暂停事件。
void ACamera2PActor::GamePause(UObject* OwnerObject)
{
	// 屏蔽输入
	DisableInput(GetController<APlayerController>());
	IIF_GameState::GamePause(OwnerObject);
}

// 响应游戏恢复事件。
void ACamera2PActor::GameResume(UObject* OwnerObject)
{
	// 启用输入
    if (!bProResultCamera) EnableInput(GetController<APlayerController>());
	IIF_GameState::GameResume(OwnerObject);
}

// 绑定鼠标、触摸和相机输入事件。
void ACamera2PActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!IsValid(PlayerInputComponent)) return;

	// 绑定鼠标按键事件
	PlayerInputComponent->BindAction("CameraRotate", IE_Pressed, this, &ACamera2PActor::OnMouseRightButtonPressed);
	PlayerInputComponent->BindAction("CameraRotate", IE_Released, this, &ACamera2PActor::OnMouseRightButtonReleased);

	// 绑定鼠标轴事件
	PlayerInputComponent->BindAxis("MouseX", this, &ACamera2PActor::OnMouseX);
	PlayerInputComponent->BindAxis("MouseY", this, &ACamera2PActor::OnMouseY);

	// 绑定鼠标滚轮输入
	PlayerInputComponent->BindAxis("MouseWheel", this, &ACamera2PActor::OnMouseWheel);

	// 绑定触摸输入
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ACamera2PActor::OnTouchBegin);
	PlayerInputComponent->BindTouch(IE_Repeat, this, &ACamera2PActor::OnTouchMove);
	PlayerInputComponent->BindTouch(IE_Released, this, &ACamera2PActor::OnTouchEnd);
}

// 开始相机旋转并暂时关闭棋子点击。
void ACamera2PActor::OnMouseRightButtonPressed()
{
    if (bPortraitCamera) return;
	bIsRotatingCamera = true;

	// 显示鼠标光标并锁定到视口
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->bEnableClickEvents = false;
		PC->bEnableMouseOverEvents = false;
	}
}

// 停止相机旋转并恢复棋子点击。
void ACamera2PActor::OnMouseRightButtonReleased()
{
	bIsRotatingCamera = false;

	// 恢复鼠标光标
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = !bPortraitCamera;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}
}

// 根据鼠标水平位移更新相机偏航。
void ACamera2PActor::OnMouseX(float Value)
{
	if (bIsRotatingCamera && FMath::Abs(Value) > 0.0f)
	{
		// 绕Z轴旋转（Yaw）
		TargetRotation.Yaw += Value * RotationSpeed;
	}
}

// 根据鼠标垂直位移更新相机俯仰。
void ACamera2PActor::OnMouseY(float Value)
{
	if (bIsRotatingCamera && FMath::Abs(Value) > 0.0f)
	{
		// 绕X轴旋转（Pitch），并限制角度
		float NewPitch = TargetRotation.Pitch + Value * RotationSpeed;
		TargetRotation.Pitch = FMath::Clamp(NewPitch, MinPitchAngle, MaxPitchAngle);
	}
}

// 鼠标滚轮缩放处理函数
void ACamera2PActor::OnMouseWheel(float Value)
{
    if (bPortraitCamera) return;
	if (FMath::Abs(Value) > 0.0f)
	{
		// 计算缩放增量（滚轮向上为正值，但我们要缩小距离，所以取负）
		float ZoomDelta = Value * MouseWheelZoomSensitivity;

		// 应用缩放
		TargetArmLength = FMath::Clamp(TargetArmLength - ZoomDelta, MinZoomDistance, MaxZoomDistance);

		// 可选：添加缩放效果反馈
		// UE_LOG(LogTemp, Warning, TEXT("Mouse Wheel: %.2f, New Target Length: %.2f"), Value, TargetArmLength);
	}
}

// 记录触摸起点并识别单指或双指操作。
void ACamera2PActor::OnTouchBegin(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (bPortraitCamera) return;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC)) return;
	if (FingerIndex == ETouchIndex::Touch1)
	{
		// 单指触摸开始
		float LocationX, LocationY;
		bool bIsCurrentlyPressed; // 这个参数表示触摸点当前是否被按下

		// 使用正确的GetInputTouchState函数签名
		PC->GetInputTouchState(
			FingerIndex,
			LocationX,
			LocationY,
			bIsCurrentlyPressed
		);

		if (bIsCurrentlyPressed)
		{
			FVector2D ScreenLocation(LocationX, LocationY);
			InitialTouchLocation = ScreenLocation;
			PreviousTouchLocation = ScreenLocation;
			bIsSingleTouchActive = true;
		}
	}
	else if (FingerIndex == ETouchIndex::Touch2)
	{
		// 双指触摸开始
		float FirstFingerX, FirstFingerY, SecondFingerX, SecondFingerY;
		bool bFirstPressed, bSecondPressed;

		// 检查两个触摸点是否都有效
		PC->GetInputTouchState(
			ETouchIndex::Touch1,
			FirstFingerX,
			FirstFingerY,
			bFirstPressed
		);

		PC->GetInputTouchState(
			ETouchIndex::Touch2,
			SecondFingerX,
			SecondFingerY,
			bSecondPressed
		);

		if (bFirstPressed && bSecondPressed)
		{
			FirstFingerLocation = FVector2D(FirstFingerX, FirstFingerY);
			SecondFingerLocation = FVector2D(SecondFingerX, SecondFingerY);
			InitialFingerDistance = FVector2D::Distance(FirstFingerLocation, SecondFingerLocation);
			bIsTwoFingerTouchActive = true;
			bIsSingleTouchActive = false;
		}
	}
}

// 处理触摸移动，更新旋转或缩放。
void ACamera2PActor::OnTouchMove(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (bPortraitCamera) return;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC)) return;
	if (bIsTwoFingerTouchActive)
	{
		// 处理双指手势（缩放）
		HandleTwoFingerGesture();
	}
	else if (bIsSingleTouchActive && FingerIndex == ETouchIndex::Touch1)
	{
		// 处理单指滑动（旋转）
		float CurrentX, CurrentY;
		bool bIsPressed;

		// 获取当前触摸点状态
		PC->GetInputTouchState(
			FingerIndex,
			CurrentX,
			CurrentY,
			bIsPressed
		);

		if (bIsPressed)
		{
			FVector2D CurrentTouchLocation(CurrentX, CurrentY);

			// 计算滑动增量
			FVector2D Delta = CurrentTouchLocation - PreviousTouchLocation;

			// 应用旋转（Y轴反向，因为屏幕坐标Y轴向下）
			float NewYaw = TargetRotation.Yaw + Delta.X * RotationSensitivity * 0.1f;
			float NewPitch = TargetRotation.Pitch - Delta.Y * RotationSensitivity * 0.1f;

			// 限制俯仰角
			NewPitch = FMath::Clamp(NewPitch, MinPitchAngle, MaxPitchAngle);

			TargetRotation = FRotator(NewPitch, NewYaw, 0.0f);

			PreviousTouchLocation = CurrentTouchLocation;
		}
	}
}

// 结束触摸并同步剩余手指状态。
void ACamera2PActor::OnTouchEnd(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (bPortraitCamera) return;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC)) return;
	float DummyX, DummyY;
	bool bIsPressed;

	if (FingerIndex == ETouchIndex::Touch1)
	{
		bIsSingleTouchActive = false;

		// 检查第二指是否仍然有效
		PC->GetInputTouchState(ETouchIndex::Touch2, DummyX, DummyY, bIsPressed);
		if (bIsPressed)
		{
			bIsTwoFingerTouchActive = true;

			// 更新第二指位置
			PC->GetInputTouchState(
				ETouchIndex::Touch2,
				SecondFingerLocation.X,
				SecondFingerLocation.Y,
				bIsPressed
			);
		}
	}
	else if (FingerIndex == ETouchIndex::Touch2)
	{
		bIsTwoFingerTouchActive = false;

		// 检查第一指是否仍然有效
		PC->GetInputTouchState(ETouchIndex::Touch1, DummyX, DummyY, bIsPressed);
		if (bIsPressed)
		{
			bIsSingleTouchActive = true;

			// 更新第一指位置
			PC->GetInputTouchState(
				ETouchIndex::Touch1,
				PreviousTouchLocation.X,
				PreviousTouchLocation.Y,
				bIsPressed
			);
		}
	}
}

// 实现
bool ACamera2PActor::IsValidTwoFingerGesture()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC)) return false;
	if (!bIsTwoFingerTouchActive) return false;

	float FirstX, FirstY, SecondX, SecondY;
	bool bFirstPressed, bSecondPressed;

	PC->GetInputTouchState(
		ETouchIndex::Touch1, FirstX, FirstY, bFirstPressed);
	PC->GetInputTouchState(
		ETouchIndex::Touch2, SecondX, SecondY, bSecondPressed);

	if (!bFirstPressed || !bSecondPressed)
	{
		bIsTwoFingerTouchActive = false;
		return false;
	}

	// 检查两指距离是否有效（避免距离太近的误触）
	float CurrentDistance = FVector2D::Distance(
		FVector2D(FirstX, FirstY),
		FVector2D(SecondX, SecondY)
	);

	return CurrentDistance >= MinPinchDistance;
}

// 根据双指间距变化缩放相机。
void ACamera2PActor::HandleTwoFingerGesture()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC)) return;
	if (!IsValidTwoFingerGesture()) return;

	float FirstX, FirstY, SecondX, SecondY;
	bool bFirstPressed, bSecondPressed;

	PC->GetInputTouchState(
		ETouchIndex::Touch1, FirstX, FirstY, bFirstPressed);
	PC->GetInputTouchState(
		ETouchIndex::Touch2, SecondX, SecondY, bSecondPressed);

	FVector2D CurrentFirstFinger(FirstX, FirstY);
	FVector2D CurrentSecondFinger(SecondX, SecondY);

	// 计算当前两指距离
	float CurrentDistance = FVector2D::Distance(CurrentFirstFinger, CurrentSecondFinger);

	// 计算缩放比例
	float ZoomDelta = (CurrentDistance - InitialFingerDistance) * ZoomSensitivity;

	// 应用缩放
	TargetArmLength = FMath::Clamp(TargetArmLength - ZoomDelta, MinZoomDistance, MaxZoomDistance);

	// 更新初始距离为当前距离，实现连续缩放
	InitialFingerDistance = CurrentDistance;

	// 同时更新存储的位置
	FirstFingerLocation = CurrentFirstFinger;
	SecondFingerLocation = CurrentSecondFinger;
}
