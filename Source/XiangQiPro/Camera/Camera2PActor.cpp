// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Camera2PActor.h"
#include "XiangQiPro/Audio/XQPAudioSettingsSubsystem.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "../GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
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
#include "Engine/LocalPlayer.h"
#include "SceneView.h"

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
    static ConstructorHelpers::FObjectFinder<USoundBase> Music(PATH_AUDIO_BATTLE);
    BattleMusic = Music.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> FengHuoMusic(PATH_AUDIO_FENGHUO_BATTLE);
    FengHuoBattleMusic = FengHuoMusic.Object;

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
    // 委托属于 GameState，生命周期可能比相机长；先解除订阅，再停止当前声音。
    if (MusicBattleStart.IsValid()) MusicBattleStart->OnSetupChanged.RemoveAll(this);
    MusicBattleStart.Reset();
    if (IsValid(BattleAudio)) BattleAudio->Stop();
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
    const auto* Room = GI ? GI->GetSubsystem<UXQPLANSubsystem>() : nullptr;
    if (Room && Room->IsSpectator() && !bPortraitCamera)
    {
        TargetRotation.Yaw -= 90.f;
        TargetRotation.Pitch = -58.f;
        TargetArmLength = FMath::Clamp(TargetArmLength * .95f, MinZoomDistance, MaxZoomDistance);
        SpringArm->TargetArmLength = TargetArmLength;
        SpringArm->SetRelativeRotation(TargetRotation);
    }
    else if (GI && Room && !Room->IsSpectator() && (GI->GetGameMode() == EXQPGameMode::LAN || GI->GetGameMode() == EXQPGameMode::FengHuoLAN) && Room->GetLocalColor() == EChessColor::BLACKCHESS)
    {
        TargetRotation.Yaw += 180.f;
        SpringArm->SetRelativeRotation(TargetRotation);
    }
    HomeRotation = TargetRotation;
    DesktopBaseTargetOffset = SpringArm->TargetOffset;
    DesktopHomeArmLength = TargetArmLength;
    DesktopHomeMaxZoom = MaxZoomDistance;
    if (MinPitchAngle <= -89.9f) MinPitchAngle = -90.f;
    if (bPortraitCamera) EnablePortraitCamera();
    else UpdateDesktopOrbit();
}

void ACamera2PActor::UpdateDesktopOrbit()
{
    const float ZoomRange = MaxZoomDistance - MinZoomDistance;
    const float Proximity = ZoomRange > KINDA_SMALL_NUMBER
        ? FMath::Clamp((MaxZoomDistance - SpringArm->TargetArmLength) / ZoomRange, 0.f, 1.f) : 0.f;
    const FRotator View = SpringArm->GetComponentRotation();
    const float Elevation = FMath::Clamp(FMath::Abs(FRotator::NormalizeAxis(View.Pitch)), 0.f, 90.f);
    const float Radius = FMath::Max(0.f, CloseOrbitRadius) * Proximity
        * FMath::Max(0.f, FMath::Cos(FMath::DegreesToRadians(Elevation)));
    // 水平径向指向镜头所在的一侧。偏移位于棋盘平面，不随俯角改变高度。
    const FVector Outward = -FRotator(0.f, View.Yaw, 0.f).Vector();
    SpringArm->TargetOffset = DesktopBaseTargetOffset + Outward * Radius;
}

void ACamera2PActor::FitProBoardToViewport(const FVector2D& Viewport, const FBox2D& BoardRect)
{
    const auto* GI = GetGameInstance<UXQPGameInstance>();
    const auto* Room = GI ? GI->GetSubsystem<UXQPLANSubsystem>() : nullptr;
    const bool Watching = Room && Room->IsSpectator();
    if (!GI || (!Watching && GI->GetGameMode() != EXQPGameMode::FengHuoAI && GI->GetGameMode() != EXQPGameMode::FengHuoLAN) ||
        bPortraitCamera || bFengHuoResultCamera || bDesktopCameraAdjusted || Viewport.X <= 0 || Viewport.Y <= 0 ||
        !BoardRect.bIsValid || BoardRect.GetSize().X < 80 || BoardRect.GetSize().Y < 80) return;
    if (!SpringArm->GetRelativeRotation().Equals(TargetRotation, .1f)) return;
    if (bDesktopFrameApplied && LastDesktopViewport.Equals(Viewport, 1.f) &&
        LastDesktopBoardRect.Min.Equals(BoardRect.Min, 1.f) && LastDesktopBoardRect.Max.Equals(BoardRect.Max, 1.f)) return;
    if (!MobileBoard.IsValid())
    {
        TActorIterator<AChessBoard2PActor> It(GetWorld());
        if (It) MobileBoard = *It;
    }
    if (!MobileBoard.IsValid() || !MobileBoard->ChessBoardMesh) return;
    const FBox Bounds = MobileBoard->ChessBoardMesh->Bounds.GetBox().ExpandBy(FVector(2.f, 2.f, 6.f));
    if (!Bounds.IsValid) return;
    FMinimalViewInfo View;
    GameCamera->GetCameraView(0.f, View);
    const FIntRect FullRect(0, 0, FMath::RoundToInt(Viewport.X), FMath::RoundToInt(Viewport.Y));
    FSceneViewProjectionData Projection;
    Projection.SetViewRectangle(FullRect);
    const EAspectRatioAxisConstraint Constraint = GameCamera->bOverrideAspectRatioAxisConstraint
        ? GameCamera->AspectRatioAxisConstraint.GetValue() : GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint.GetValue();
    FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, Constraint, FullRect, Projection);
    const FRotator OriginalArmRotation = SpringArm->GetComponentRotation();
    const FMatrix AxisSwap(FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
    // 候选镜头仅改变弹簧臂距离与俯仰角。这里和 UpdateDesktopOrbit 使用
    // 完全相同的原始中心、径向半径与 SocketOffset，不能为了 HUD 平移中心，
    // 否则旋转到棋盘另一侧时会产生额外的远近变化。
    const auto Fits = [&](float Distance, const FRotator& ArmRotation)
    {
        const FVector Outward = -FRotator(0.f, ArmRotation.Yaw, 0.f).Vector();
        const float PitchWeight = FMath::Max(0.f, FMath::Cos(FMath::DegreesToRadians(FMath::Abs(ArmRotation.Pitch))));
        const float Range = DesktopHomeMaxZoom - MinZoomDistance;
        const float Proximity = Range > KINDA_SMALL_NUMBER ? FMath::Clamp((DesktopHomeMaxZoom - Distance) / Range, 0.f, 1.f) : 0.f;
        const FVector Pivot = SpringArm->GetComponentLocation() + DesktopBaseTargetOffset + Outward * (FMath::Max(0.f, CloseOrbitRadius) * Proximity * PitchWeight);
        const FTransform Socket(ArmRotation, Pivot - ArmRotation.Vector() * Distance + ArmRotation.RotateVector(SpringArm->SocketOffset));
        const FTransform Camera = GameCamera->GetRelativeTransform() * Socket;
        const FMatrix Matrix = FTranslationMatrix(-Camera.GetLocation()) * FInverseRotationMatrix(Camera.Rotator()) * AxisSwap * Projection.ProjectionMatrix;
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector Point((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
                (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
            FVector2D Pixel;
            if (!FSceneView::ProjectWorldToScreen(Point, Projection.GetConstrainedViewRect(), Matrix, Pixel) ||
                Pixel.X < BoardRect.Min.X || Pixel.X > BoardRect.Max.X || Pixel.Y < BoardRect.Min.Y || Pixel.Y > BoardRect.Max.Y) return false;
        }
        return true;
    };
    float Distance = TNumericLimits<float>::Max();
    FRotator BestRotation = OriginalArmRotation;
    // 观战从原有 58 度俯角逐步增加到 82 度，选择能容纳完整棋盘的最近距离。
    // 每个候选都检查扩展棋盘包围盒的八角，覆盖棋子高度及不同屏幕比例。
    // 普通烽火弈对局沿用当前俯角，只拟合距离。
    const int32 PitchCandidates = Watching ? 7 : 1;
    for (int32 Candidate = 0; Candidate < PitchCandidates; ++Candidate)
    {
        FRotator Rotation = OriginalArmRotation;
        if (Watching) Rotation.Pitch = -58.f - Candidate * 4.f;
        float Low = FMath::Max(MinZoomDistance, DesktopHomeArmLength);
        float High = Low;
        if (!Fits(High, Rotation))
        {
            High = FMath::Max(DesktopHomeMaxZoom, Low * 2.f);
            for (int32 Attempt = 0; Attempt < 6 && !Fits(High, Rotation); ++Attempt) High *= 2.f;
            if (!Fits(High, Rotation)) continue;
            for (int32 Step = 0; Step < 20; ++Step)
            {
                const float Mid = (Low + High) * .5f;
                if (Fits(Mid, Rotation)) High = Mid;
                else Low = Mid;
            }
        }
        if (High < Distance)
        {
            Distance = High;
            BestRotation = Rotation;
        }
    }
    if (Distance == TNumericLimits<float>::Max()) return;
    if (Watching)
    {
        // 将世界空间候选旋转转换回相对旋转，不假设相机 Actor 本身没有旋转。
        const FQuat ParentRotation = SpringArm->GetComponentQuat() * SpringArm->GetRelativeRotation().Quaternion().Inverse();
        TargetRotation = (ParentRotation.Inverse() * BestRotation.Quaternion()).Rotator();
        HomeRotation = TargetRotation;
        SpringArm->SetRelativeRotation(TargetRotation);
    }
    MaxZoomDistance = FMath::Max(DesktopHomeMaxZoom, Distance);
    TargetArmLength = Distance;
    if (!bDesktopFrameApplied) SpringArm->TargetArmLength = Distance;
    UpdateDesktopOrbit();
    bDesktopFrameApplied = true;
    LastDesktopViewport = Viewport;
    LastDesktopBoardRect = BoardRect;
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

void ACamera2PActor::PlayFengHuoResultCamera()
{
    if (bPortraitCamera) { bFengHuoResultCamera = true; FitMobileBoard(); return; }
    bFengHuoResultCamera = true;
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
    if (const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>(); State && State->IsFengHuoBattle())
    {
        // 重开广播的 Actor 调用顺序不固定，不能依赖 GameState 已经重置确认组件。
        // 立即停掉上一局曲目；新局 BeginSetup 的通知会重新进入等待倒计时状态。
        if (IsValid(BattleAudio)) BattleAudio->Stop();
        bFengHuoMusicStarted = false;
        bFengHuoResultCamera = false;
        TargetRotation = HomeRotation;
        if (bPortraitCamera) FitMobileBoard();
        else
        {
            TargetArmLength = DesktopHomeArmLength;
            MaxZoomDistance = DesktopHomeMaxZoom;
            bDesktopCameraAdjusted = bDesktopFrameApplied = false;
        }
        EnableInput(GetController<APlayerController>());
    }
    IIF_GameState::GamePlayAgain(OwnerObject);
}

// 普通模式进场播放默认音乐；烽火弈只准备资源，正式起音由开局状态控制。
void ACamera2PActor::InitAudio()
{
    if (!IsValid(BattleAudio)) return;
    // 在背景音乐首次起音前应用本机总线偏好，覆盖 PIE 和跨地图的新音频设备。
    if (UGameInstance* Game = GetGameInstance())
        Game->GetSubsystem<UXQPAudioSettingsSubsystem>()->ApplyToWorld(GetWorld());

    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (State && State->IsFengHuoBattle())
    {
        // 复用唯一的 BattleAudio，明确替换默认声音，防止两首背景音乐同时播放。
        // 即便专属资源缺失，也不退回默认音乐，避免提前起音掩盖导入问题。
        BattleAudio->Stop();
        BattleAudio->SetSound(FengHuoBattleMusic);
        MusicBattleStart = State->BattleStart;
        if (MusicBattleStart.IsValid())
        {
            MusicBattleStart->OnSetupChanged.AddUObject(this, &ThisClass::RefreshFengHuoMusicState);
            // 兼容相机晚于开局创建；等待期间此调用只停止声音，不会提前播放。
            RefreshFengHuoMusicState();
        }
        return;
    }
    BattleAudio->SetSound(BattleMusic);
    if (BattleMusic) BattleAudio->Play();
    BattleAudio->SetPaused(bMusicPausedByMenu);
}

void ACamera2PActor::RefreshFengHuoMusicState()
{
    if (!MusicBattleStart.IsValid() || !IsValid(BattleAudio)) return;
    if (!MusicBattleStart->HasStarted())
    {
        // BeginSetup 会在每局开始时广播，包括联机重开；准备和三秒倒计时均保持静音。
        BattleAudio->Stop();
        bFengHuoMusicStarted = false;
        return;
    }
    if (bFengHuoMusicStarted) return;

    // HasStarted 是烽火计时组件使用的同一门槛：本机由倒计时归零置位，
    // 联机客户端由权威开局快照置位。不能用本地 UI 倒计时推测开局时间。
    // 标记保持到下一局，曲目结束或重复收到 running 快照都不会触发重播。
    bFengHuoMusicStarted = true;
    if (FengHuoBattleMusic)
    {
        BattleAudio->Play(0.f);
        // 单人暂停时保留暂停状态；联机选项菜单不改变播放状态。
        BattleAudio->SetPaused(bMusicPausedByMenu);
    }
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
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
		SpringArm->SetRelativeRotation(NewRotation);
	}

	// 平滑插值缩放
	if (!FMath::IsNearlyEqual(SpringArm->TargetArmLength, TargetArmLength))
	{
		float CurrentLength = SpringArm->TargetArmLength;
		float NewLength = FMath::FInterpTo(CurrentLength, TargetArmLength, DeltaTime, ZoomInterpSpeed);
		SpringArm->TargetArmLength = NewLength;
	}
    UpdateDesktopOrbit();
}

// 响应游戏暂停事件。
void ACamera2PActor::GamePause(UObject* OwnerObject)
{
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    const bool bOnline = State && State->IsLANGame();
    // 联机仅打开选项，音乐继续播放；单人显式暂停 UI Sound 并保留读头。
    bMusicPausedByMenu = !bOnline;
    if (IsValid(BattleAudio)) BattleAudio->SetPaused(bMusicPausedByMenu);
	// 屏蔽输入
	DisableInput(GetController<APlayerController>());
    if (!bOnline) IIF_GameState::GamePause(OwnerObject);
}

// 响应游戏恢复事件。
void ACamera2PActor::GameResume(UObject* OwnerObject)
{
    // 只恢复既有实例，不重新 Play；未开局或已自然播完的音乐不会被重新启动。
    bMusicPausedByMenu = false;
    if (IsValid(BattleAudio)) BattleAudio->SetPaused(false);
	// 启用输入
    if (!bFengHuoResultCamera) EnableInput(GetController<APlayerController>());
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
    bDesktopCameraAdjusted = true;

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
        bDesktopCameraAdjusted = true;
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
    bDesktopCameraAdjusted = true;
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
