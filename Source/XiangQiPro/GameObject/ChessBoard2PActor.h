// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Interface/IF_GameState.h"
#include "XiangQiPro/Interface/IF_EndingGame.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChessBoard2PActor.generated.h"

#define PATH_SM_CHESSBOARD_2P TEXT("/Script/Engine.StaticMesh'/Game/Mesh/ChessBoard/SM_ChessBoard_2P.SM_ChessBoard_2P'")

class UStaticMeshComponent;
class UChessBoard2P;

UCLASS()
class XIANGQIPRO_API AChessBoard2PActor : public AActor, public IIF_GameState, public IIF_EndingGame
{
	GENERATED_BODY()

	TWeakObjectPtr<UChessBoard2P> Board2P;

public:

	// 棋盘左下角坐标
	FVector BorderLoc1 = FVector(-38, -30, 1.3f);

	// 棋盘右上角坐标
	FVector BorderLoc2 = FVector(38, 30, 1.3f);
	
public:	
	// Sets default values for this actor's properties
	AChessBoard2PActor();

	void Init(TWeakObjectPtr<UChessBoard2P> InBoard2P);

	UStaticMeshComponent* ChessBoardMesh;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void GamePlayAgain(UObject* OwnerObject) override;

	virtual void OnEndingGameStart(UObject* OwnerObject, int32 Index) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	class AChesses* SpawnChessAt(const TSubclassOf<AActor>& ActorClass, const FIntPoint& Pos);

	// 生成棋子Actor并保存到ChessBoard2P
	void GenerateChesses();

	void GenerateSettingPoints();

	TWeakObjectPtr<class AChesses> GenerateChessesForSoloRide();

	FIntPoint GetRandomEmptyPosition(const TArray<FIntPoint>& BlockedPositions);

};
