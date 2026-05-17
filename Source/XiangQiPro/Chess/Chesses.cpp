// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chesses.h"

// Sets default values
AChesses::AChesses()
{
	PrimaryActorTick.bCanEverTick = true;

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

void AChesses::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	MyColor = color;
	Pos = pos;
	Board2P = board2P;

	GameState = Cast<GS>(GetWorld()->GetGameState());
	if (GameState)
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
			FVector2D pos = WeakThis->Pos;
			FVector2D targetPos = WeakThis->TargetPos;

			FVector	Start = WeakThis->Board2P->BoardLocs[pos.X][pos.Y];				// 起始位置
			FVector End = WeakThis->Board2P->BoardLocs[targetPos.X][targetPos.Y];   // 终止位置
			FVector Vertex = (End - Start) / 2 + Start + FVector(0, 0, 5);			// 顶点位置

			FVector result = WeakThis->CalculateParabolicPosition(Start, Vertex, End, value);

			WeakThis->SetActorLocation(result); // 更新Actor位置
		}
		}));
	TimeLine_ChessMove->SetTimelineFinishedFunc(FOnTimelineEventStatic::CreateLambda([WeakThis]() {
		if (WeakThis.IsValid())
		{
			if (WeakThis->GameState)
			{
				WeakThis->bAnimationing = false;
				WeakThis->Pos = WeakThis->TargetPos;
				WeakThis->GameState->OnFinishMove2P();
			}
		}
		}));



	// 用时间轴控制边缘消散效果
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
			WeakThis->ChessMesh->SetHiddenInGame(true); // 吃掉，将其隐藏
			WeakThis->Destroy();
		}
		}));
}

// Called when the game starts or when spawned
void AChesses::BeginPlay()
{
	Super::BeginPlay();
	if (MyColor == EChessColor::BLACKCHESS && (GameState->GetBattleType() == EBattleType::P2_AI || GameState->GetBattleType() == EBattleType::SoloRide))
	{
		bSelectable = false; // 棋子属于AI，不可被选中
	}
}

void AChesses::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 停止所有时间线
	TimeLine_ChessMove->Stop();
	Timeline_Fade->Stop();

	Super::EndPlay(EndPlayReason);
}

void AChesses::GamePlayAgain(UObject* OwnerObject)
{
	Destroy();
	IIF_GameState::GamePlayAgain(OwnerObject);
}

// Called every frame
void AChesses::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AChesses::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AChesses::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	if (ButtonPressed == EKeys::LeftMouseButton)
	{
		HandleClick();
	}
}

void AChesses::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();
	ChessMesh->SetOverlayMaterial(MI_Stroke); // 添加描边材质
}

void AChesses::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	if (!bSelected) // 未被选中
	{
		ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	}
}

void AChesses::NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchBegin(FingerIndex);
	if (FingerIndex == ETouchIndex::Touch1)
		if (!bSelected)
			ChessMesh->SetOverlayMaterial(MI_Stroke); // 添加描边材质
}

void AChesses::GameOver(UObject* OwnerObject)
{
	bSelectable = false; // 禁止棋子被选中
	if (bSelected)
	{
		bSelected = false; // 移除被选中状态
		GameState->DismissSettingPoint2P(); // 清除落子点
		ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	}
	IIF_GameState::GameOver(OwnerObject);
}

void AChesses::NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchEnd(FingerIndex);

	// 对于触摸事件，我们直接处理，不区分手指索引（或者只处理第一根手指）
	if (FingerIndex == ETouchIndex::Touch1)
	{
		HandleClick();
	}
}

void AChesses::HandleClick()
{
	if (GameState)
	{
		if (GameState->IsMyTurn()) // 判断是否到了我的回合
		{
			if (!bAnimationing)
			{
				if (bSelectable)
				{
					if (bSelected)
					{
						bSelected = false; // 移除被选中状态
						GameState->DismissSettingPoint2P(); // 清除落子点
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

void AChesses::Defeated()
{
	FadeNiagara->SetActive(true); // 激活粒子效果
	ChessMask->SetHiddenInGame(true); // 提前隐藏掉
	ChessMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 关闭碰撞体积
	ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	Timeline_Fade->PlayFromStart(); // 执行击败效果的曲线
}

FString AChesses::GetChessName() const
{
	return MyName;
}

EChessColor AChesses::GetColor() const
{
	return MyColor;
}

EChessType AChesses::GetType() const
{
	return MyType;
}

Position AChesses::GetPosition() const
{
	return Pos;
}

void AChesses::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	for (int32 i = 0; i < 10; i++)
	{
		for (int32 j = 0; j < 9; j++)
		{
			TWeakObjectPtr<AChesses> captureChess = Board2P->AllChess[i][j];
			if (captureChess != nullptr && captureChess != target.Get())
			{
				captureChess->ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
				captureChess->bSelected = false; // 清除被选择状态
			}
		}
	}
}

void AChesses::ApplyMove(FChessMove2P Move)
{
	TargetPos = Position(Move.to.X, Move.to.Y); // 获取移动的目标位置

	PlayMoveAnim();

	bSelected = false; // 移除被选中状态
	ChessMesh->SetOverlayMaterial(nullptr); // 移除描边材质
	GameState->DismissSettingPoint2P(); // 隐藏所有落子点
}

void AChesses::PlayMoveAnim()
{
	bAnimationing = true;
	TimeLine_ChessMove->PlayFromStart(); // 开始播放移动动画
}

FVector AChesses::CalculateParabolicPosition(const FVector& Start, const FVector& Vertex, const FVector& End, float T)
{
	// 处理百分比异常
	if (T < 0)
		T = 0;
	if (T > 1)
		T = 1;

	// 计算控制点P1，假设抛物线对称，顶点在t=0.5
	FVector P0 = Start;
	FVector P2 = End;
	FVector P1 = 2 * Vertex - 0.5f * P0 - 0.5f * P2;

	// 二次贝塞尔曲线公式
	float OneMinusT = 1 - T;
	FVector Result = (OneMinusT * OneMinusT) * P0 + 2 * OneMinusT * T * P1 + (T * T) * P2;
	return Result;
}

