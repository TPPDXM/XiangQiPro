// Copyright 2026 Ultimate Player All Rights Reserved.
#include "MenuChessDemoComponent.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Sphere.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/Collision/PBDCollisionSolverSettings.h"
#include "Physics/Experimental/ChaosInterfaceUtils.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsAdapters.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsSimulation.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/BodySetup.h"

// 与游戏世界分离的刚体场景，拥有其中所有 ActorHandle。只在飞散的几秒内分配，
// 重建棋谱或离开主界面时整体销毁；指针不会交给世界场景，也不会写进正式棋局。
struct FMenuScatterSimulation
{
    ImmediatePhysics::FSimulation Scene;
    ImmediatePhysics::FActorHandle* Board = nullptr;
    ImmediatePhysics::FActorHandle* Background = nullptr;
    TArray<ImmediatePhysics::FActorHandle*> Pieces;
    TArray<float> RestSeconds;
    TArray<bool> SleepingPieces;
    TArray<FVector> BoardContactNormals;
    FTransform PreviousBackgroundTransform;
    // 预分配移动端支持的十个触点，按触点编号独立开启碰撞，不在物理步中增删刚体。
    struct FFinger
    {
        ImmediatePhysics::FActorHandle* Actor = nullptr;
        FVector Target = FVector::ZeroVector;
        bool bActive = false;
    };
    FFinger Fingers[10];
    static constexpr float FingerRadius = 4.5f;
};

namespace
{
    // 形状来自资产烘焙数据，不读取渲染顶点，Android 打包后也无需 CPU 网格或运行时烘焙。
    // 简单模式只使用模型凸壳，复杂模式只使用模型三角面；不会叠加另一套包围盒。
    bool BuildScatterGeometry(UBodySetup* Body, const FTransform& Transform,
        ECollisionTraceFlag CollisionType, ImmediatePhysics::FActorSetup& Setup)
    {
        FGeometryAddParams Params;
        Params.bDoubleSided = Body->bDoubleSidedGeometry;
        Params.CollisionTraceType = CollisionType;
        Params.Scale = Transform.GetScale3D();
        Params.SimpleMaterial = nullptr;
        Params.LocalTransform = FTransform::Identity;
        Params.WorldTransform = Transform;
        Params.Geometry = &Body->AggGeom;
        Params.TriMeshGeometries = MakeArrayView(Body->TriMeshGeometries);
        // UE 5.8 的独立物理适配器也使用此过滤器；实际配对由局部场景显式注册。
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Params.CollisionData.CollisionFilterData.SimpleShapeFilterData =
            Chaos::Filter::FShapeFilterBuilder::BuildBlockAll(Chaos::EFilterFlags::All);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Params.CollisionData.CollisionFilterData.ComplexShapeFilterData =
            Params.CollisionData.CollisionFilterData.SimpleShapeFilterData;
        Params.CollisionData.CollisionFlags.bEnableSimCollisionSimple = true;
        Params.CollisionData.CollisionFlags.bEnableSimCollisionComplex = true;
        Params.CollisionData.CollisionFlags.bEnableQueryCollision = false;
        TArray<Chaos::FImplicitObjectPtr> Geometry;
        Chaos::FShapesArray Shapes;
        ChaosInterface::CreateGeometry(Params, Geometry, Shapes);
        if (Geometry.IsEmpty()) return false;
        Setup.Shapes.Reset();
        for (auto& Shape : Shapes) Setup.Shapes.Emplace(MoveTemp(Shape));
        Setup.Geometry = Geometry.Num() == 1 ? MoveTemp(Geometry[0])
            : MakeImplicitObjectPtr<Chaos::FImplicitObjectUnion>(MoveTemp(Geometry));
        Setup.Transform = Transform;
        return true;
    }

    // 从资产已烘焙的模型三角面创建运动学表面。明确指定 ComplexAsSimple，
    // 避免棋盘外接盒抹掉模型轮廓；不改写共享网格的 BodySetup 或世界碰撞设置。
    ImmediatePhysics::FActorHandle* CreateScatterSurface(FMenuScatterSimulation& Simulation, UStaticMeshComponent* Mesh)
    {
        if (!Mesh || !Mesh->GetStaticMesh()) return nullptr;
        UBodySetup* Body = Mesh->GetStaticMesh()->GetBodySetup();
        if (!Body) return nullptr;
        Body->CreatePhysicsMeshes();
        if (Body->TriMeshGeometries.IsEmpty()) return nullptr;
        ImmediatePhysics::FActorSetup Setup;
        if (!BuildScatterGeometry(Body, Mesh->GetComponentTransform(), CTF_UseComplexAsSimple, Setup)) return nullptr;
        Setup.ActorType = ImmediatePhysics::EActorType::KinematicActor;
        Setup.CollisionEnabled = ECollisionEnabled::PhysicsOnly;
        Setup.Material = MakeUnique<Chaos::FChaosPhysicsMaterial>();
        Setup.Material->Friction = .85f;
        Setup.Material->StaticFriction = 1.f;
        Setup.Material->Restitution = .05f;
        auto* Actor = Simulation.Scene.CreateActor(MoveTemp(Setup));
        // ImmediatePhysics 创建粒子时会重新生成 ShapesArray，未保留 Setup 的
        // CollisionTraceType。必须在最终形状上恢复复杂面参与刚体接触的标志。
        for (auto& Shape : Actor->GetParticle()->ShapesArray())
            Shape->SetCollisionTraceType(Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseComplexAsSimple);
        return Actor;
    }

    constexpr float MoveSeconds = .65f;
    EChessType Type(char Letter)
    {
        switch (Letter)
        {
        case 'K': return EChessType::JIANG; case 'A': return EChessType::SHI;
        case 'E': return EChessType::XIANG; case 'H': return EChessType::MA;
        case 'R': return EChessType::JV; case 'C': return EChessType::PAO;
        case 'P': return EChessType::BING; default: return EChessType::EMPTY;
        }
    }
}

FMenuChessGame UMenuChessDemoComponent::MakeGame(int32 Index)
{
    FMenuChessGame Result;
    auto Put = [&](int32 Row, int32 Column, EChessColor Color, char Letter)
    { Result.Position.Set({Row, Column}, {Type(Letter), Color}); };
    const EChessColor Red = EChessColor::REDCHESS, Black = EChessColor::BLACKCHESS;
    if (Index == 0)
    {

        for (EChessColor Color : {Red, Black})
        {
            const int32 Row = Color == Red ? 0 : 9;
            const char* Back = "RHEAKAEHR";
            for (int32 C = 0; C < 9; ++C) Put(Row, C, Color, Back[C]);
            for (int32 C : {1, 7}) Put(Color == Red ? 2 : 7, C, Color, 'C');
            for (int32 C : {0, 2, 4, 6, 8}) Put(Color == Red ? 3 : 6, C, Color, 'P');
        }
        Result.Moves = {{{2,1},{9,1}},{{6,2},{5,2}},{{2,7},{9,7}},{{6,4},{5,4}},{{9,7},{9,5}},{{5,4},{4,4}},
            {{9,5},{9,8}},{{9,6},{7,4}},{{9,8},{9,3}},{{4,4},{3,4}},{{9,3},{8,3}},{{9,2},{7,0}},
            {{8,3},{9,3}},{{9,0},{9,1}},{{0,0},{2,0}},{{6,6},{5,6}},{{2,0},{2,4}},{{5,6},{4,6}},
            {{2,4},{3,4}},{{4,6},{3,6}},{{3,4},{7,4}},{{9,4},{9,5}},{{7,4},{7,5}}};
    }
    else
    {
        Put(0,4,Red,'K'); Put(9,4,Black,'K'); Put(5,4,Red,'P');
        if (Index == 1)
        {

            Put(6,0,Red,'R'); Put(5,8,Red,'R'); Put(8,8,Black,'R'); Put(7,0,Black,'P');
            Result.Moves = {{{6,0},{6,4}},{{9,4},{9,5}},{{6,4},{9,4}},{{9,5},{8,5}},{{5,8},{5,5}}};
        }
        else if (Index == 2)
        {

            Put(6,2,Red,'H'); Put(6,6,Red,'C'); Put(8,3,Black,'A'); Put(9,5,Black,'A');
            Result.Moves = {{{6,6},{9,6}},{{9,4},{8,4}},{{6,2},{8,3}},{{8,4},{7,4}},{{8,3},{9,5}},{{7,4},{7,3}},{{9,6},{7,6}}};
        }
        else
        {

            Put(6,1,Red,'R'); Put(5,6,Red,'H'); Put(9,5,Black,'A'); Put(7,6,Black,'E');
            Result.Moves = {{{6,1},{9,1}},{{9,4},{8,4}},{{9,1},{9,4}},{{8,4},{8,3}},{{5,6},{7,5}},
                {{8,3},{7,3}},{{9,4},{7,4}},{{7,3},{7,4}},{{5,4},{6,4}},{{7,4},{7,3}},{{6,4},{6,3}}};
            // Reverse the third ending so the black side also wins a demonstration.
            FChessPosition Mirrored;
            for (int32 I = 0; I < 90; ++I)
                if (Result.Position.Squares[I].Type != EChessType::EMPTY)
                {
                    auto Piece = Result.Position.Squares[I];
                    Piece.Color = FChessPosition::Opponent(Piece.Color);
                    Mirrored.Set({9 - I / 9, 8 - I % 9}, Piece);
                }
            Result.Position = Mirrored;
            for (auto& Move : Result.Moves)
            {
                Move.from = {9 - Move.from.X, 8 - Move.from.Y};
                Move.to = {9 - Move.to.X, 8 - Move.to.Y};
            }
            Result.FirstSide = Black;
        }
    }
    return Result;
}

UMenuChessDemoComponent::UMenuChessDemoComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(PATH_PACKAGE_SM_CHESS);
    PieceMesh = Mesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LabelMaterial(PATH_M_MENU_CHESSMASK);
    MenuLabelMaterial = LabelMaterial.Object;
}

UMenuChessDemoComponent::~UMenuChessDemoComponent() = default;

void UMenuChessDemoComponent::BeginPlay()
{
    Super::BeginPlay();
    ResetBoard();
}

FVector UMenuChessDemoComponent::SquareLocation(FIntPoint Square) const
{
    const FVector Local(-38.f + Square.X * (76.f / 9.f), -30.f + Square.Y * 7.5f, 2.8f);
    return PresentationRoot ? Local : GetOwner()->GetActorLocation() + Local;
}

void UMenuChessDemoComponent::SetPieceLocation(UStaticMeshComponent* Piece, const FVector& Location) const
{
    if (!Piece) return;
    if (PresentationRoot) Piece->SetRelativeLocation(Location);
    else Piece->SetWorldLocation(Location);
}

void UMenuChessDemoComponent::ClearPieces()
{
    for (const auto& Label : Labels) if (Label) Label->DestroyComponent();
    for (const auto& Piece : Pieces) if (Piece) Piece->DestroyComponent();
    Labels.Empty(); Pieces.Empty();
    ScatterSimulation.Reset();
    ScatterBoard.Reset();
    ScatterBackground.Reset();
    bPiecesScattered = false;
}

void UMenuChessDemoComponent::ResetBoard()
{
    ClearPieces();
    Game = MakeGame(Episode);
    Position = Game.Position;
    Side = Game.FirstSide;
    Ply = 0; Elapsed = 0; Wait = 2.5f; bMoving = bFinished = false;
    Pieces.SetNumZeroed(90); Labels.SetNumZeroed(90);
    const TCHAR* RedNames[] = {TEXT(""),TEXT("shuai"),TEXT("shi"),TEXT("xiang"),TEXT("ma"),TEXT("jv"),TEXT("pao"),TEXT("bing")};
    const TCHAR* BlackNames[] = {TEXT(""),TEXT("jiang"),TEXT("shi"),TEXT("xiang"),TEXT("ma"),TEXT("jv"),TEXT("pao"),TEXT("zu")};
    for (int32 I = 0; I < 90; ++I)
    {
        const auto Piece = Position.Squares[I];
        if (Piece.Type == EChessType::EMPTY) continue;
        auto* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
        Mesh->SetStaticMesh(PieceMesh);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // 悬浮展示不向下方纸面投影；每次新棋谱重建时也应用，避免恢复阴影。
        if (PresentationRoot) Mesh->SetCastShadow(false);
        Mesh->SetupAttachment(PresentationRoot ? PresentationRoot.Get() : GetOwner()->GetRootComponent());
        Mesh->RegisterComponent();
        if (PresentationRoot) Mesh->SetRelativeRotation(FRotator::ZeroRotator);
        else Mesh->SetWorldRotation(FRotator::ZeroRotator);
        SetPieceLocation(Mesh, SquareLocation({I / 9, I % 9}));
        Pieces[I] = Mesh;
        auto* Label = NewObject<UDecalComponent>(GetOwner());
        Label->SetupAttachment(Mesh);
        Label->SetRelativeLocation(FVector(0,0,1));
        Label->SetRelativeScale3D(FVector(.0025f,.008f,.008f));
        const bool IsRed = Piece.Color == EChessColor::REDCHESS;
        Label->SetRelativeRotation(FRotator(-90, IsRed ? 90 : -90, 0));
        const FString MaterialPath = FString::Printf(PATH_MI_CHESSMASK_FORMAT,
            IsRed ? TEXT("red") : TEXT("black"), IsRed ? TEXT("rad") : TEXT("black"),
            IsRed ? RedNames[static_cast<int32>(Piece.Type)] : BlackNames[static_cast<int32>(Piece.Type)]);
        UMaterialInterface* SourceLabel = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (PresentationRoot && MenuLabelMaterial)
        {
            // 独立菜单材质继承每个字样实例的完整纹理/颜色参数；不改正式棋子的贴花材质。
            auto* Material = UMaterialInstanceDynamic::Create(MenuLabelMaterial, Label);
            Material->CopyMaterialUniformParameters(SourceLabel);
            Material->SetScalarParameterValue(TEXT("MenuOpacity"), 1.f);
            Label->SetDecalMaterial(Material);
        }
        else Label->SetDecalMaterial(SourceLabel);
        Label->SetFadeScreenSize(0.f);
        Label->RegisterComponent();
        Labels[I] = Label;
    }
}

void UMenuChessDemoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bResetTransition)
    {
        // 暂停棋谱和散落模拟，所有棋子及字样在同一时间轴上渐隐/渐显。
        // 不提前销毁散落棋子；换盘只发生在淡出完全结束的那一帧。
        constexpr float FadeSeconds = .35f;
        TransitionTime = FMath::Min(TransitionTime + DeltaTime, FadeSeconds);
        const float Alpha = FMath::SmoothStep(0.f, 1.f, TransitionTime / FadeSeconds);
        SetPieceOpacity(bFadingIn ? Alpha : 1.f - Alpha);
        if (TransitionTime >= FadeSeconds)
        {
            if (!bFadingIn)
            {
                Episode = Episode % 3 + 1;
                ResetBoard();
                SetPieceOpacity(0.f);
                bFadingIn = true;
                TransitionTime = 0.f;
            }
            else bResetTransition = false;
        }
        return;
    }
    if (bPiecesScattered)
    {
        TickScatteredPieces(DeltaTime);
        return; // 飞散期间冻结棋谱，不对已脱离棋盘的组件继续写局部走子坐标。
    }
    if (!bMoving)
    {
        Wait -= DeltaTime;
        if (Wait > 0) return;
        if (bFinished) { BeginResetTransition(); return; }
        if (!Game.Moves.IsValidIndex(Ply)) return;
        Moving = Game.Moves[Ply];
        // Validate the replay against the same rules used by real matches.
        if (!Position.GenerateLegalMoves(Side).Contains(Moving))
        {
            UE_LOG(LogTemp, Error, TEXT("Menu demo %d has an illegal move at ply %d"), Episode, Ply);
            SetComponentTickEnabled(false); return;
        }
        bMoving = true; Elapsed = 0;
    }
    Elapsed += DeltaTime;
    const float Alpha = FMath::Clamp(Elapsed / MoveSeconds, 0.f, 1.f);
    const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha);
    if (UStaticMeshComponent* Mesh = Pieces[Moving.from.X * 9 + Moving.from.Y])
        SetPieceLocation(Mesh, FMath::Lerp(SquareLocation(Moving.from), SquareLocation(Moving.to), Smooth)
            + FVector(0, 0, FMath::Sin(PI * Alpha) * 3.5f));
    if (Alpha >= 1.f) FinishMove();
}

void UMenuChessDemoComponent::FinishMove()
{
    const int32 From = Moving.from.X * 9 + Moving.from.Y, To = Moving.to.X * 9 + Moving.to.Y;
    if (Labels[To]) Labels[To]->DestroyComponent();
    if (Pieces[To]) Pieces[To]->DestroyComponent();
    Pieces[To] = Pieces[From]; Pieces[From] = nullptr;
    Labels[To] = Labels[From]; Labels[From] = nullptr;
    Position.MakeMove(Moving);
    Side = FChessPosition::Opponent(Side);
    ++Ply; bMoving = false;
    bFinished = Position.GenerateLegalMoves(Side).IsEmpty();
    Wait = bFinished ? 3.5f : (Side == EChessColor::REDCHESS ? 1.35f : 1.65f);
}

void UMenuChessDemoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPieces();
    Super::EndPlay(EndPlayReason);
}


void UMenuChessDemoComponent::ScatterPieces(UStaticMeshComponent* InBoard, const FVector& AngularVelocity, UStaticMeshComponent* InBackground)
{
    if (bResetTransition || bPiecesScattered || !PresentationRoot || !InBoard || !PieceMesh) return;
    UBodySetup* PieceBody = PieceMesh->GetBodySetup();
    if (!PieceBody || PieceBody->AggGeom.ConvexElems.IsEmpty() || PieceBody->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
    {
        UE_LOG(LogTemp, Warning, TEXT("Menu scatter requires convex collision on the chess piece mesh"));
        return;
    }
    PieceBody->CreatePhysicsMeshes();
    auto Simulation = MakePimpl<FMenuScatterSimulation>();
    Simulation->Board = CreateScatterSurface(*Simulation, InBoard);
    Simulation->Background = CreateScatterSurface(*Simulation, InBackground);
    // 任一要求的碰撞模型缺失时保持正常演示，不退回外接盒或让棋子掉穿背景。
    if (!Simulation->Board || (InBackground && !Simulation->Background))
    {
        UE_LOG(LogTemp, Warning, TEXT("Menu scatter requires cooked triangle collision on the board and background"));
        return;
    }
    Simulation->Pieces.SetNumZeroed(Pieces.Num());
    Simulation->RestSeconds.SetNumZeroed(Pieces.Num());
    Simulation->SleepingPieces.SetNumZeroed(Pieces.Num());
    Simulation->BoardContactNormals.SetNumZeroed(Pieces.Num());
    if (InBackground) Simulation->PreviousBackgroundTransform = InBackground->GetComponentTransform();
    Simulation->Scene.SetSolverSettings(1.f / 120.f, .2f, FMath::Max(0.f, MaxSeparationSpeed), -1, 8, 4, 1, 1);
    const FVector Center = PresentationRoot->GetComponentLocation();
    const FVector Up = PresentationRoot->GetUpVector();
    for (int32 I = 0; I < Pieces.Num(); ++I)
    {
        UStaticMeshComponent* Piece = Pieces[I];
        if (!Piece) continue;
        // 仅把资产凸壳转换到独立场景。临时 BodyInstance 从不 InitBody，因而不会
        // 创建世界刚体；真实接触力矩会让侧边着地的棋子继续翻落，不再任意角度冻结。
        FBodyInstance Body;
        Body.BodySetup = PieceBody;
        Body.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly, false);
        auto Setup = ImmediatePhysics::MakeDynamicActorSetup(&Body, Piece->GetComponentTransform());
        // 适配器提供凸壳质量、重心与惯量；重新指定简单几何，排除查询用三角面。
        // 若资产以后改成 ComplexAsSimple，则不能作为动态棋子使用。
        BuildScatterGeometry(PieceBody, Piece->GetComponentTransform(), CTF_UseSimpleAsComplex, Setup);
        Setup.LinearDamping = FMath::Max(0.f, ScatterLinearDamping);
        Setup.AngularDamping = FMath::Max(0.f, ScatterAngularDamping);
        Setup.Material->Friction = .85f;
        Setup.Material->StaticFriction = 1.f;
        Setup.Material->Restitution = .05f;
        auto* Actor = Simulation->Scene.CreateActor(MoveTemp(Setup));
        for (auto& Shape : Actor->GetParticle()->ShapesArray())
            Shape->SetCollisionTraceType(Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseSimpleAsComplex);
        Simulation->Pieces[I] = Actor;
        Piece->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        FRandomStream Random(Episode * 7919 + Ply * 97 + I * 13 + 1);
        FVector Radial = FVector::VectorPlaneProject(Piece->GetComponentLocation() - Center, Up).GetSafeNormal();
        if (Radial.IsNearlyZero()) Radial = PresentationRoot->GetForwardVector();
        // 保留转动传来的方向；继承比例和随机速度范围可在演示组件实例中调整。
        // 限速避免猛划瞬间把全部棋子炸出画面；之后被运动棋盘碰到也受同一限速保护。
        // 范围端点允许在详情面板中交换顺序；同时防御运行时写入的负值。
        const auto SampleSpeed = [&Random](const FVector2D& Range)
        {
            return Random.FRandRange(FMath::Max(0., FMath::Min(Range.X, Range.Y)),
                FMath::Max(0., FMath::Max(Range.X, Range.Y)));
        };
        Actor->SetLinearVelocity((FVector::CrossProduct(AngularVelocity, Piece->GetComponentLocation() - Center)
            * FMath::Max(0.f, TangentialVelocityScale) + Radial * SampleSpeed(OutwardSpeedRange)
            + Up * SampleSpeed(LiftSpeedRange)).GetClampedToMaxSize(FMath::Max(0.f, InitialLinearSpeedLimit)));
        Actor->SetAngularVelocity((AngularVelocity * FMath::Max(0.f, InheritedSpinScale)
            + Random.VRand() * SampleSpeed(RandomSpinSpeedRange)).GetClampedToMaxSize(FMath::Max(0.f, InitialAngularSpeedLimit)));
        Actor->SetMaxLinearVelocitySquared(FMath::Square(FMath::Max(0.f, FlightLinearSpeedLimit)));
        Actor->SetMaxAngularVelocitySquared(FMath::Square(FMath::Max(0.f, FlightAngularSpeedLimit)));
        Actor->SetMaxDepenetrationVelocity(FMath::Max(0.f, MaxSeparationSpeed));
        Actor->SetCCDEnabled(true);
    }
    // 手指是局部物理场景中的运动学球体，不渲染、不加入世界碰撞通道。
    for (auto& Finger : Simulation->Fingers)
    {
        ImmediatePhysics::FActorSetup FingerSetup;
        FingerSetup.ActorType = ImmediatePhysics::EActorType::KinematicActor;
        FingerSetup.CollisionEnabled = ECollisionEnabled::PhysicsOnly;
        FingerSetup.Geometry = MakeImplicitObjectPtr<Chaos::TSphere<Chaos::FReal, 3>>(
            Chaos::FVec3(0), FMenuScatterSimulation::FingerRadius);
        FingerSetup.Transform = FTransform(InBackground ? InBackground->GetComponentLocation() : Center);
        FingerSetup.Material = MakeUnique<Chaos::FChaosPhysicsMaterial>();
        FingerSetup.Material->Friction = .6f;
        FingerSetup.Material->Restitution = 0.f;
        Finger.Actor = Simulation->Scene.CreateActor(MoveTemp(FingerSetup));
    }
    // 每颗棋子都注册与其它动态棋子的接触，保持表面之间互不碰撞。
    // 这些配对始终留在同一个菜单专用场景，不会影响正式棋局或共享网格资源。
    // 新建的 ImmediatePhysics 动态体默认 Disabled；显式启用后才能进入接触配对。
    // UE 5.8 默认按 ActiveBodyIndices 激活，只有数量而索引为空会把所有棋子
    // 标记为 Disabled：轻量积分器仍更新弹道，但碰撞配对会全部被剔除。
    TArray<int32> ActiveBodies;
    for (int32 Index = 0; Index < Simulation->Scene.NumActors(); ++Index) ActiveBodies.Add(Index);
    Simulation->Scene.SetNumActiveBodies(ActiveBodies.Num(), ActiveBodies);
    Simulation->Scene.AddToCollidingPairs(Simulation->Board);
    if (Simulation->Background) Simulation->Scene.AddToCollidingPairs(Simulation->Background);
    for (auto* PieceActor : Simulation->Pieces)
        if (PieceActor) Simulation->Scene.AddToCollidingPairs(PieceActor);
    for (auto& Finger : Simulation->Fingers)
    {
        Simulation->Scene.AddToCollidingPairs(Finger.Actor);
        Simulation->Scene.SetHasCollision(Finger.Actor, false);
    }
    // 自适应子步要求求解器精确使用本次步长，不能沿用布娃娃的平滑步长或最小步长。
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Simulation->Scene.SetMaxNumRollingAverageStepTimes(1);
    Simulation->Scene.SetUseMinStepTime(false);
    Simulation->Scene.SetUseFixedStepTolerance(false);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    ScatterBoard = InBoard;
    ScatterBackground = InBackground;
    ScatterSimulation = MoveTemp(Simulation);
    bPiecesScattered = true;
}

void UMenuChessDemoComponent::TickScatteredPieces(float DeltaTime)
{
    if (!ScatterSimulation || !ScatterBoard.IsValid() || DeltaTime <= 0.f) return;
    auto& Simulation = *ScatterSimulation;
    // CCD 对运动学三角网格的大角度旋转并不足够。按“最远盘角 + 最快棋子”
    // 每步最多移动 0.5cm 细分，并逐步设置棋盘目标，避免薄棋盘跨过棋子。
    // 同样细分手指的扫动路径，快速划过时也不会直接跳到棋子的另一侧。
    const FTransform BoardStart = Simulation.Board->GetWorldTransform();
    const FTransform BoardEnd = ScatterBoard->GetComponentTransform();
    const FTransform BackgroundStart = Simulation.Background ? Simulation.Background->GetWorldTransform() : FTransform::Identity;
    const FTransform BackgroundEnd = ScatterBackground.IsValid() ? ScatterBackground->GetComponentTransform() : BackgroundStart;
    FVector FingerStarts[10];
    double FingerTravel = 0.;
    bool bFingerActive = false;
    for (int32 I = 0; I < UE_ARRAY_COUNT(Simulation.Fingers); ++I)
    {
        const auto& Finger = Simulation.Fingers[I];
        FingerStarts[I] = Finger.Actor->GetWorldTransform().GetLocation();
        if (Finger.bActive)
        {
            bFingerActive = true;
            FingerTravel = FMath::Max(FingerTravel, FVector::Distance(FingerStarts[I], Finger.Target));
        }
    }
    const float Duration = FMath::Min(DeltaTime, .1f);
    const double BoardTravel = BoardStart.GetRotation().AngularDistance(BoardEnd.GetRotation()) * ScatterBoard->Bounds.SphereRadius
        + FVector::Distance(BoardStart.GetLocation(), BoardEnd.GetLocation());
    const double BackgroundTravel = FVector::Distance(BackgroundStart.GetLocation(), BackgroundEnd.GetLocation())
        + BackgroundStart.GetRotation().AngularDistance(BackgroundEnd.GetRotation())
            * (ScatterBackground.IsValid() ? ScatterBackground->Bounds.SphereRadius : 0.);
    double PieceSpeed = 0.;
    bool bMovingPiece = false;
    for (int32 I = 0; I < Simulation.Pieces.Num(); ++I)
        if (auto* Actor = Simulation.Pieces[I])
        {
            PieceSpeed = FMath::Max(PieceSpeed, Actor->GetLinearVelocity().Size());
            bMovingPiece |= !Simulation.SleepingPieces[I] && Actor->GetLinearVelocity().SizeSquared() > .01;
        }
    const int32 Steps = FMath::Max(FMath::CeilToInt(Duration * 120.f),
        FMath::CeilToInt((FMath::Max3(BoardTravel, FingerTravel, BackgroundTravel) + PieceSpeed * Duration) / .5));
    const float StepTime = Duration / FMath::Max(1, Steps);
    Simulation.Scene.SetSolverSettings(StepTime, .2f, FMath::Max(0.f, MaxSeparationSpeed), -1, 8, 4, 1, 1);
    // 睡眠体也必须能被另一颗活动棋子、扫动的棋盘或手指唤醒，否则互碰仍会穿透。
    if (BoardTravel > .001 || BackgroundTravel > .001 || bFingerActive || bMovingPiece)
        for (int32 I = 0; I < Simulation.Pieces.Num(); ++I)
            if (Simulation.Pieces[I] && Simulation.SleepingPieces[I])
            {
                Simulation.Pieces[I]->GetParticle()->CastToRigidParticle()->SetSleeping(false);
                Simulation.SleepingPieces[I] = false;
                Simulation.RestSeconds[I] = 0.f;
            }
    auto ContactSettings = Simulation.Scene.GetCollisionSolverSettings();
    ContactSettings.NumPositionFrictionIterations = 4;
    ContactSettings.NumVelocityFrictionIterations = 1;
    ContactSettings.MaxPushOutVelocity = FMath::Max(0.f, MaxSeparationSpeed);
    TArray<FVector> PieceStepStarts;
    PieceStepStarts.SetNum(Simulation.Pieces.Num());
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const FTransform PreviousBoard = Simulation.Board->GetWorldTransform();
        if (BoardTravel > .001)
            for (int32 I = 0; I < Simulation.Pieces.Num(); ++I)
                if (Simulation.Pieces[I]) PieceStepStarts[I] = Simulation.Pieces[I]->GetWorldTransform().GetLocation();
        const float Alpha = float(Step) / Steps;
        FTransform Target;
        Target.Blend(BoardStart, BoardEnd, Alpha);
        Simulation.Board->SetKinematicTarget(Target);
        if (Simulation.Background)
        {
            Target.Blend(BackgroundStart, BackgroundEnd, Alpha);
            Simulation.Background->SetKinematicTarget(Target);
        }
        for (int32 I = 0; I < UE_ARRAY_COUNT(Simulation.Fingers); ++I)
            if (Simulation.Fingers[I].bActive)
                Simulation.Fingers[I].Actor->SetKinematicTarget(FTransform(FMath::Lerp(FingerStarts[I], Simulation.Fingers[I].Target, Alpha)));
        Simulation.Scene.Simulate(StepTime, StepTime, 1, FVector(0, 0, -FMath::Max(0.f, ScatterGravity)), nullptr, &ContactSettings);
        if (BoardTravel > .001)
        {
            // 极速转盘的运动学表面速度可能远高于棋子限速，仅靠接触冲量仍可能
            // 把表面压过棋子。对本子步的相对路径再做模型三角面射线，若中心确实
            // 穿越了盘面，则按真实凸壳支撑距离投回原接触侧。几何仍来自烘焙资产，
            // 不用外接盒代替棋盘；位置约束也不要求取消飞行速度上限。
            const FTransform CurrentBoard = Simulation.Board->GetWorldTransform();
            for (int32 I = 0; I < Simulation.Pieces.Num(); ++I)
            {
                auto* Actor = Simulation.Pieces[I];
                if (!Actor || !Actor->GetHasCollision()) continue;
                FTransform PieceTransform = Actor->GetWorldTransform();
                const FVector LocalStart = PreviousBoard.InverseTransformPositionNoScale(PieceStepStarts[I]);
                const FVector LocalEnd = CurrentBoard.InverseTransformPositionNoScale(PieceTransform.GetLocation());
                const FVector Travel = LocalEnd - LocalStart;
                const double Length = Travel.Size();
                if (Length < UE_SMALL_NUMBER) continue;
                Chaos::FReal HitTime;
                Chaos::FVec3 HitPoint, HitNormal;
                int32 Face;
                const bool bCrossed = Simulation.Board->GetParticle()->GetGeometry()->Raycast(
                    LocalStart, Travel / Length, Length, 0.f, HitTime, HitPoint, HitNormal, Face);
                if (bCrossed)
                {
                    if (FVector::DotProduct(HitNormal, Travel) > 0.) HitNormal = -HitNormal;
                    Simulation.BoardContactNormals[I] = HitNormal;
                }
                else
                {
                    // 中心尚未再次穿面时，也要沿已接触侧检查完整凸壳。否则限速后的
                    // 棋子虽没有换侧，盘面仍可能逐步压进半个棋子，直到下一次中心穿越。
                    HitNormal = Simulation.BoardContactNormals[I];
                    if (HitNormal.IsNearlyZero()) continue;
                    const float Radius = PieceMesh->GetBounds().SphereRadius * Pieces[I]->GetComponentScale().GetAbsMax() + 1.f;
                    Chaos::FVec3 SurfaceNormal;
                    if (!Simulation.Board->GetParticle()->GetGeometry()->Raycast(
                        LocalEnd + HitNormal * Radius, -HitNormal, Radius * 2.f, 0.f, HitTime, HitPoint, SurfaceNormal, Face))
                    {
                        Simulation.BoardContactNormals[I] = FVector::ZeroVector;
                        continue;
                    }
                }
                const FVector Normal = CurrentBoard.TransformVectorNoScale(HitNormal).GetSafeNormal();
                const FVector Contact = CurrentBoard.TransformPositionNoScale(HitPoint);
                const FVector Center = PieceTransform.GetLocation();
                PieceTransform.SetScale3D(Pieces[I]->GetComponentScale());
                double MinSupport = 0.;
                for (const auto& Hull : PieceMesh->GetBodySetup()->AggGeom.ConvexElems)
                    for (const FVector& Vertex : Hull.VertexData)
                        MinSupport = FMath::Min(MinSupport, FVector::DotProduct(
                            PieceTransform.TransformPosition(Hull.GetTransform().TransformPosition(Vertex)) - Center, Normal));
                const double Push = .02 - MinSupport - FVector::DotProduct(Center - Contact, Normal);
                if (Push < -.2) Simulation.BoardContactNormals[I] = FVector::ZeroVector;
                if (Push > 0.)
                {
                    // SetWorldTransform 保留求解后的速度/姿态；缩放已烘进几何，不能重复应用。
                    FTransform Corrected = Actor->GetWorldTransform();
                    Corrected.SetLocation(Center + Normal * Push);
                    Actor->SetWorldTransform(Corrected);
                }
            }
        }
        // 每个子步都限速，避免接触高速运动学表面后，在本帧剩余子步继续失控加速。
        for (auto* Actor : Simulation.Pieces)
            if (Actor)
            {
                Actor->SetLinearVelocity(Actor->GetLinearVelocity().GetClampedToMaxSize(FMath::Max(0.f, FlightLinearSpeedLimit)));
                Actor->SetAngularVelocity(Actor->GetAngularVelocity().GetClampedToMaxSize(FMath::Max(0.f, FlightAngularSpeedLimit)));
            }
    }
    for (int32 I = 0; I < Pieces.Num(); ++I)
    {
        if (UStaticMeshComponent* Piece = Pieces[I])
        {
            if (auto* Actor = Simulation.Pieces[I])
            {
                // ImmediatePhysics 的轻量积分器不使用世界场景的速度上限，故此处
                // 再明确限速，避免用户持续猛转棋盘时通过接触反复加速棋子。
                Actor->SetLinearVelocity(Actor->GetLinearVelocity().GetClampedToMaxSize(FMath::Max(0.f, FlightLinearSpeedLimit)));
                Actor->SetAngularVelocity(Actor->GetAngularVelocity().GetClampedToMaxSize(FMath::Max(0.f, FlightAngularSpeedLimit)));
                const FTransform Transform = Actor->GetWorldTransform();
                // 必须确实由背景模型支撑、已经躺平且持续低速才允许睡眠，不能把
                // 空中或靠棱边站立的棋子锁死。背景射线和凸壳支撑点使用相同的物理模型。
                if (!Simulation.SleepingPieces[I] && Simulation.Background)
                {
                    const FTransform SurfaceTransform = Simulation.Background->GetWorldTransform();
                    const FVector RayStart = SurfaceTransform.InverseTransformPositionNoScale(Transform.GetLocation() + FVector(0, 0, 10));
                    const FVector RayDirection = SurfaceTransform.InverseTransformVectorNoScale(-FVector::UpVector);
                    Chaos::FReal HitTime;
                    Chaos::FVec3 HitPoint, HitNormal;
                    int32 FaceIndex;
                    const bool Hit = Simulation.Background->GetParticle()->GetGeometry()->Raycast(
                        RayStart, RayDirection, 30.f, 0.f, HitTime, HitPoint, HitNormal, FaceIndex);
                    bool Supported = false;
                    if (Hit)
                    {
                        FVector Normal = SurfaceTransform.TransformVectorNoScale(HitNormal).GetSafeNormal();
                        if (Normal.Z < 0.) Normal = -Normal;
                        const FVector Contact = SurfaceTransform.TransformPositionNoScale(HitPoint);
                        double SupportDistance = TNumericLimits<double>::Max();
                        const FTransform PieceTransform(Transform.GetRotation(), Transform.GetLocation(), Piece->GetComponentScale());
                        for (const auto& Hull : PieceMesh->GetBodySetup()->AggGeom.ConvexElems)
                            for (const FVector& Vertex : Hull.VertexData)
                                SupportDistance = FMath::Min(SupportDistance, FVector::DotProduct(
                                    PieceTransform.TransformPosition(Hull.GetTransform().TransformPosition(Vertex)) - Contact, Normal));
                        Supported = Normal.Z > .7 && FMath::Abs(FVector::DotProduct(Transform.GetUnitAxis(EAxis::Z), Normal)) > .995
                            && FMath::Abs(SupportDistance) < .15 && Actor->GetLinearVelocity().SizeSquared() < 9.
                            && Actor->GetAngularVelocity().SizeSquared() < 1.44;
                    }
                    Simulation.RestSeconds[I] = Supported ? Simulation.RestSeconds[I] + FMath::Min(DeltaTime, .1f) : 0.f;
                    if (Simulation.RestSeconds[I] >= .25f)
                    {
                        Actor->SetLinearVelocity(FVector::ZeroVector);
                        Actor->SetAngularVelocity(FVector::ZeroVector);
                        Actor->GetParticle()->CastToRigidParticle()->SetSleeping(true);
                        Simulation.SleepingPieces[I] = true;
                    }
                }
                // 几何已包含组件缩放，仅同步位置/姿态，避免把可见网格再次缩放。
                Piece->SetWorldLocationAndRotation(Transform.GetLocation(), Transform.GetRotation());
                if (Transform.GetLocation().Z < PresentationRoot->GetComponentLocation().Z - 180.f ||
                    FVector::DistSquared(Transform.GetLocation(), PresentationRoot->GetComponentLocation()) > FMath::Square(1000.f))
                    Piece->SetVisibility(false, true);
            }
        }
    }
    if (ScatterBackground.IsValid()) Simulation.PreviousBackgroundTransform = ScatterBackground->GetComponentTransform();
}

bool UMenuChessDemoComponent::UpdateFingerContact(const FVector& RayOrigin, const FVector& RayDirection, int32 FingerIndex, bool bBegin)
{
    if (!ScatterSimulation || !ScatterSimulation->Background || bResetTransition || FingerIndex < 0 || FingerIndex >= 10) return false;
    auto& Simulation = *ScatterSimulation;
    const FTransform Surface = Simulation.Background->GetWorldTransform();
    Chaos::FReal Time;
    Chaos::FVec3 Point, Normal;
    int32 Face;
    if (!Simulation.Background->GetParticle()->GetGeometry()->Raycast(
        Surface.InverseTransformPositionNoScale(RayOrigin),
        Surface.InverseTransformVectorNoScale(RayDirection).GetSafeNormal(), 100000.f, 0.f, Time, Point, Normal, Face))
    {
        EndFingerContact(FingerIndex);
        return false;
    }
    FVector WorldNormal = Surface.TransformVectorNoScale(Normal).GetSafeNormal();
    if (FVector::DotProduct(WorldNormal, RayDirection) > 0.) WorldNormal = -WorldNormal;
    auto& Finger = Simulation.Fingers[FingerIndex];
    Finger.Target = Surface.TransformPositionNoScale(Point) + WorldNormal * FMenuScatterSimulation::FingerRadius;
    // 新的按下没有上一段扫动；不能从旧手指位置扫过半个屏幕。
    if (bBegin || !Finger.bActive)
    {
        Finger.Actor->InitWorldTransform(FTransform(Finger.Target));
        Simulation.Scene.SetHasCollision(Finger.Actor, true);
        Simulation.Scene.AddToCollidingPairs(Finger.Actor);
    }
    Finger.bActive = true;
    return true;
}

void UMenuChessDemoComponent::EndFingerContact(int32 FingerIndex)
{
    if (!ScatterSimulation) return;
    for (int32 I = 0; I < UE_ARRAY_COUNT(ScatterSimulation->Fingers); ++I)
        if (FingerIndex == INDEX_NONE || FingerIndex == I)
        {
            auto& Finger = ScatterSimulation->Fingers[I];
            Finger.bActive = false;
            ScatterSimulation->Scene.SetHasCollision(Finger.Actor, false);
        }
}

void UMenuChessDemoComponent::SetPieceOpacity(float Opacity)
{
    // 复用棋子材质已有的 Fade（0=显示、1=消失），只生成组件自己的 MID。
    // 贴花使用菜单专用 MID 的透明度同步字样，不修改任何共享材质实例。
    for (const auto& Piece : Pieces)
        if (Piece) Piece->SetScalarParameterValueOnMaterials(TEXT("Fade"), 1.f - Opacity);
    for (const auto& Label : Labels)
        if (Label)
            if (auto* Material = Cast<UMaterialInstanceDynamic>(Label->GetDecalMaterial()))
                Material->SetScalarParameterValue(TEXT("MenuOpacity"), Opacity);
}

void UMenuChessDemoComponent::BeginResetTransition()
{
    if (bResetTransition) return;
    if (!PresentationRoot)
    {
        Episode = Episode % 3 + 1;
        ResetBoard();
        return;
    }
    EndFingerContact();
    bResetTransition = true;
    bFadingIn = false;
    TransitionTime = 0.f;
}

void UMenuChessDemoComponent::AdvanceAfterScatter()
{
    BeginResetTransition();
}
