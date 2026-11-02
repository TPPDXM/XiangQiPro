// Copyright 2026 Ultimate Player All Rights Reserved.
#include "MobileMenuCamera.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "XiangQiPro/GameObject/MenuChessDemoComponent.h"
#include "XiangQiPro/UI/Main/UI_Main_Base.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/XQP_HUD.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"

namespace
{
    constexpr float HorizontalSensitivity = 180.f;
    constexpr float VerticalSensitivity = 150.f;
    constexpr float InertiaDecay = 5.f;
    constexpr float ResetIdleSeconds = 5.f;
    constexpr float ReturnDuration = .8f;
}

AMobileMenuCamera::AMobileMenuCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
    BoardPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoardPivot"));
    BoardPivot->SetupAttachment(RootComponent);
    Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
    Board->SetupAttachment(BoardPivot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(PATH_PACKAGE_SM_CHESSBOARD_2P);
    Board->SetStaticMesh(Mesh.Object);
    // This mesh's long edge is its local Y axis; the game coordinates use X for ranks.
    Board->SetRelativeRotation(FRotator(0, 90, 0));
    Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Board->SetCastShadow(false);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("MobileMenuCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bConstrainAspectRatio = false;
    Camera->bOverrideAspectRatioAxisConstraint = true;
    Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
    Camera->FieldOfView = 42.f;
    Camera->PostProcessSettings.bOverride_DepthOfFieldScale = true;
    Camera->PostProcessSettings.DepthOfFieldScale = 0.f;
    Demo = CreateDefaultSubobject<UMenuChessDemoComponent>(TEXT("ChessDemo"));
    Demo->SetPresentationRoot(BoardPivot);
    Music = CreateDefaultSubobject<UAudioComponent>(TEXT("Music"));
    Music->SetupAttachment(Board);
    Music->bIsUISound = true;
    static ConstructorHelpers::FObjectFinder<USoundBase> MenuMusic(PATH_AUDIO_LOBBY);
    Music->SetSound(MenuMusic.Object);
}

void AMobileMenuCamera::BeginPlay()
{
    Super::BeginPlay();
    // 已有地图可能保存了旧的 Board 绑定关系，运行时重新确认其父级。
    // Demo 在组件 BeginPlay 前已经从默认子对象获得同一个展示枢轴。
    Board->AttachToComponent(BoardPivot, FAttachmentTransformRules::KeepRelativeTransform);
    Board->SetCastShadow(false); // 已保存地图中的旧组件值也必须关闭。
    ConfigureBoardClearance();
    HomeBoardRotation = BoardPivot->GetRelativeRotation().Quaternion();
    Demo->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
    FitBoard();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    // 此场景由展示相机接管，默认玩家球体无需渲染。抬高镜头后必须隐藏它，
    // 避免球体和它的阴影出现在主界面背景里；正式对局会生成自己的 Pawn。
    if (APawn* Pawn = PC->GetPawn()) Pawn->SetActorHiddenInGame(true);
    PC->bAutoManageActiveCameraTarget = false;
    PC->SetViewTarget(this);
    PC->bShowMouseCursor = false;
    PC->bEnableClickEvents = true;
    if (auto* HUD = Cast<AXQP_HUD>(PC->GetHUD()))
        if (auto* Menu = XQPMobile::CreateWidget<UUI_Main_Base>(PC, HUD->Class_Main_Base))
        {
            Menu->Init(nullptr);
            // 兼容旧版已保存的菜单资源：演示不再提供字幕，移除原字幕控件及其占位。
            if (UWidget* LegacyCaption = Menu->GetWidgetFromName(TEXT("DemoCaption"))) LegacyCaption->RemoveFromParent();
            GetGameInstance()->GetSubsystem<UUIManager>()->Init(Menu);
        }
    Music->Play();
}

void AMobileMenuCamera::FitBoard()
{
    const FVector2D Viewport = XQPMobile::ViewportSize(this);
    if (Viewport.X <= 0.f || Viewport.Y <= 0.f) return;
    const FMargin Safe = XQPMobile::SafeInsets(this);
    const float DPI = UWidgetLayoutLibrary::GetViewportScale(this);
    const float UsableHeight = Viewport.Y - Safe.Top - Safe.Bottom;
    // Title above the board, actions below it. All measurements use the same safe area as UMG.
    const FBox2D Frame(FVector2D(Safe.Left + 16.f * DPI, Safe.Top + UsableHeight * .16f),
        FVector2D(Viewport.X - Safe.Right - 16.f * DPI, Safe.Top + UsableHeight * .53f));
    const FRotator Rotation(-38.f, 45.f, 0.f);
    const FVector2D FrameCenter = Frame.GetCenter() / Viewport;
    LensShift = FVector2D(1.f - 2.f * FrameCenter.X, 2.f * FrameCenter.Y - 1.f);
    // Keep even the upper corner rays below the tabletop horizon on tall screens.
    const double MaxTanV = FMath::Tan(FMath::DegreesToRadians(30.0)) / (1.0 + FMath::Abs(LensShift.Y));
    const double TanH = FMath::Min(FMath::Tan(FMath::DegreesToRadians(21.0)), MaxTanV * Viewport.X / Viewport.Y);
    Camera->SetFieldOfView(FMath::RadiansToDegrees(2.0 * FMath::Atan(TanH)));
    const FVector Forward = Rotation.Vector();
    // 自由翻转使用旋转包络球拟合，一次覆盖所有朝向，不依赖角度采样或俯仰上限。
    // 镜头只在分辨率变化时重算，手动滑动、惯性和回正都不移动镜头或背景。
    const FVector Center = BoardPivot->GetComponentLocation();
    const double TanV = TanH * Viewport.Y / Viewport.X;
    const double Radius = FVector(44., 36., 7.).Size();
    const double HorizontalLimit = TanH * Frame.GetSize().X / Viewport.X;
    const double VerticalLimit = TanV * Frame.GetSize().Y / Viewport.Y;
    const double Distance = Radius * FMath::Sqrt(1. + FMath::Max(1. / FMath::Square(HorizontalLimit), 1. / FMath::Square(VerticalLimit)));
    Camera->SetWorldLocationAndRotation(Center - Forward * (Distance * 1.05), Rotation);
    LastViewport = Viewport;
    LastInsets = Safe;
}

void AMobileMenuCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBoardInteraction(DeltaSeconds);
    if (!LastViewport.Equals(XQPMobile::ViewportSize(this)) || LastInsets != XQPMobile::SafeInsets(this)) FitBoard();
}

void AMobileMenuCamera::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    OutResult.OffCenterProjectionOffset = LensShift;
}

// 碰撞保持关闭，避免菜单演示参与游戏拾取；命中检测直接在棋盘局部空间做射线/盒相交。
bool AMobileMenuCamera::IsBoardTouchPosition(const FVector2D& ScreenPosition) const
{
    const auto* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Player || !Board || !Board->GetStaticMesh()) return false;
    FVector Origin, Direction;
    if (!Player->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction)) return false;
    const FTransform Transform = Board->GetComponentTransform();
    const FVector LocalStart = Transform.InverseTransformPosition(Origin);
    const FVector LocalEnd = Transform.InverseTransformPosition(Origin + Direction * 100000.f);
    return FMath::LineBoxIntersection(Board->GetStaticMesh()->GetBoundingBox().ExpandBy(2.f), LocalStart, LocalEnd, LocalEnd - LocalStart);
}

void AMobileMenuCamera::ApplyBoardRotation(const FVector2D& AngleDegrees)
{
    // 按屏幕横纵轴施加世界空间增量，避免棋盘翻到另一面后上下手势倒向。
    // 使用四元数累计完整姿态，允许越过 90/180 度，不会发生欧拉角锁死或强行归零 Roll。
    const FVector Right = Camera->GetRightVector();
    const FQuat Delta = FQuat(Camera->GetUpVector(), FMath::DegreesToRadians(AngleDegrees.X))
        * FQuat(Right, FMath::DegreesToRadians(AngleDegrees.Y));
    const FQuat World = (Delta * BoardPivot->GetComponentQuat()).GetNormalized();
    BoardPivot->SetRelativeRotation((GetActorQuat().Inverse() * World).GetNormalized());
}

void AMobileMenuCamera::BeginBoardInteraction()
{
    bBoardHeld = true;
    InertialVelocity = FVector2D::ZeroVector;
    RotationIdleSeconds = 0.f;
    bReturningBoard = false;
}

void AMobileMenuCamera::EndBoardInteraction(bool bAllowInertia)
{
    bBoardHeld = false;
    if (!bAllowInertia)
    {
        InertialVelocity = PendingInputDegrees = FVector2D::ZeroVector;
        PendingRotationRadians = 0.f;
        PendingAngularDisplacement = FVector::ZeroVector;
    }
    else if (RotationIdleSeconds > .12f) InertialVelocity = FVector2D::ZeroVector;
}

void AMobileMenuCamera::RotateBoard(const FVector2D& ScreenDelta)
{
    const float Width = XQPMobile::ViewportSize(this).X;
    if (!BoardPivot || Width <= 0.f) return;
    const FQuat Before = BoardPivot->GetComponentQuat();
    // 向右拖时正对玩家的盘边向右移动；向下拖时同一盘边向下移动。
    // 纵向接近横向灵敏度，两个方向都允许连续翻转，无角度夹取。
    const FVector2D Input(-ScreenDelta.X * HorizontalSensitivity / Width, -ScreenDelta.Y * VerticalSensitivity / Width);
    if (Input.IsNearlyZero()) return;
    ApplyBoardRotation(Input);
    FQuat Delta = (BoardPivot->GetComponentQuat() * Before.Inverse()).GetNormalized();
    FVector Axis; float Angle;
    Delta.ToAxisAndAngle(Axis, Angle);
    if (Angle > PI) { Angle = 2.f * PI - Angle; Axis = -Axis; }
    PendingAngularDisplacement += Axis * Angle;
    PendingRotationRadians += FMath::DegreesToRadians(Input.Size());
    PendingInputDegrees += Input;
    RotationIdleSeconds = 0.f;
    bHasUserRotation = true;
    bReturningBoard = false;
}

void AMobileMenuCamera::ConfigureBoardClearance()
{
    if (!Board->GetStaticMesh()) return;
    const FBox Bounds = Board->GetStaticMesh()->GetBoundingBox().TransformBy(Board->GetRelativeTransform());
    double Radius = 0.;
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
            (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
        Radius = FMath::Max(Radius, Point.Size());
    }
    double SurfaceZ = GetActorLocation().Z;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        UStaticMeshComponent* Component = It->GetStaticMeshComponent();
        if (!Component || !Component->GetStaticMesh()) continue;
        const FBox Surface = Component->Bounds.GetBox();
        const FVector Origin = GetActorLocation();
        if (Surface.GetExtent().X < Radius * 2. || Surface.GetExtent().Y < Radius * 2. || Surface.GetExtent().Z > Radius ||
            Origin.X < Surface.Min.X || Origin.X > Surface.Max.X || Origin.Y < Surface.Min.Y || Origin.Y > Surface.Max.Y) continue;
        if (!BackgroundSurface.IsValid() || Surface.Max.Z > SurfaceZ)
        {
            BackgroundSurface = Component;
            SurfaceZ = Surface.Max.Z;
        }
    }
    // 不锁角度时，任意盘角都可能翻到枢轴正下方；悬浮高度必须覆盖完整旋转半径。
    // 相对实际背景顶面额外留出 4 单位，倒置和斜向翻滚时同样不会穿进背景。
    FVector Location = BoardPivot->GetRelativeLocation();
    Location.Z = Radius + 4. + SurfaceZ - GetActorLocation().Z;
    BoardPivot->SetRelativeLocation(Location);
}

void AMobileMenuCamera::UpdateBoardInteraction(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f) return;
    if (PendingRotationRadians > 0.f)
    {
        // 合并同一帧内的多个触摸采样，再除以真实帧时长，避免高采样率设备误触发。
        const float Speed = PendingRotationRadians / FMath::Max(DeltaSeconds, 1.f / 120.f);
        if (Speed >= FMath::DegreesToRadians(Demo->GetScatterTriggerSpeed()))
        {
            FVector Direction = PendingAngularDisplacement.GetSafeNormal();
            if (Direction.IsNearlyZero()) Direction = GetActorUpVector();
            Demo->ScatterPieces(Board, Direction * Speed, BackgroundSurface.Get());
        }
        InertialVelocity = (PendingInputDegrees / FMath::Max(DeltaSeconds, 1.f / 120.f)).GetClampedToMaxSize(1800.);
        PendingInputDegrees = FVector2D::ZeroVector;
        PendingRotationRadians = 0.f;
        PendingAngularDisplacement = FVector::ZeroVector;
    }
    else if (bHasUserRotation)
    {
        // 松手后指数衰减，使用解析积分保证不同帧率下的总惯性角接近一致。
        // 惯性不刷新静置计时，且程序化回正不参与甩飞速度检测。
        if (!bBoardHeld && !bReturningBoard && InertialVelocity.SizeSquared() > 4.)
        {
            const float Decay = FMath::Exp(-InertiaDecay * DeltaSeconds);
            ApplyBoardRotation(InertialVelocity * ((1.f - Decay) / InertiaDecay));
            InertialVelocity *= Decay;
            if (InertialVelocity.SizeSquared() <= 4.) InertialVelocity = FVector2D::ZeroVector;
        }
        RotationIdleSeconds += DeltaSeconds;
    }

    if (!bHasUserRotation || RotationIdleSeconds < ResetIdleSeconds) return;
    if (!bReturningBoard)
    {
        bReturningBoard = true;
        InertialVelocity = FVector2D::ZeroVector;
        ReturnSeconds = 0.f;
        ReturnStartRotation = BoardPivot->GetRelativeRotation().Quaternion();
    }
    ReturnSeconds = FMath::Min(ReturnSeconds + DeltaSeconds, ReturnDuration);
    const float Alpha = FMath::SmoothStep(0.f, 1.f, ReturnSeconds / ReturnDuration);
    BoardPivot->SetRelativeRotation(FQuat::Slerp(ReturnStartRotation, HomeBoardRotation, Alpha).GetNormalized());
    if (ReturnSeconds >= ReturnDuration)
    {
        BoardPivot->SetRelativeRotation(HomeBoardRotation);
        bHasUserRotation = bReturningBoard = false;
        RotationIdleSeconds = 0.f;
        // 未飞散时这次调用为空操作，当前棋谱和正在进行的走子动画继续正常运行。
        Demo->AdvanceAfterScatter();
    }
}
