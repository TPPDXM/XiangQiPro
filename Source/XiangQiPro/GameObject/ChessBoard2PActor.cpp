// Copyright 2026 Ultimate Player All Rights Reserved.


#include "ChessBoard2PActor.h"
#include "Components/StaticMeshComponent.h"

#include "ChessBoard2P.h"
#include "XiangQiPro/Chess/AllChessHeader.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameObject/SettingPoint.h"
#include "XiangQiPro/Util/ChessMove.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/ObjectManager.h"
#include "XiangQiPro/Util/EndingLibrary.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
AChessBoard2PActor::AChessBoard2PActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ChessBoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChessBoardMeshComponent"));
	ChessBoardMesh->SetStaticMesh(OM::GetConstructorObject<UStaticMesh>(PATH_SM_CHESSBOARD_2P));
}

void AChessBoard2PActor::Init(TWeakObjectPtr<UChessBoard2P> InBoard2P)
{
    Board2P = InBoard2P;
}

// Called when the game starts or when spawned
void AChessBoard2PActor::BeginPlay()
{
	Super::BeginPlay();
    BorderLoc1 += (GetActorLocation() + FVector(0, 0, 1.5f));
    BorderLoc2 += (GetActorLocation() + FVector(0, 0, 1.5f));
    AXQPGameStateBase* GameState = Cast<AXQPGameStateBase>(GetWorld()->GetGameState());
    if (GameState)
    {
        GameState->Start2PGame(this);
    }
}

void AChessBoard2PActor::GamePlayAgain(UObject* OwnerObject)
{
    AXQPGameStateBase* GameState = Cast<AXQPGameStateBase>(GetWorld()->GetGameState());
    if (GameState)
    {
        GameState->Start2PGame(this);
    }
    IIF_GameState::GamePlayAgain(OwnerObject);
}

void AChessBoard2PActor::OnEndingGameStart(UObject* OwnerObject, int32 Index)
{
    auto Infos = UEndingLibrary::GetChessGenerateInfo(Index);
    for (const auto& info : Infos)
    {
        FVector WorldLoc = Board2P->BoardLocs[info.Pos.X][info.Pos.Y]; // 世界坐标
        FTransform Transform(WorldLoc);

        // 生成棋子
        AChesses* Chess = SpawnChessAt(info.Class, info.Pos);
        Chess->Init(info.Color, info.Pos, Board2P); // 初始化棋子
        Chess->FinishSpawning(Transform);

        // 将棋子保存到棋盘中
        Board2P->AllChess[info.Pos.X][info.Pos.Y] = Chess;
    }
    GenerateSettingPoints();
    IIF_EndingGame::OnEndingGameStart(OwnerObject, Index);
}

// Called every frame
void AChessBoard2PActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

AChesses* AChessBoard2PActor::SpawnChessAt(const TSubclassOf<AActor>& ActorClass, const FIntPoint& Pos)
{
    FVector WorldLoc = Board2P->BoardLocs[Pos.X][Pos.Y]; // 世界坐标
    FTransform Transform(WorldLoc);
    // 生成棋子
    AChesses* Chess = Cast<AChesses>(
        UGameplayStatics::BeginDeferredActorSpawnFromClass(
            GetWorld(),
            ActorClass,  // 用指定类型生成棋子
            Transform,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        )
    );
    return Chess;
}

void AChessBoard2PActor::GenerateChesses()
{
    if (!Board2P.IsValid())
    {
        ULogger::LogError(TEXT("Can't generate chesses, because Board2P is nullptr!"));
        return;
    }

    TArray<FIntPoint> Indexs = { {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {0, 8}, {2, 1}, {2, 7}, {3, 0}, {3, 2}, {3, 4}, {3, 6}, {3, 8},
                                           {9, 0}, {9, 1}, {9, 2}, {9, 3}, {9, 4}, {9, 5}, {9, 6}, {9, 7}, {9, 8}, {7, 1}, {7, 7}, {6, 0}, {6, 2}, {6, 4}, {6, 6}, {6, 8} };
    
    TArray<TSubclassOf<AChesses>> Classes = { 
    AChess_Jv::StaticClass(), AChess_Ma::StaticClass(), AChess_Xiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Jiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Xiang::StaticClass(), AChess_Ma::StaticClass(), AChess_Jv::StaticClass(), 
    AChess_Pao::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(),
    AChess_Jv::StaticClass(), AChess_Ma::StaticClass(), AChess_Xiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Jiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Xiang::StaticClass(), AChess_Ma::StaticClass(), AChess_Jv::StaticClass(),
    AChess_Pao::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass() };

#define RED EChessColor::REDCHESS
#define BLACK EChessColor::BLACKCHESS

    TArray<EChessColor> Colors = { RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, 
    BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK };

    for (int32 i = 0; i < 32; i++)
    {
        // 生成棋子
        AChesses* Chess = SpawnChessAt(Classes[i], Indexs[i]);
        Chess->Init(Colors[i], Indexs[i], Board2P); // 初始化棋子
        Chess->FinishSpawning(FTransform(Board2P->BoardLocs[Indexs[i].X][Indexs[i].Y]));

        // 将棋子保存到棋盘中
        Board2P->AllChess[Indexs[i].X][Indexs[i].Y] = Chess;
    }

    GenerateSettingPoints();
}

void AChessBoard2PActor::GenerateSettingPoints()
{
    /************生成落子点*************/
    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            FVector WorldLoc = Board2P->BoardLocs[i][j]; // 世界坐标
            WorldLoc.Z -= 1.3f; // 和棋子的高度不一样
            FTransform Transform(WorldLoc);

            // 生成置棋位置Actor
            ASettingPoint* SettingPoint = Cast<ASettingPoint>(
                UGameplayStatics::BeginDeferredActorSpawnFromClass(
                    GetWorld(),
                    ASettingPoint::StaticClass(),
                    Transform,
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                )
            );
            SettingPoint->SetPosition2P(Position(i, j));
            SettingPoint->FinishSpawning(Transform);

            // 保存到棋盘中
            Board2P->SettingPoints[i][j] = SettingPoint;
        }
    }
}

TWeakObjectPtr<AChesses> AChessBoard2PActor::GenerateChessesForSoloRide()
{

    // 生成红方马在棋盘中心 (4, 4)
    TWeakObjectPtr<AChesses> redHorse = SpawnChessAt(AChess_Ma::StaticClass(), FIntPoint(4, 4));
    if (redHorse.IsValid())
    {
        redHorse->Init(EChessColor::REDCHESS, FIntPoint(4, 4), Board2P);
        redHorse->FinishSpawning(FTransform(Board2P->BoardLocs[4][4]));
        Board2P->AllChess[4][4] = redHorse;
    }

    GenerateSettingPoints(); // 生成可移动点指示器
    return redHorse;
}
