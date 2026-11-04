// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chesses.h"
#include "ChessMoveEffectsComponent.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/GameMode/FengHuoBattleComponent.h"

// Sets default values
AChesses::AChesses()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

	UStaticMesh* Mesh = OM::GetConstructorObject<UStaticMesh>(PATH_SM_CHESS);

	ChessMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChessMeshComponent"));
	ChessMesh->SetStaticMesh(Mesh);
	RootComponent = ChessMesh;

    MoveEffects = CreateDefaultSubobject<UChessMoveEffectsComponent>(TEXT("MoveEffects"));

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
            if (WeakThis->MoveEffects->bKnightEffects)
            {
                // 马用直接时间进度而不是通用抬棋曲线，0.5 秒完成一次明确的二次贝塞尔腾跃。
                const float Alpha = FMath::Clamp(WeakThis->TimeLine_ChessMove->GetPlaybackPosition()
                    / FMath::Max(WeakThis->TimeLine_ChessMove->GetTimelineLength(), SMALL_NUMBER), 0.f, 1.f);
                result = FMath::Lerp(Start, End, Alpha);
                result.Z += 2.f * Alpha * (1.f - Alpha) * WeakThis->MoveEffects->GetKnightJumpHeight(Start, End);
            }

			WeakThis->SetActorLocation(result); // 更新Actor位置
            WeakThis->MoveEffects->UpdateMove(WeakThis->GetWorld()->GetDeltaSeconds());
		}
		}));
	TimeLine_ChessMove->SetTimelineFinishedFunc(FOnTimelineEventStatic::CreateLambda([WeakThis]() {
		if (WeakThis.IsValid())
		{
			WeakThis->bAnimationing = false;
			WeakThis->Pos = WeakThis->TargetPos;
			WeakThis->MoveEffects->FinishMove();
			if (WeakThis->GameState.IsValid())
			{
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
            WeakThis->bCaptureFadeComplete = true;
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
    MoveEffects->CancelMove();
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
    if (bMovementPaused || (GameState.IsValid() && GameState->IsGameplayFrozen()) || !Board2P.IsValid()) return;
    if (bCaptureApproach)
    {
        FengHuoMoveTime += DeltaTime;
        const float Alpha = FMath::Clamp(FengHuoMoveTime / FengHuoMoveDuration, 0.f, 1.f);
        SetActorLocation(CaptureApproachLocation(PendingCapture.Get(), Alpha));
        if (Alpha >= 1.f)
        {
            bCaptureApproach = false;
            bCaptureLanding = true;
            Pos = TargetPos;
            CaptureSupport = PendingCapture;
            CaptureHoverRemaining = 0.16f;
            CaptureHoverHeight = GetActorLocation().Z;
            CaptureHoverOffset = CaptureSupport.IsValid() ? CaptureHoverHeight - CaptureSupport->GetActorLocation().Z : 0.f;
            // 首次到达被吃棋子上方时爆发；之后的悬停和下落不再次触发。
            MoveEffects->FinishMove(PendingCapture.Get());
            if (PendingCapture.IsValid()) PendingCapture->Defeated();
            PendingCapture.Reset();
        }
    }
    else if (bRealtimeTravelling)
    {
        FengHuoMoveTime += DeltaTime;
        const float Alpha = FMath::Clamp(FengHuoMoveTime / FengHuoMoveDuration, 0.f, 1.f);
        const auto Occupant = Board2P->GetChess(TargetPos);
        AChesses* Victim = Occupant.IsValid() && Occupant->GetColor() != MyColor && !Occupant->IsDead() ? Occupant.Get() : nullptr;
        FVector Location = CaptureApproachLocation(Victim, Alpha);
        SetActorLocation(Location);
    }
    else if (CaptureHoverRemaining > 0.f)
    {
        FVector Location = GetActorLocation();
        if (CaptureSupport.IsValid()) CaptureHoverHeight = CaptureSupport->GetActorLocation().Z + CaptureHoverOffset;
        Location.Z = CaptureHoverHeight;
        SetActorLocation(Location);
        CaptureHoverRemaining = FMath::Max(0.f, CaptureHoverRemaining - DeltaTime);
        if (CaptureHoverRemaining <= 0.f) CaptureSupport.Reset();
    }
    else if ((bFengHuoMode || bCaptureLanding) && Board2P->IsValidPosition(Pos.X, Pos.Y))
    {
        FVector Location = GetActorLocation();
        Location.Z = FMath::FInterpConstantTo(Location.Z, Board2P->BoardLocs[Pos.X][Pos.Y].Z, DeltaTime, 12.f);
        SetActorLocation(Location);
        if (FMath::IsNearlyEqual(Location.Z, Board2P->BoardLocs[Pos.X][Pos.Y].Z, 0.01f))
        {
            // 普通吃子与烽火吃子都先悬停再下降。将帅的落印只能在真正接触棋盘后触发；
            // 组件内的单次锁存使后续静止帧和重复联机快照保持无待机特效。
            MoveEffects->FinishLanding();
        }
        if (bCaptureLanding && FMath::IsNearlyEqual(Location.Z, Board2P->BoardLocs[Pos.X][Pos.Y].Z, 0.01f))
        {
            bCaptureLanding = bAnimationing = false;
            if (GameState.IsValid()) GameState->OnFinishMove2P(this);
            if (!bFengHuoMode && !bDead) SetActorTickEnabled(false);
        }
    }
    // 普通吃子接近和烽火移动均在本 Tick 更新位移；及时把同帧数据送入 Niagara。
    MoveEffects->UpdateMove(DeltaTime);
    if (bDead)
    {
        CaptureDeathTime += DeltaTime;
        if (bCaptureFadeComplete && FadeNiagara->IsComplete()) Destroy();
        else if (CaptureDeathTime > 8.f) { FadeNiagara->DeactivateImmediate(); Destroy(); }
    }
}

bool AChesses::BeginTouchMoveSelection()
{
    // 与原点击选择共用权限和走法生成：观战、对手回合、动画中及已阵亡棋子均不可拖动。
    if (!GameState.IsValid() || !GameState->IsMyTurn() || !Board2P.IsValid() || bDead || bAnimationing || !bSelectable)
        return false;
    if (!bSelected) HandleClick();
    else GenerateMove2P(Board2P, this);
    return bSelected;
}

void AChesses::ClearMoveSelection()
{
    bSelected = false;
    ChessMesh->SetOverlayMaterial(nullptr);
}

void AChesses::EnableFengHuoMode()
{
    bFengHuoMode = true;
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
        FVector Location = FMath::Lerp(FengHuoMoveStart, End, Alpha);
        if (MoveEffects->bKnightEffects)
        {
            // 吃子先腾空，在敌棋顶面以上收束，仍沿用原有结算、悬停与落下的生命周期。
            Location.Z = FMath::Lerp(FengHuoMoveStart.Z, HoverZ, Alpha)
                + 2.f * Alpha * (1.f - Alpha) * MoveEffects->GetKnightJumpHeight(FengHuoMoveStart, End);
            return Location;
        }
        Location.Z = FMath::Lerp(FengHuoMoveStart.Z, HoverZ, FMath::SmoothStep(0.f, 0.35f, Alpha));
        return Location;
    }
    FVector Location = FMath::Lerp(FengHuoMoveStart, End, Alpha);
    if (MoveEffects->bKnightEffects)
    {
        Location.Z += 2.f * Alpha * (1.f - Alpha) * MoveEffects->GetKnightJumpHeight(FengHuoMoveStart, End);
        return Location;
    }
    Location.Z += FMath::Sin(Alpha * PI) * 1.8f;
    return Location;
}

float AChesses::GetSurfaceZ() const
{
    return ChessMesh->Bounds.Origin.Z + ChessMesh->Bounds.BoxExtent.Z;
}

FVector AChesses::GetEffectImpactLocation() const
{
    // 动画中的被吃棋子可能悬在上一枚棋子上方，不能拿它当前的 Z 作为贴地冲击波的高度。
    if (Board2P.IsValid() && Board2P->BoardLocs.IsValidIndex(TargetPos.X) &&
        Board2P->BoardLocs[TargetPos.X].IsValidIndex(TargetPos.Y))
        return Board2P->BoardLocs[TargetPos.X][TargetPos.Y];
    return GetActorLocation();
}

void AChesses::BeginFengHuoMove(const FChessMove2P& Move, float Duration)
{
    TargetPos = Move.to;
    // 一次性世界弧线必须在目标格已更新后读取终点，连续走子不能沿用上一段 TargetPos。
    MoveEffects->BeginMove();
    bRealtimeTravelling = bAnimationing = true;
    FengHuoMoveTime = 0.f;
    FengHuoMoveDuration = FMath::Max(0.01f, Duration);
    FengHuoMoveStart = GetActorLocation();
    CaptureSupport.Reset();
    CaptureHoverRemaining = 0.f;
    bSelected = false;
    ChessMesh->SetOverlayMaterial(nullptr);
    TimeLine_ChessMove->Stop();
    ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

void AChesses::ArriveFengHuoMove(FIntPoint Destination, AChesses* Captured)
{
    bRealtimeTravelling = bAnimationing = false;
    Pos = TargetPos = Destination;
    CaptureSupport = Captured;
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
    // 主机和联机快照共用到达入口；组件内部的移动锁存防止重复快照重复播放。
    MoveEffects->FinishMove(Captured);
    ChessMesh->SetCollisionResponseToChannel(ECC_Visibility, bSelectable ? ECR_Block : ECR_Ignore);
}

void AChesses::StopFengHuoMove()
{
    MoveEffects->CancelMove();
    bRealtimeTravelling = bAnimationing = false;
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

    MoveEffects->CancelMove();

    bCaptureApproach = bCaptureLanding = bRealtimeTravelling = bAnimationing = false;
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
    if (GameState->IsFengHuoBattle())
    {
        GameState->FengHuoBattle->SelectedType = MyType;
        Moves = GameState->FengHuoBattle->GenerateMoves(this);
    }
    else Moves = board2P->GenerateMovesForChess(Pos.X, Pos.Y, this);

	GameState->ShowSettingPoint2P(Moves, this);
}

// 更新移动目标并播放落子动画。
void AChesses::ApplyMove(FChessMove2P Move)
{
	TargetPos = Position(Move.to.X, Move.to.Y); // 获取目标位置
    MoveEffects->BeginMove();


	bSelected = false; // 移除选中状态
	ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	if (GameState.IsValid()) GameState->DismissSettingPoint2P(); // 隐藏所有落子点
    if (PendingCapture.IsValid())
    {
        // 普通模式吃子固定用时到达目标上方，距离越远位移速度越快。
        FengHuoMoveStart = GetActorLocation(); FengHuoMoveTime = 0.f;
        FengHuoMoveDuration = MoveEffects->bKnightEffects ? 0.5f : 0.65f;
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
        MoveEffects->FinishMove();
        if (GameState.IsValid()) GameState->OnFinishMove2P(this);
        return;
    }
	bAnimationing = true;
	const float Duration = MoveEffects->bKnightEffects ? 0.5f : FMath::Max(0.65f, FVector2D(TargetPos - Pos).Size() / 4.f);
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


void AChesses::SetMovementPaused(bool bPaused)
{
    if (bMovementPaused == bPaused) return;
    bMovementPaused = bPaused;
    if (bPaused)
    {
        bResumeActorTick = IsActorTickEnabled();
        bResumeMoveTimeline = TimeLine_ChessMove->IsPlaying();
        bResumeFadeTimeline = Timeline_Fade->IsPlaying();
        bReverseMoveTimeline = TimeLine_ChessMove->IsReversing();
        bReverseFadeTimeline = Timeline_Fade->IsReversing();
        TimeLine_ChessMove->Stop();
        Timeline_Fade->Stop();
        SetActorTickEnabled(false);
        // 世界暂停之外再锁住棋子自身的粒子，兼容资源勾选 TickWhenPaused 的情况。
        TInlineComponentArray<UNiagaraComponent*> Effects(this);
        for (auto* Effect : Effects)
            if (Effect->IsActive() && !Effect->IsPaused())
            {
                PausedMoveEffects.Add(Effect);
                Effect->SetPaused(true);
            }
    }
    else
    {
        SetActorTickEnabled(bResumeActorTick);
        if (bResumeMoveTimeline)
        {
            if (bReverseMoveTimeline) TimeLine_ChessMove->Reverse();
            else TimeLine_ChessMove->Play();
        }
        if (bResumeFadeTimeline)
        {
            if (bReverseFadeTimeline) Timeline_Fade->Reverse();
            else Timeline_Fade->Play();
        }
        for (const auto& Effect : PausedMoveEffects) if (Effect.IsValid()) Effect->SetPaused(false);
        PausedMoveEffects.Reset();
    }
}
