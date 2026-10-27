// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SettingPoint.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SphereComponent.h"

#include "../Chess/Chesses.h"
#include "../GameObject/ChessBoard2P.h"
#include "../GameMode/XQPGameStateBase.h"
#include "../Util/ObjectManager.h"

// Sets default values
ASettingPoint::ASettingPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	NS_SettingPoint = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_SETTINGPOINT);

	PointNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PointNiagara"));
	PointNiagara->SetAsset(NS_SettingPoint);
	PointNiagara->bAutoActivate = false;
	PointNiagara->SetActive(false);
	RootComponent = PointNiagara;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetSphereRadius(3);
	Sphere->SetupAttachment(PointNiagara);

	// 鼠标事件
	Sphere->OnClicked.AddDynamic(this, &ASettingPoint::HandleClick);
	Sphere->OnBeginCursorOver.AddDynamic(this, &ASettingPoint::HandleHoverStart);
	Sphere->OnEndCursorOver.AddDynamic(this, &ASettingPoint::HandleHoverEnd);

	// 触摸事件
	Sphere->OnInputTouchEnter.AddDynamic(this, &ASettingPoint::OnInputTouchEnter);
	Sphere->OnInputTouchLeave.AddDynamic(this, &ASettingPoint::OnInputTouchLeave);
	Sphere->OnInputTouchEnd.AddDynamic(this, &ASettingPoint::OnInputTouchEnd);
}

// Called when the game starts or when spawned
void ASettingPoint::BeginPlay()
{
	Super::BeginPlay();
	GameState = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
	DefaultWorldPosition = GetActorLocation(); // 获取默认世界坐标
}

// 响应重开事件并清理上一局状态。
void ASettingPoint::GamePlayAgain(UObject* OwnerObject)
{
	Destroy();
	IIF_GameState::GamePlayAgain(OwnerObject);
}

// Called every frame
void ASettingPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// 将触摸进入转换为悬停反馈。
void ASettingPoint::OnInputTouchEnter(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent)
{
	HandleHoverStart(TouchedComponent);
}

// 将触摸离开转换为悬停结束反馈。
void ASettingPoint::OnInputTouchLeave(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent)
{
	HandleHoverEnd(TouchedComponent);
}

// 将触摸结束转换为落子点击。
void ASettingPoint::OnInputTouchEnd(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent)
{
	HandleClick(TouchedComponent, EKeys::LeftMouseButton);
}

// 统一的悬停进入处理
void ASettingPoint::HandleHoverStart(UPrimitiveComponent* TouchedComponent)
{
	PointNiagara->SetVectorParameter(FName(TEXT("RayColor")), FVector(1, 0, 0));
}

// 统一的悬停离开处理
void ASettingPoint::HandleHoverEnd(UPrimitiveComponent* TouchedComponent)
{
	PointNiagara->SetVectorParameter(FName(TEXT("RayColor")), FVector(1, 1, 1));
}

// 统一的点击处理
void ASettingPoint::HandleClick(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	if (!bActive || !GameState.IsValid() || !TargetChess.IsValid() || !GameState->IsMyTurn()) return;
	GameState->ApplyMove2P(TargetChess, FChessMove2P(TargetChess->GetPosition(), Position2P));
}

// 切换落子点显示和碰撞响应。
void ASettingPoint::SetActivate(bool bInActive)
{
	bActive = bInActive;
	PointNiagara->SetActive(bActive);
	if (bActive)
	{
		PointNiagara->Activate();
		PointNiagara->SetHiddenInGame(false);
		Sphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block); // 开始相应鼠标

		const auto Board = GameState.IsValid() ? GameState->GetChessBoard2P() : nullptr;
		const auto chess = Board.IsValid() ? Board->GetChess(Position2P) : nullptr;
		if (chess.IsValid() && !chess->IsDead()) // 可以吃子
		{
			SetActorLocation(DefaultWorldPosition + FVector(0, 0, 2.5f)); // 向上偏移,到棋子顶部
		}
	}
	else
	{
		PointNiagara->Deactivate();
		PointNiagara->SetHiddenInGame(true);
		SetActorLocation(DefaultWorldPosition); // 恢复默认位置
		Sphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore); // 禁止相应鼠标
	}
}

// 设置落子点当前关联的棋子。
void ASettingPoint::SetTargetChess(TWeakObjectPtr<AChesses> targetChess)
{
	TargetChess = targetChess;
}

// 获取落子点关联棋子的弱引用。
TWeakObjectPtr<AChesses> ASettingPoint::GetTargetChess()
{
	return TargetChess;
}

// 获取落子点棋盘坐标。
FVector2D ASettingPoint::GetPosition2P()
{
	return Position2P;
}

// 设置落子点棋盘坐标。
void ASettingPoint::SetPosition2P(Position InPosition2P)
{
	Position2P = InPosition2P;
}

