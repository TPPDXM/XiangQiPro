// Copyright 2026 Ultimate Player All Rights Reserved.

#include "CameraMainActor.h"
#include "CineCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "XiangQiPro/UI/XQP_HUD.h"
#include "XiangQiPro/Util/Logger.h"

// Sets default values
ACameraMainActor::ACameraMainActor()
{
	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

	// 场景根组件
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// CineCamera
	CineCameraComponent = CreateDefaultSubobject<UCineCameraComponent>(TEXT("CineCamera"));
	CineCameraComponent->SetupAttachment(SceneRoot);

	// 创建音频组件
	MainAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MainAudio"));
	MainAudio->SetupAttachment(CineCameraComponent);

	// 默认值
	FocusTarget = nullptr;
	CameraDistance = 800.0f;
	CameraHeightOffset = 150.0f;

	// 初始化鼠标相关变量
	MouseSensitivity = 0.1f;
	MaxHorizontalOffset = 5.0f;
	MaxVerticalOffset = 3.0f;

	CurrentMouseX = 0.5f;
	CurrentMouseY = 0.5f;
	TargetMouseX = 0.5f;
	TargetMouseY = 0.5f;
	BaseHorizontalAngle = 0.0f;
	BaseVerticalAngle = -15.0f; // 默认俯角
	bMouseInputEnabled = false;

	// CineCamera参数设置
	SetupCineCameraParameters();
}

// Called when the game starts or when spawned
void ACameraMainActor::BeginPlay()
{
	Super::BeginPlay();
	// 监听窗口大小变化
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		GEngine->GameViewport->Viewport->ViewportResizedEvent.AddUObject(
			this, &ACameraMainActor::OnViewportResized);
	}

	// 初始化时更新一次裁剪比例
	UpdateFilmbackAspectRatio();

	InitUI();
	InitAudio();

	UpdateCameraFocus();
	SetupMouseInput(); // 设置鼠标输入

	// 计算基础角度（基于当前相机位置和朝向）
	CalculateBaseAngles();
}

void ACameraMainActor::InitUI()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		HUD = Cast<AXQP_HUD>(PC->GetHUD());
		if (HUD)
		{
			UI_Main_Base* Base = CreateWidget<UI_Main_Base>(GetWorld(), HUD->Class_Main_Base);
			Base->Init(this);

			if (UUIManager* UIManager = GetGameInstance()->GetSubsystem<UUIManager>())
			{
				UIManager->Init(Base); // 初始化用户界面管理器
			}
		}
		else
		{
			ULogger::LogError(TEXT("ACameraMainActor::BeginPlay: HUD is nullptr!"));
		}
	}
	else
	{
		ULogger::LogError(TEXT("ACameraMainActor::BeginPlay: Player controller is nullptr!"));
	}
}

void ACameraMainActor::InitAudio()
{
	MainAudio->SetSound(MainMusic); // 设置音频资源
	MainAudio->Play(); // 开始播放背景音乐
}

void ACameraMainActor::OnViewportResized(FViewport* Viewport, uint32 Param)
{
	UpdateFilmbackAspectRatio();
}

void ACameraMainActor::UpdateFilmbackAspectRatio()
{
	TWeakObjectPtr<ACameraMainActor> WeakThis(this);
	if (!WeakThis.IsValid())
	{
		return;
	}
	if (!WeakThis->CineCameraComponent)
	{
		ULogger::LogError(TEXT("ACameraMainActor::UpdateFilmbackAspectRatio"), TEXT("CineCameraComponent is nullptr!"));
		return;
	}

	// 获取视口大小
	FVector2D ViewportSize = FVector2D();
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	float AspectRatio = ViewportSize.X / ViewportSize.Y;
	float BaseAspectRatio = BaseFilmbackWidth / BaseFilmbackHeight; // 16:9 = 1.777...

	// 根据宽高比调整裁剪比例
	if (AspectRatio > BaseAspectRatio)
	{
		// 宽屏：保持高度不变，增加宽度
		CineCameraComponent->Filmback.SensorWidth = BaseFilmbackHeight * AspectRatio;
		CineCameraComponent->Filmback.SensorHeight = BaseFilmbackHeight;
	}
	else
	{
		// 窄屏：保持宽度不变，减少高度
		CineCameraComponent->Filmback.SensorWidth = BaseFilmbackWidth;
		CineCameraComponent->Filmback.SensorHeight = BaseFilmbackWidth / AspectRatio;
	}

	// 可选：限制最大/最小裁剪比例
	CineCameraComponent->Filmback.SensorWidth = FMath::Clamp(CineCameraComponent->Filmback.SensorWidth, 10.0f, 100.0f);
	CineCameraComponent->Filmback.SensorHeight = FMath::Clamp(CineCameraComponent->Filmback.SensorHeight, 5.0f, 50.0f);
}

// 计算基础角度
void ACameraMainActor::CalculateBaseAngles()
{
	if (FocusTarget)
	{
		FVector ToCamera = GetActorLocation() - FocusTarget->GetActorLocation();
		ToCamera.Normalize();

		// 计算水平角度（Yaw）
		BaseHorizontalAngle = FMath::RadiansToDegrees(FMath::Atan2(ToCamera.Y, ToCamera.X));

		// 计算垂直角度（Pitch）
		FVector HorizontalProjection = FVector(ToCamera.X, ToCamera.Y, 0.0f);
		HorizontalProjection.Normalize();
		BaseVerticalAngle = FMath::RadiansToDegrees(FMath::Asin(ToCamera.Z));
	}
}

void ACameraMainActor::UpdateCameraFocus()
{
	if (FocusTarget)
	{
		// 重新计算基础角度（以防目标位置变化）
		CalculateBaseAngles();

		CameraDistance = FVector::Distance(GetActorLocation(), FocusTarget->GetActorLocation());
		CineCameraComponent->FocusSettings.ManualFocusDistance = CameraDistance - 1.5f;
	}

	EnableForPlayer(0);

	// 确保鼠标输入启用
	if (!bMouseInputEnabled)
	{
		SetupMouseInput();
	}
}

// Called every frame
void ACameraMainActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bMouseInputEnabled)
	{
		HandleMouseMovement(DeltaTime);
	}

	// 更新相机位置以保持聚焦目标
	if (FocusTarget)
	{
		// 计算基于鼠标偏移的最终角度
		float FinalHorizontalAngle = BaseHorizontalAngle + FMath::Clamp(
			(CurrentMouseX - 0.5f) * 2.0f * MaxHorizontalOffset,
			-MaxHorizontalOffset,
			MaxHorizontalOffset
		);

		float FinalVerticalAngle = BaseVerticalAngle + FMath::Clamp(
			(CurrentMouseY - 0.5f) * 2.0f * MaxVerticalOffset,
			-MaxVerticalOffset,
			MaxVerticalOffset
		);

		// 球坐标计算相机位置
		FVector TargetLocation = FocusTarget->GetActorLocation();
		float Theta = FMath::DegreesToRadians(FinalHorizontalAngle);
		float Phi = FMath::DegreesToRadians(FinalVerticalAngle);

		FVector Offset = { 0,0,0 };
		Offset.X = CameraDistance * FMath::Cos(Phi) * FMath::Cos(Theta);
		Offset.Y = CameraDistance * FMath::Cos(Phi) * FMath::Sin(Theta);
		Offset.Z = CameraDistance * FMath::Sin(Phi) + CameraHeightOffset;

		FVector NewCameraLocation = TargetLocation + LocationOffset + Offset;
		SetActorLocation(NewCameraLocation);

		// 确保相机始终看向目标
		FVector LookDirection = (TargetLocation - NewCameraLocation).GetSafeNormal();
		FRotator NewRotation = LookDirection.Rotation();
		NewRotation.Yaw += RotationZOffset;
		SetActorRotation(NewRotation);

		// 更新对焦距离
		float CurrentFocusDistance = FVector::Distance(NewCameraLocation, TargetLocation);
		CineCameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		CineCameraComponent->FocusSettings.ManualFocusDistance = CurrentFocusDistance;
	}
}

// 为玩家启用这个相机
void ACameraMainActor::EnableForPlayer(int32 PlayerIndex)
{
	// 获取指定索引的玩家控制器
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, PlayerIndex);
	if (PlayerController && CineCameraComponent)
	{
		// 将相机设置为视图目标
		PlayerController->SetViewTargetWithBlend(this, 0);

		// 禁用自动管理活动相机目标
		PlayerController->bAutoManageActiveCameraTarget = false;
	}
}

// 设置CineCamera参数
void ACameraMainActor::SetupCineCameraParameters()
{
	if (!CineCameraComponent) return;

	// 视野设置
	CineCameraComponent->SetFieldOfView(60.0f);
	CineCameraComponent->Filmback.SensorWidth = 36.0f;
	CineCameraComponent->Filmback.SensorHeight = 24.0f;

	// 镜头设置
	CineCameraComponent->LensSettings.MinFocalLength = 20.0f;
	CineCameraComponent->LensSettings.MaxFocalLength = 200.0f;
	CineCameraComponent->LensSettings.MinFStop = 1.2f;  // 光圈范围
	CineCameraComponent->LensSettings.MaxFStop = 22.0f;

	// 当前镜头和光圈
	CineCameraComponent->CurrentFocalLength = 30.0f; // 30mm镜头
	CineCameraComponent->CurrentAperture = 2.8f;     // 光圈f/2.8

	// 对焦设置（手动对焦）
	CineCameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	CineCameraComponent->FocusSettings.ManualFocusDistance = 500.0f;

	// 光圈叶片数量和形状
	CineCameraComponent->LensSettings.DiaphragmBladeCount = 9;
}

// 设置新的聚焦目标
void ACameraMainActor::SetFocusTarget(AActor* NewTarget)
{
	FocusTarget = NewTarget;

	if (FocusTarget)
	{
		// 重新计算基础角度
		CalculateBaseAngles();
		UpdateCameraFocus();
	}
}

// 设置鼠标输入
void ACameraMainActor::SetupMouseInput()
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		// 启用鼠标输入
		bMouseInputEnabled = true;

		// 显示鼠标光标
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}
}

// 处理鼠标移动
void ACameraMainActor::HandleMouseMovement(float DeltaTime)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		// 获取鼠标位置
		float MouseX, MouseY;
		bool bGotMousePosition = PlayerController->GetMousePosition(MouseX, MouseY);

		// 获取视口大小
		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		if (ViewportSizeX > 0 && ViewportSizeY > 0)
		{
			// 仅在鼠标在窗口内时调整目标位置
			if (bGotMousePosition)
			{
				// 将鼠标位置归一化到 [0, 1] 范围
				TargetMouseX = FMath::Clamp(MouseX / ViewportSizeX, 0.0f, 1.0f);
				TargetMouseY = FMath::Clamp(MouseY / ViewportSizeY, 0.0f, 1.0f);
			}

			// 平滑插值当前值到目标值
			CurrentMouseX = FMath::FInterpTo(CurrentMouseX, TargetMouseX, DeltaTime, 5.0f);
			CurrentMouseY = FMath::FInterpTo(CurrentMouseY, TargetMouseY, DeltaTime, 5.0f);
		}
	}
}