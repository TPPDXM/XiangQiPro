// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chesses.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/GameMode/ProBattleComponent.h"

// Sets default values
AChesses::AChesses()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

	UStaticMesh* Mesh = OM::GetConstructorObject<UStaticMesh>(PATH_SM_CHESS);

	ChessMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChessMeshComponent"));
	ChessMesh->SetStaticMesh(Mesh);
	RootComponent = ChessMesh;

	ChessMask = CreateDefaultSubobject<UDecalComponent>(TEXT("Mask"));
	ChessMask->SetDecalMaterial(MI_ChessMask);
	ChessMask->SetupAttachment(ChessMesh);
	ChessMask->SetRelativeLocation(FVector(0, 0, 1));
	ChessMask->SetRelativeScale3D(FVector(0.0025f, 0.008f, 0.008f));

	FadeNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_Fade"));
	FadeNiagara->SetAsset(OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_FADE));
	FadeNiagara->SetVariableStaticMesh(FName("ChessMesh"), Mesh);
	FadeNiagara->SetupAttachment(ChessMesh);
	FadeNiagara->bAutoActivate = false;
	FadeNiagara->SetActive(false);

	TimeLine_ChessMove = CreateDefaultSubobject<UTimelineComponent>(TEXT("TimeLine_ChessMove"));
	Timeline_Fade = CreateDefaultSubobject<UTimelineComponent>(TEXT("Timeline_Fade"));

	MI_ChessMask = OM::GetConstructorObject<UMaterialInterface>(PATH_M_CHESSMASK);
	MI_Stroke = OM::GetConstructorObject<UMaterialInterface>(PATH_MI_STROKE_CHESS);
	CF_ChessMove = OM::GetConstructorObject<UCurveFloat>(PATH_CF_CHESSMOVE);
}

// 初始化棋子身份、棋盘引用和动画回调。
void AChesses::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	MyColor = color;
	MyRealColor = color;
	Pos = pos;
	Board2P = board2P;

	GameState = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
	if (GameState.IsValid())
	{
		//if (GameState->GetBattleType() == EBattleType::P2 || GameState->GetBattleType() == EBattleType::P2_AI)
		{
			if (color == EChessColor::REDCHESS)
			{
				ChessMask->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, -90, 90)));
			}
			else if (color == EChessColor::BLACKCHESS)
			{
				ChessMask->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, -90, -90)));
			}
		}
	}

	TWeakObjectPtr<AChesses> WeakThis(this);
	TimeLine_ChessMove->AddInterpFloat(CF_ChessMove, FOnTimelineFloatStatic::CreateLambda([WeakThis](float value) {
		if (WeakThis.IsValid() && WeakThis->Board2P.IsValid())
		{
			const Position pos = WeakThis->Pos;
			const Position targetPos = WeakThis->TargetPos;
            if (!WeakThis->Board2P->BoardLocs.IsValidIndex(pos.X) || !WeakThis->Board2P->BoardLocs[pos.X].IsValidIndex(pos.Y) ||
                !WeakThis->Board2P->BoardLocs.IsValidIndex(targetPos.X) || !WeakThis->Board2P->BoardLocs[targetPos.X].IsValidIndex(targetPos.Y)) return;

			FVector	Start = WeakThis->Board2P->BoardLocs[pos.X][pos.Y];				// 起始位置
			FVector End = WeakThis->Board2P->BoardLocs[targetPos.X][targetPos.Y];   // 目标位置
			FVector Vertex = (End - Start) / 2 + Start + FVector(0, 0, 5);			// 顶点位置

			FVector result = WeakThis->CalculateParabolicPosition(Start, Vertex, End, value);

			WeakThis->SetActorLocation(result); // 更新Actor位置
		}
		}));
	TimeLine_ChessMove->SetTimelineFinishedFunc(FOnTimelineEventStatic::CreateLambda([WeakThis]() {
		if (WeakThis.IsValid())
		{
			if (WeakThis->GameState.IsValid())
			{
				WeakThis->bAnimationing = false;
				WeakThis->Pos = WeakThis->TargetPos;
				WeakThis->GameState->OnFinishMove2P(WeakThis);
			}
		}
		}));



	// 使用时间轴控制消散效果
	Timeline_Fade->AddInterpFloat(CF_ChessMove, FOnTimelineFloatStatic::CreateLambda([WeakThis](float value) {
		if (WeakThis.IsValid())
		{
			WeakThis->ChessMesh->SetScalarParameterValueOnMaterials(FName(UTF8_TO_TCHAR("Fade")), value);
			WeakThis->FadeNiagara->SetVariableFloat(FName(UTF8_TO_TCHAR("Fade")), value);
		}
		}));

	Timeline_Fade->SetTimelineFinishedFunc(FOnTimelineEventStatic::CreateLambda([WeakThis]() {
		if (WeakThis.IsValid())
		{
			WeakThis->ChessMesh->SetHiddenInGame(true); // 棋子被吃后隐藏并销毁
            WeakThis->bProFadeComplete = true;
            WeakThis->FadeNiagara->Deactivate();
		}
		}));
}

// Called when the game starts or when spawned
void AChesses::BeginPlay()
{
	Super::BeginPlay();
	UpdateSelectable();
}

// 离开场景时停止本对象的运行逻辑。
void AChesses::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 停止所有时间轴
	TimeLine_ChessMove->Stop();
	Timeline_Fade->Stop();

	Super::EndPlay(EndPlayReason);
}

// 响应重开事件并清理上一局状态。
void AChesses::GamePlayAgain(UObject* OwnerObject)
{
	Destroy();
	IIF_GameState::GamePlayAgain(OwnerObject);
}

// Called every frame
void AChesses::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    if (!Board2P.IsValid()) return;
    if (bCaptureApproach)
    {
        ProMoveTime += DeltaTime;
        const float Alpha = FMath::Clamp(ProMoveTime / ProMoveDuration, 0.f, 1.f);
        SetActorLocation(CaptureApproachLocation(PendingCapture.Get(), Alpha));
        if (Alpha >= 1.f)
        {
            bCaptureApproach = false;
            bCaptureLanding = true;
            Pos = TargetPos;
            ProSupport = PendingCapture;
            CaptureHoverRemaining = 0.16f;
            CaptureHoverHeight = GetActorLocation().Z;
            CaptureHoverOffset = ProSupport.IsValid() ? CaptureHoverHeight - ProSupport->GetActorLocation().Z : 0.f;
            if (PendingCapture.IsValid()) PendingCapture->Defeated();
            PendingCapture.Reset();
        }
    }
    else if (bProTravelling)
    {
        ProMoveTime += DeltaTime;
        const float Alpha = FMath::Clamp(ProMoveTime / ProMoveDuration, 0.f, 1.f);
        const auto Occupant = Board2P->GetChess(TargetPos);
        AChesses* Victim = Occupant.IsValid() && Occupant->GetColor() != MyColor && !Occupant->IsDead() ? Occupant.Get() : nullptr;
        FVector Location = CaptureApproachLocation(Victim, Alpha);
        SetActorLocation(Location);
    }
    else if (CaptureHoverRemaining > 0.f)
    {
        FVector Location = GetActorLocation();
        if (ProSupport.IsValid()) CaptureHoverHeight = ProSupport->GetActorLocation().Z + CaptureHoverOffset;
        Location.Z = CaptureHoverHeight;
        SetActorLocation(Location);
        CaptureHoverRemaining = FMath::Max(0.f, CaptureHoverRemaining - DeltaTime);
        if (CaptureHoverRemaining <= 0.f) ProSupport.Reset();
    }
    else if ((bProMode || bCaptureLanding) && Board2P->IsValidPosition(Pos.X, Pos.Y))
    {
        FVector Location = GetActorLocation();
        Location.Z = FMath::FInterpConstantTo(Location.Z, Board2P->BoardLocs[Pos.X][Pos.Y].Z, DeltaTime, 12.f);
        SetActorLocation(Location);
        if (bCaptureLanding && FMath::IsNearlyEqual(Location.Z, Board2P->BoardLocs[Pos.X][Pos.Y].Z, 0.01f))
        {
            bCaptureLanding = bAnimationing = false;
            if (GameState.IsValid()) GameState->OnFinishMove2P(this);
            if (!bProMode && !bDead) SetActorTickEnabled(false);
        }
    }
    if (bDead)
    {
        ProDeathTime += DeltaTime;
        if (bProFadeComplete && FadeNiagara->IsComplete()) Destroy();
        else if (ProDeathTime > 8.f) { FadeNiagara->DeactivateImmediate(); Destroy(); }
    }
}

void AChesses::ClearMoveSelection()
{
    bSelected = false;
    ChessMesh->SetOverlayMaterial(nullptr);
}

void AChesses::EnableProMode()
{
    bProMode = true;
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
    UpdateSelectable();
    if (!bSelectable) ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

FVector AChesses::CaptureApproachLocation(AChesses* Victim, float Alpha) const
{
    const FVector End = Board2P->BoardLocs[TargetPos.X][TargetPos.Y];
    if (IsValid(Victim))
    {
        const float BottomOffset = GetActorLocation().Z - (ChessMesh->Bounds.Origin.Z - ChessMesh->Bounds.BoxExtent.Z);
        const float HoverZ = Victim->GetSurfaceZ() + BottomOffset + 0.25f;
        FVector Location = FMath::Lerp(ProMoveStart, End, Alpha);
        Location.Z = FMath::Lerp(ProMoveStart.Z, HoverZ, FMath::SmoothStep(0.f, 0.35f, Alpha));
        return Location;
    }
    FVector Location = FMath::Lerp(ProMoveStart, End, Alpha);
    Location.Z += FMath::Sin(Alpha * PI) * 1.8f;
    return Location;
}

float AChesses::GetSurfaceZ() const
{
    return ChessMesh->Bounds.Origin.Z + ChessMesh->Bounds.BoxExtent.Z;
}

void AChesses::BeginProMove(const FChessMove2P& Move, float Duration)
{
    TargetPos = Move.to;
    bProTravelling = bAnimationing = true;
    ProMoveTime = 0.f;
    ProMoveDuration = FMath::Max(0.01f, Duration);
    ProMoveStart = GetActorLocation();
    ProSupport.Reset();
    CaptureHoverRemaining = 0.f;
    bSelected = false;
    ChessMesh->SetOverlayMaterial(nullptr);
    TimeLine_ChessMove->Stop();
    ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

void AChesses::ArriveProMove(FIntPoint Destination, AChesses* Captured)
{
    bProTravelling = bAnimationing = false;
    Pos = TargetPos = Destination;
    ProSupport = Captured;
    FVector Location = Board2P->BoardLocs[Pos.X][Pos.Y];
    if (IsValid(Captured))
    {
        const float BottomOffset = GetActorLocation().Z - (ChessMesh->Bounds.Origin.Z - ChessMesh->Bounds.BoxExtent.Z);
        Location.Z = Captured->GetSurfaceZ() + BottomOffset + 0.25f;
        CaptureHoverRemaining = 0.16f;
        CaptureHoverHeight = Location.Z;
        CaptureHoverOffset = Location.Z - Captured->GetActorLocation().Z;
    }
    SetActorLocation(Location);
    ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, bSelectable ? ECR_Block : ECR_Ignore);
}

void AChesses::StopProMove()
{
    bProTravelling = bAnimationing = false;
    bSelectable = bSelected = false;
    ChessMesh->SetOverlayMaterial(nullptr);
    ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

// Called to bind functionality to input
void AChesses::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// 将鼠标点击转换为棋子选择操作。
void AChesses::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	if (ButtonPressed == EKeys::LeftMouseButton)
	{
		HandleClick();
	}
}

// 鼠标进入时显示棋子描边。
void AChesses::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();
	ChessMesh->SetOverlayMaterial(MI_Stroke); // 添加描边材质
}

// 鼠标离开时清理未选中棋子的描边。
void AChesses::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	if (!bSelected) // 未被选中
	{
		ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	}
}

// 触摸开始时显示棋子反馈。
void AChesses::NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchBegin(FingerIndex);
	if (FingerIndex == ETouchIndex::Touch1)
		if (!bSelected)
			ChessMesh->SetOverlayMaterial(MI_Stroke); // 添加描边材质
}

// 结束对局后禁止棋子继续被选择。
void AChesses::GameOver(UObject* OwnerObject)
{
	bSelectable = false; // 禁止棋子被选中
	if (bSelected)
	{
		bSelected = false; // 移除选中状态
		if (GameState.IsValid()) GameState->DismissSettingPoint2P(); // 清除落子点
		ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	}
	IIF_GameState::GameOver(OwnerObject);
}

// 触摸结束时处理棋子选择。
void AChesses::NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchEnd(FingerIndex);

	// 只处理第一根手指的点击事件
	if (FingerIndex == ETouchIndex::Touch1)
	{
		HandleClick();
	}
}

// 检查回合和对象有效性后处理点击。
void AChesses::HandleClick()
{
	if (GameState.IsValid())
	{
		if (GameState->IsMyTurn()) // 判断是否到了玩家回合
		{
			if (!bAnimationing)
			{
				if (bSelectable)
				{
					if (bSelected)
					{
						bSelected = false; // 移除选中状态
						if (GameState.IsValid()) GameState->DismissSettingPoint2P(); // 清除落子点
						ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
					}
					else
					{
						if (Board2P.IsValid())
						{
							bSelected = true;
							ChessMesh->SetOverlayMaterial(MI_Stroke); // 添加描边材质
							GenerateMove2P(Board2P, this);
						}
						else
						{
							ULogger::LogError(TEXT("AChesses::HandleClick: ChessBoard2P instance is nullptr!"));
						}
					}
				}
			}
		}
		else
		{
			ULogger::Log(TEXT("AChesses::HandleClick: Not your turn"));
		}
	}
	else
	{
		ULogger::LogError(TEXT("AChesses::HandleClick: GameState instance is nullptr!"));
	}
}

// 播放棋子被吃效果，并处理派生棋子的结束逻辑。
void AChesses::Defeated()
{
    SetActorTickEnabled(true);
	if (bDead)
		return;

    bCaptureApproach = bCaptureLanding = bProTravelling = bAnimationing = false;
    TimeLine_ChessMove->Stop();

	bDead = true;													// 标记阵亡
	FadeNiagara->SetActive(true);									// 激活粒子效果
	ChessMask->SetHiddenInGame(true);								// 隐藏棋面贴花
	ChessMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 关闭碰撞
	ChessMesh->SetOverlayMaterial(nullptr);							// 移除描边材质
	Timeline_Fade->PlayFromStart();									// 播放击败效果
}

// 获取棋子显示名称。
FString AChesses::GetChessName() const
{
	return MyName;
}

// 获取棋子当前阵营。
EChessColor AChesses::GetColor() const
{
	return MyColor;
}

// 修改棋子阵营并更新局面修订号。
void AChesses::SetColor(EChessColor InColor)
{
	if (Board2P.IsValid()) Board2P->MarkPositionChanged();
	MyColor = InColor;
}

// 获取棋子当前类型。
EChessType AChesses::GetType() const
{
	return MyType;
}

// 修改棋子类型并更新局面修订号。
void AChesses::SetType(EChessType InType)
{
	if (Board2P.IsValid()) Board2P->MarkPositionChanged();
	MyType = InType;
}

// 获取棋子当前的棋盘坐标。
Position AChesses::GetPosition() const
{
	return Pos;
}

// 获取棋子是否已经被吃。
bool AChesses::IsDead() const
{
	return bDead;
}

// 根据玩法与阵营更新棋子的可选状态。
void AChesses::UpdateSelectable()
{
	if (!GameState.IsValid()) { bSelectable = false; return; }
    if (GameState->IsLANGame())
    {
        // 选择权限与本机执棋颜色一致，避免在自己的回合选到对方棋子。
        const auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
        bSelectable = Room->GetLocalColor() == MyColor;
        return;
    }
	if (MyColor == EChessColor::BLACKCHESS && (GameState->GetBattleType() == EBattleType::P2_AI || GameState->GetBattleType() == EBattleType::SoloRide))
	{
		bSelectable = false; // AI棋子不可被玩家选中
	}
	else
	{
		bSelectable = true; // 玩家可以选中
	}
}

// 生成选中棋子的合法走法并显示提示。
void AChesses::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	if (!GameState.IsValid() || !board2P.IsValid() || !target.IsValid()) return;
	for (int32 i = 0; i < 10; i++)
	{
		for (int32 j = 0; j < 9; j++)
		{
			TWeakObjectPtr<AChesses> captureChess = board2P->GetChess(i, j);
			if (captureChess.IsValid() && captureChess != target.Get())
			{
				captureChess->ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
				captureChess->bSelected = false; // 清除选择状态
			}
		}
	}

	// 获取全部合法走法
    TArray<FChessMove2P> Moves;
    if (GameState->IsProBattle())
    {
        GameState->ProBattle->SelectedType = MyType;
        Moves = GameState->ProBattle->GenerateMoves(this);
    }
    else Moves = board2P->GenerateMovesForChess(Pos.X, Pos.Y, this);

	GameState->ShowSettingPoint2P(Moves, this);
}

// 更新移动目标并播放落子动画。
void AChesses::ApplyMove(FChessMove2P Move)
{
	TargetPos = Position(Move.to.X, Move.to.Y); // 获取目标位置


	bSelected = false; // 移除选中状态
	ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	if (GameState.IsValid()) GameState->DismissSettingPoint2P(); // 隐藏所有落子点
    if (PendingCapture.IsValid())
    {
        // 非烽火弈棋子统一速度；吃子与普通移动共用同一行程时长。
        ProMoveStart = GetActorLocation(); ProMoveTime = 0.f;
        ProMoveDuration = FMath::Max(0.65f, FVector2D(TargetPos - Pos).Size() / 4.f);
        bCaptureApproach = bAnimationing = true;
        SetActorTickEnabled(true);
    }
    else PlayMoveAnim();
}

// 播放落子动画，缺失曲线时直接完成落子。
void AChesses::PlayMoveAnim()
{
    if (!IsValid(CF_ChessMove) || !IsValid(TimeLine_ChessMove))
    {
        Pos = TargetPos;
        bAnimationing = false;
        if (Board2P.IsValid() && Board2P->BoardLocs.IsValidIndex(Pos.X) && Board2P->BoardLocs[Pos.X].IsValidIndex(Pos.Y))
            SetActorLocation(Board2P->BoardLocs[Pos.X][Pos.Y]);
        if (GameState.IsValid()) GameState->OnFinishMove2P(this);
        return;
    }
	bAnimationing = true;
	const float Duration = FMath::Max(0.65f, FVector2D(TargetPos - Pos).Size() / 4.f);
	TimeLine_ChessMove->SetPlayRate(TimeLine_ChessMove->GetTimelineLength() / Duration);
	TimeLine_ChessMove->PlayFromStart(); // 播放移动动画
}

// 使用二次贝塞尔曲线计算移动位置。
FVector AChesses::CalculateParabolicPosition(const FVector& Start, const FVector& Vertex, const FVector& End, float T)
{
	// 限制插值范围
	if (T < 0)
		T = 0;
	if (T > 1)
		T = 1;

	// 根据中点高度计算贝塞尔控制点
	FVector P0 = Start;
	FVector P2 = End;
	FVector P1 = 2 * Vertex - 0.5f * P0 - 0.5f * P2;

	// 二次贝塞尔曲线公式
	float OneMinusT = 1 - T;
	FVector Result = (OneMinusT * OneMinusT) * P0 + 2 * OneMinusT * T * P1 + (T * T) * P2;
	return Result;
}

// 揭示真实身份并刷新局面与可选状态。
void AChesses::SwitchToRealIdentity()
{
	if (Board2P.IsValid()) Board2P->MarkPositionChanged();
	MyColor = MyRealColor;
	MyType = MyRealType;
	ChessMask->SetVisibility(true);
	UpdateSelectable();
}

