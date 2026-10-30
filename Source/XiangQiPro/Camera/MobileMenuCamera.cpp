// Copyright 2026 Ultimate Player All Rights Reserved.
#include "MobileMenuCamera.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "XiangQiPro/GameObject/MenuChessDemoComponent.h"
#include "XiangQiPro/UI/Main/UI_Main_Base.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/XQP_HUD.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"

AMobileMenuCamera::AMobileMenuCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
    Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
    Board->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Mesh/ChessBoard/SM_ChessBoard_2P"));
    Board->SetStaticMesh(Mesh.Object);
    // This mesh's long edge is its local Y axis; the game coordinates use X for ranks.
    Board->SetRelativeRotation(FRotator(0, 90, 0));
    Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("MobileMenuCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bConstrainAspectRatio = false;
    Camera->bOverrideAspectRatioAxisConstraint = true;
    Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
    Camera->FieldOfView = 42.f;
    Camera->PostProcessSettings.bOverride_DepthOfFieldScale = true;
    Camera->PostProcessSettings.DepthOfFieldScale = 0.f;
    Demo = CreateDefaultSubobject<UMenuChessDemoComponent>(TEXT("ChessDemo"));
    Music = CreateDefaultSubobject<UAudioComponent>(TEXT("Music"));
    Music->SetupAttachment(Board);
    Music->bIsUISound = true;
    static ConstructorHelpers::FObjectFinder<USoundBase> MenuMusic(TEXT("/Game/Audio/Lobby"));
    Music->SetSound(MenuMusic.Object);
}

void AMobileMenuCamera::BeginPlay()
{
    Super::BeginPlay();
    FitBoard();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    PC->bAutoManageActiveCameraTarget = false;
    PC->SetViewTarget(this);
    PC->bShowMouseCursor = false;
    PC->bEnableClickEvents = true;
    if (auto* HUD = Cast<AXQP_HUD>(PC->GetHUD()))
        if (auto* Menu = XQPMobile::CreateWidget<UUI_Main_Base>(PC, HUD->Class_Main_Base))
        {
            Menu->Init(nullptr);
            Caption = Cast<UTextBlock>(Menu->GetWidgetFromName(TEXT("DemoCaption")));
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
    const FVector Right = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
    const FVector Up = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
    const FVector Center = GetActorLocation() + FVector(0, 0, 2.8f);
    const double TanV = TanH * Viewport.Y / Viewport.X;
    double Distance = 1.f;
    // Fit all eight corners, including the pieces, while keeping the requested angles fixed.
    for (double X : {-44., 44.})
        for (double Y : {-36., 36.})
            for (double Z : {-2., 3.})
            {
                const FVector Corner(X, Y, Z);
                const double Depth = FVector::DotProduct(Corner, Forward);
                Distance = FMath::Max(Distance, FMath::Abs(FVector::DotProduct(Corner, Right)) / (TanH * Frame.GetSize().X / Viewport.X) - Depth);
                Distance = FMath::Max(Distance, FMath::Abs(FVector::DotProduct(Corner, Up)) / (TanV * Frame.GetSize().Y / Viewport.Y) - Depth);
            }
    Camera->SetWorldLocationAndRotation(Center - Forward * (Distance * 1.05), Rotation);
    LastViewport = Viewport;
    LastInsets = Safe;
}

void AMobileMenuCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!LastViewport.Equals(XQPMobile::ViewportSize(this)) || LastInsets != XQPMobile::SafeInsets(this)) FitBoard();
    if (Caption && !Caption->GetText().EqualTo(Demo->GetCaption())) Caption->SetText(Demo->GetCaption());
}

void AMobileMenuCamera::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    OutResult.OffCenterProjectionOffset = LensShift;
}
