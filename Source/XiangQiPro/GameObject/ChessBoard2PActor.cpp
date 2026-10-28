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
#include "XiangQiPro/Util/ArrayToolLibrary.h"

#include "Kismet/GameplayStatics.h"

#define RED EChessColor::REDCHESS
#define BLACK EChessColor::BLACKCHESS

// Sets default values
AChessBoard2PActor::AChessBoard2PActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ChessBoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChessBoardMeshComponent"));
    RootComponent = ChessBoardMesh;
	ChessBoardMesh->SetStaticMesh(OM::GetConstructorObject<UStaticMesh>(PATH_SM_CHESSBOARD_2P));
}

// 绑定本局棋盘数据。
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

// 响应重开事件并清理上一局状态。
void AChessBoard2PActor::GamePlayAgain(UObject* OwnerObject)
{
    // 新局由GameState在广播结束后统一启动。
    IIF_GameState::GamePlayAgain(OwnerObject);
}

// 根据残局数据表生成棋子和落子点。
void AChessBoard2PActor::OnEndingGameStart(UObject* OwnerObject, int32 Index)
{
    if (!Board2P.IsValid()) return;
    auto Infos = UEndingLibrary::GetChessGenerateInfo(Index);
    for (const auto& info : Infos)
    {
        if (!Board2P->BoardLocs.IsValidIndex(info.Pos.X) || !Board2P->BoardLocs[info.Pos.X].IsValidIndex(info.Pos.Y)) continue;
        FVector WorldLoc = Board2P->BoardLocs[info.Pos.X][info.Pos.Y]; // 世界坐标
        FTransform Transform(WorldLoc);

        // 生成棋子
        AChesses* Chess = SpawnChessAt(info.Class, info.Pos);
        if (!IsValid(Chess)) continue;
        Chess->Init(info.Color, info.Pos, Board2P); // 初始化棋子
        Chess->FinishSpawning(Transform);

        // 将棋子保存到棋盘中
        Board2P->SetChess(info.Pos.X, info.Pos.Y, Chess);
    }
    GenerateSettingPoints();
    IIF_EndingGame::OnEndingGameStart(OwnerObject, Index);
}

// Called every frame
void AChessBoard2PActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// 检查类型和坐标后延迟生成棋子Actor。
AChesses* AChessBoard2PActor::SpawnChessAt(const TSubclassOf<AActor>& ActorClass, const FIntPoint& Pos)
{
    if (!IsValid(GetWorld()) || !ActorClass || !ActorClass->IsChildOf(AChesses::StaticClass()) || !Board2P.IsValid() ||
        !Board2P->BoardLocs.IsValidIndex(Pos.X) || !Board2P->BoardLocs[Pos.X].IsValidIndex(Pos.Y)) return nullptr;
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

// 生成标准象棋初始棋盘。
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
    AChess_Pao::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass() };

    for (int32 i = 0; i < 32; i++)
    {
        // 生成棋子
        AChesses* Chess = SpawnChessAt(Classes[i % 16], Indexs[i]);
        if (!IsValid(Chess)) continue;
        Chess->Init(i < 16 ? RED : BLACK, Indexs[i], Board2P); // 初始化棋子
        Chess->FinishSpawning(FTransform(Board2P->BoardLocs[Indexs[i].X][Indexs[i].Y]));

        // 将棋子保存到棋盘中
        Board2P->SetChess(Indexs[i].X, Indexs[i].Y, Chess);
    }

    GenerateSettingPoints();
}

// 为每个棋盘格生成落子提示Actor。
void AChessBoard2PActor::GenerateFromPosition(const FChessPosition& PositionData)
{
    TSubclassOf<AChesses> Classes[] = {nullptr, AChess_Jiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Xiang::StaticClass(),
        AChess_Ma::StaticClass(), AChess_Jv::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass()};
    for (int32 I = 0; I < 90; ++I)
    {
        const FChessPiece2P& Piece = PositionData.Squares[I];
        const uint8 Type = static_cast<uint8>(Piece.Type);
        if (Type == 0 || Type > 7) continue;
        const FIntPoint Pos(I / 9, I % 9);
        AChesses* Chess = SpawnChessAt(Classes[Type], Pos);
        if (!Chess) continue;
        Chess->Init(Piece.Color, Pos, Board2P);
        Chess->FinishSpawning(FTransform(Board2P->BoardLocs[Pos.X][Pos.Y]));
        Board2P->SetChess(Pos.X, Pos.Y, Chess);
    }
    GenerateSettingPoints();
}

// 为每个棋盘格生成落子提示Actor。
void AChessBoard2PActor::GenerateSettingPoints()
{
    if (!Board2P.IsValid() || !GetWorld()) return;
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
            if (!IsValid(SettingPoint)) continue;
            SettingPoint->SetPosition2P(Position(i, j));
            SettingPoint->FinishSpawning(Transform);

            // 保存到棋盘中
            Board2P->SettingPoints[i][j] = SettingPoint;
        }
    }
}

// 生成单骑玩法的玩家马和落子提示。
TWeakObjectPtr<AChesses> AChessBoard2PActor::GenerateChessesForSoloRide()
{

    // 生成红方马在棋盘中心 (4, 4)
    TWeakObjectPtr<AChesses> redHorse = SpawnChessAt(AChess_Ma::StaticClass(), FIntPoint(4, 4));
    if (redHorse.IsValid())
    {
        redHorse->Init(EChessColor::REDCHESS, FIntPoint(4, 4), Board2P);
        redHorse->FinishSpawning(FTransform(Board2P->BoardLocs[4][4]));
        Board2P->SetChess(4, 4, redHorse);
    }

    GenerateSettingPoints(); // 生成可移动点指示器
    return redHorse;
}

/*
* 保存两份生成坐标，打乱其中一份，然后按照类型生成棋子
* 此时的棋子在初始化时就已经保存了真实的类型和颜色
* 后续再按照另一份未打乱的坐标赋予每个棋子假的颜色和类型，用于在揭露真实面目前玩家和电脑的操控
*
*/
void AChessBoard2PActor::GenerateChessesForGuessWho()
{
    if (!Board2P.IsValid())
    {
        ULogger::LogError(TEXT("Can't generate chesses, because Board2P is nullptr!"));
        return;
    }

    // 双方将需确定位置生成
    auto JiangChess = SpawnChessAt(AChess_Jiang::StaticClass(), FIntPoint(0, 4));
    if (!IsValid(JiangChess)) return;
    JiangChess->Init(RED, FIntPoint(0, 4), Board2P);
    JiangChess->FinishSpawning(FTransform(Board2P->BoardLocs[0][4]));
    Board2P->SetChess(0, 4, JiangChess);

    JiangChess = SpawnChessAt(AChess_Jiang::StaticClass(), FIntPoint(9, 4));
    if (!IsValid(JiangChess)) return;
    JiangChess->Init(BLACK, FIntPoint(9, 4), Board2P);
    JiangChess->FinishSpawning(FTransform(Board2P->BoardLocs[9][4]));
    Board2P->SetChess(9, 4, JiangChess);

    TArray<FIntPoint> Pos = { {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 5}, {0, 6}, {0, 7}, {0, 8}, {2, 1}, {2, 7}, {3, 0}, {3, 2}, {3, 4}, {3, 6}, {3, 8},
                                           {9, 0}, {9, 1}, {9, 2}, {9, 3}, {9, 5}, {9, 6}, {9, 7}, {9, 8}, {7, 1}, {7, 7}, {6, 0}, {6, 2}, {6, 4}, {6, 6}, {6, 8} };
    TArray<FIntPoint> ShuffledPos(Pos);
    UArrayToolLibrary::ShuffleArray(ShuffledPos); // 打乱生成位置

    TArray<TSubclassOf<AChesses>> Classes = {
    AChess_Jv::StaticClass(), AChess_Ma::StaticClass(), AChess_Xiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Shi::StaticClass(), AChess_Xiang::StaticClass(), AChess_Ma::StaticClass(), AChess_Jv::StaticClass(),
    AChess_Pao::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass(), AChess_Bing::StaticClass() };

    for (int32 i = 0; i < 30; i++)
    {
        // 生成棋子
        AChesses* Chess = SpawnChessAt(Classes[i % 15], ShuffledPos[i]);
        if (!IsValid(Chess)) continue;
        Chess->Init(i < 15 ? RED : BLACK, ShuffledPos[i], Board2P); // 初始化棋子
        Chess->ChessMask->SetVisibility(false); // 隐藏棋子的Mask显示

        // 将棋子保存到棋盘中
        Board2P->SetChess(ShuffledPos[i].X, ShuffledPos[i].Y, Chess);
    }

    // 赋予假的类型和颜色
    TArray<EChessType> FakeType = { EChessType::JV, EChessType::MA, EChessType::XIANG, EChessType::SHI, EChessType::SHI, EChessType::XIANG, EChessType::MA, EChessType::JV,
    EChessType::PAO, EChessType::PAO, EChessType::BING , EChessType::BING , EChessType::BING , EChessType::BING , EChessType::BING };
    for (int32 i = 0; i < 15; i++)
    {
        const auto chess = Board2P->GetChess(Pos[i]);
        if (!chess.IsValid()) continue;
        chess->SetColor(RED);
        chess->SetType(FakeType[i]);
        chess->FinishSpawning(FTransform(Board2P->BoardLocs[Pos[i].X][Pos[i].Y]));
    }
    for (int32 i = 15; i < 30; i++)
    {
        const auto chess = Board2P->GetChess(Pos[i]);
        if (!chess.IsValid()) continue;
        chess->SetColor(BLACK);
        chess->SetType(FakeType[i - 15]);
        chess->FinishSpawning(FTransform(Board2P->BoardLocs[Pos[i].X][Pos[i].Y]));
    }
    GenerateSettingPoints();
}

#undef RED
#undef BLACK
