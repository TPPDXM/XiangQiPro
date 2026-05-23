// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chesses.h"

// Sets default values
AChesses::AChesses()
{
	PrimaryActorTick.bCanEverTick = false;

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
	MyRealColor = color;
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

			FVector	Start = WeakThis->Board2P->BoardLocs[pos.X][pos.Y];				// 璧峰浣嶇疆
			FVector End = WeakThis->Board2P->BoardLocs[targetPos.X][targetPos.Y];   // 缁堟浣嶇疆
			FVector Vertex = (End - Start) / 2 + Start + FVector(0, 0, 5);			// 椤剁偣浣嶇疆

			FVector result = WeakThis->CalculateParabolicPosition(Start, Vertex, End, value);

			WeakThis->SetActorLocation(result); // 鏇存柊Actor浣嶇疆
		}
		}));
	TimeLine_ChessMove->SetTimelineFinishedFunc(FOnTimelineEventStatic::CreateLambda([WeakThis]() {
		if (WeakThis.IsValid())
		{
			if (WeakThis->GameState)
			{
				WeakThis->bAnimationing = false;
				WeakThis->Pos = WeakThis->TargetPos;
				WeakThis->GameState->OnFinishMove2P(WeakThis);
			}
		}
		}));



	// 鐢ㄦ椂闂磋酱鎺у埗杈圭紭娑堟暎鏁堟灉
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
			WeakThis->ChessMesh->SetHiddenInGame(true); // 鍚冩帀锛屽皢鍏堕殣钘?
			WeakThis->Destroy();
		}
		}));
}

// Called when the game starts or when spawned
void AChesses::BeginPlay()
{
	Super::BeginPlay();
	UpdateSelectable();
}

void AChesses::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 鍋滄鎵€鏈夋椂闂寸嚎
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
	ChessMesh->SetOverlayMaterial(MI_Stroke); // 娣诲姞鎻忚竟鏉愯川
}

void AChesses::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	if (!bSelected) // 鏈閫変腑
	{
		ChessMesh->SetOverlayMaterial(nullptr); // 绉婚櫎鎻忚竟鏉愯川
	}
}

void AChesses::NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchBegin(FingerIndex);
	if (FingerIndex == ETouchIndex::Touch1)
		if (!bSelected)
			ChessMesh->SetOverlayMaterial(MI_Stroke); // 娣诲姞鎻忚竟鏉愯川
}

void AChesses::GameOver(UObject* OwnerObject)
{
	bSelectable = false; // 绂佹妫嬪瓙琚€変腑
	if (bSelected)
	{
		bSelected = false; // 绉婚櫎琚€変腑鐘舵€?
		GameState->DismissSettingPoint2P(); // 娓呴櫎钀藉瓙鐐?
		ChessMesh->SetOverlayMaterial(nullptr); // 绉婚櫎鎻忚竟鏉愯川
	}
	IIF_GameState::GameOver(OwnerObject);
}

void AChesses::NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchEnd(FingerIndex);

	// 瀵逛簬瑙︽懜浜嬩欢锛屾垜浠洿鎺ュ鐞嗭紝涓嶅尯鍒嗘墜鎸囩储寮曪紙鎴栬€呭彧澶勭悊绗竴鏍规墜鎸囷級
	if (FingerIndex == ETouchIndex::Touch1)
	{
		HandleClick();
	}
}

void AChesses::HandleClick()
{
	if (GameState)
	{
		if (GameState->IsMyTurn()) // 鍒ゆ柇鏄惁鍒颁簡鎴戠殑鍥炲悎
		{
			if (!bAnimationing)
			{
				if (bSelectable)
				{
					if (bSelected)
					{
						bSelected = false; // 绉婚櫎琚€変腑鐘舵€?
						GameState->DismissSettingPoint2P(); // 娓呴櫎钀藉瓙鐐?
						ChessMesh->SetOverlayMaterial(nullptr); // 绉婚櫎鎻忚竟鏉愯川
					}
					else
					{
						if (Board2P.IsValid())
						{
							bSelected = true;
							ChessMesh->SetOverlayMaterial(MI_Stroke); // 娣诲姞鎻忚竟鏉愯川
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
	if (bDead)
		return;

	bDead = true;													// 鏍囪闃典骸
	FadeNiagara->SetActive(true);									// 婵€娲荤矑瀛愭晥鏋?
	ChessMask->SetHiddenInGame(true);								// 鎻愬墠闅愯棌鎺?
	ChessMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 鍏抽棴纰版挒浣撶Н
	ChessMesh->SetOverlayMaterial(nullptr);							// 绉婚櫎鎻忚竟鏉愯川
	Timeline_Fade->PlayFromStart();									// 鎵ц鍑昏触鏁堟灉鐨勬洸绾?
}

FString AChesses::GetChessName() const
{
	return MyName;
}

EChessColor AChesses::GetColor() const
{
	return MyColor;
}

void AChesses::SetColor(EChessColor InColor)
{
	MyColor = InColor;
}

EChessType AChesses::GetType() const
{
	return MyType;
}

void AChesses::SetType(EChessType InType)
{
	MyType = InType;
}

Position AChesses::GetPosition() const
{
	return Pos;
}

bool AChesses::IsDead() const
{
	return bDead;
}

void AChesses::UpdateSelectable()
{
	if (MyColor == EChessColor::BLACKCHESS && (GameState->GetBattleType() == EBattleType::P2_AI || GameState->GetBattleType() == EBattleType::SoloRide))
	{
		bSelectable = false; // 妫嬪瓙灞炰簬AI锛屼笉鍙閫変腑
	}
	else
	{
		bSelectable = true; // 鐜╁鍙互閫変腑
	}
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
				captureChess->ChessMesh->SetOverlayMaterial(nullptr); // 绉婚櫎鎻忚竟鏉愯川
				captureChess->bSelected = false; // 娓呴櫎琚€夋嫨鐘舵€?
			}
		}
	}

	// 鑾峰彇鎵€鏈夌Щ鍔ㄦ柟寮?
	TArray<FChessMove2P> Moves = board2P->GenerateMovesForChess(Pos.X, Pos.Y, this);

	GameState->ShowSettingPoint2P(Moves, this);
}

void AChesses::ApplyMove(FChessMove2P Move)
{
	TargetPos = Position(Move.to.X, Move.to.Y); // 鑾峰彇绉诲姩鐨勭洰鏍囦綅缃?

	PlayMoveAnim();

	bSelected = false; // 绉婚櫎琚€変腑鐘舵€?
	ChessMesh->SetOverlayMaterial(nullptr); // 绉婚櫎鎻忚竟鏉愯川
	GameState->DismissSettingPoint2P(); // 闅愯棌鎵€鏈夎惤瀛愮偣
}

void AChesses::PlayMoveAnim()
{
	bAnimationing = true;
	TimeLine_ChessMove->PlayFromStart(); // 寮€濮嬫挱鏀剧Щ鍔ㄥ姩鐢?
}

FVector AChesses::CalculateParabolicPosition(const FVector& Start, const FVector& Vertex, const FVector& End, float T)
{
	// 澶勭悊鐧惧垎姣斿紓甯?
	if (T < 0)
		T = 0;
	if (T > 1)
		T = 1;

	// 璁＄畻鎺у埗鐐筆1锛屽亣璁炬姏鐗╃嚎瀵圭О锛岄《鐐瑰湪t=0.5
	FVector P0 = Start;
	FVector P2 = End;
	FVector P1 = 2 * Vertex - 0.5f * P0 - 0.5f * P2;

	// 浜屾璐濆灏旀洸绾垮叕寮?
	float OneMinusT = 1 - T;
	FVector Result = (OneMinusT * OneMinusT) * P0 + 2 * OneMinusT * T * P1 + (T * T) * P2;
	return Result;
}

void AChesses::SwitchToRealIdentity()
{
	MyColor = MyRealColor;
	MyType = MyRealType;
	ChessMask->SetVisibility(true);
	UpdateSelectable();
}

