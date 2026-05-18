// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SoloRideMode.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/GameObject/ChessBoard2PActor.h"
#include "XiangQiPro/Chess/AllChessHeader.h"
#include "XiangQiPro/Util/Logger.h"

AXQPGameStateBase* USoloRideMode::GS = nullptr;
std::vector<std::pair<TWeakObjectPtr<AChesses>, int32>> USoloRideMode::SoloRideEnemies = std::vector<std::pair<TWeakObjectPtr<AChesses>, int32>>(0);
TWeakObjectPtr<AChesses> USoloRideMode::SoloRideHorse = nullptr;
int32 USoloRideMode::SoloRideScore = 0;
int32 USoloRideMode::EnemyGenerateWave = 0;

void USoloRideMode::InitSoloRideGame(AXQPGameStateBase* InGameState)
{
    GS = InGameState;
}

bool USoloRideMode::OnApplyMove(FChessMove2P move)
{
    if (GS->GetBattleTurn() == EPlayerTag::P1) // 玩家执棋
    {
        TWeakObjectPtr<AChesses> chess = GS->board2P->GetChess(move.to);
        if (chess.IsValid() && !chess->IsDead())
        {
            EChessType Type = chess->GetType(); // 获取被吃掉的子的类型
            if (Type == EChessType::BING)
            {
                USoloRideMode::SoloRideScore += 5; // 吃掉兵加5分
            }
            else
            {
                USoloRideMode::SoloRideScore += 10; // 其他加10分
            }
            return true;
        }
    }
    return false;
}

void USoloRideMode::OnFinishMove()
{
    if (!SoloRideHorse.IsValid())
    {
        ULogger::LogError(TEXT("AXQPGameStateBase::OnFinishMove2P"), TEXT("SoloRideHorse is nullptr."));
        return;
    }
    Position horsePos = SoloRideHorse->GetPosition();
    auto horseMoves = GS->board2P->GenerateMovesForChess(horsePos.X, horsePos.Y, SoloRideHorse);

    if (horseMoves.Num() <= 0)
    {
#define this GS
        EXEC_ONSOLORIDEDEFEAT(SoloRideScore); // 执行失败事件
#undef this
        return;
    }

    auto moves = GS->board2P->GenerateAllMoves(EChessColor::BLACKCHESS);
    for (const auto& move : moves)
    {
        // 检查玩家移动后是否被吃
        if (move.to == SoloRideHorse->GetPosition())
        {
            GS->SetBattleTurn(EPlayerTag::P2);
            GS->ApplyMove2P(GS->GetChessBoard2P()->GetChess(move.from), move); // 吃掉玩家的马
#define this GS
            EXEC_ONSOLORIDEDEFEAT(SoloRideScore); // 执行失败事件
#undef this
            return;
        }
    }

    // 生成新的敌人
    GenerateNewEnemies();
}

void USoloRideMode::GenerateNewEnemies(int32 GenerateCount)
{
    EnemyGenerateWave++;

    // 1. 计算生成数量和类型概率
    TArray<TSubclassOf<AChesses>> classPool;

    // 根据波次调整概率池
    if (EnemyGenerateWave < 8)
    {
        // 前期：100% 兵
        for (int i = 0; i < 10; ++i) classPool.Add(AChess_Bing::StaticClass());
    }
    else if (EnemyGenerateWave < 15)
    {
        // 中期：80% 兵，20% 象
        for (int i = 0; i < 8; ++i) classPool.Add(AChess_Bing::StaticClass());
        for (int i = 0; i < 2; ++i) classPool.Add(AChess_Xiang::StaticClass());
    }
    else
    {
        // 后期：70% 兵，20% 象，10% 马
        for (int i = 0; i < 7; ++i) classPool.Add(AChess_Bing::StaticClass());
        for (int i = 0; i < 2; ++i) classPool.Add(AChess_Xiang::StaticClass());
        for (int i = 0; i < 1; ++i) classPool.Add(AChess_Ma::StaticClass());
    }

    // 2. 查找红方马的位置和安全移动位置
    TArray<Position> safeMoves = GetAvailablePos(SoloRideHorse); // 获取当前安全移动位置

    // 3. 生成新敌人
    for (int32 i = 0; i < GenerateCount && safeMoves.Num() > 0; ++i)
    {
        // 从安全位置中随机选一个，确保生成后马至少还有一处可走
        int32 randomIndex = FMath::RandRange(0, safeMoves.Num() - 1);
        Position spawnPos = safeMoves[randomIndex];
        safeMoves.RemoveAt(randomIndex); // 移除此位置，避免下次重复

        // 随机选择棋子类型
        TSubclassOf<AChesses> chessClass = classPool[FMath::RandRange(0, classPool.Num() - 1)];

        // 生成敌人
        AChesses* newEnemy = GS->GetChessBoard2PActor()->SpawnChessAt(chessClass, spawnPos);
        newEnemy->Init(EChessColor::BLACKCHESS, spawnPos, GS->board2P);
        newEnemy->FinishSpawning(FTransform(GS->board2P->BoardLocs[spawnPos.X][spawnPos.Y]));
        GS->board2P->AllChess[spawnPos.X][spawnPos.Y] = newEnemy;

        SoloRideEnemies.push_back({ newEnemy, EnemyGenerateWave + Math::RandomIntegerInRange(3, 5) });
    }

    for (auto it = SoloRideEnemies.begin(); it != SoloRideEnemies.end();)
    {
        TWeakObjectPtr<AChesses> chess = (*it).first;
        int32 DestroyWave = (*it).second;
        if (!chess.IsValid())
        {
            SoloRideEnemies.erase(it);
            continue;
        }

        if (EnemyGenerateWave >= DestroyWave)
        {
            chess->Defeated();
        }
        ++it;
    }
}

TArray<Position> USoloRideMode::GetAvailablePos(TWeakObjectPtr<AChesses> Horse)
{
    Position HorsePos = SoloRideHorse->GetPosition();
    TArray<Position> Moves;
    for (int32 i = 0; i < 10; i++)
        for (int32 j = 0; j < 9; j++)
        {
            if (i != HorsePos.X - 1 && i != HorsePos.X && i != HorsePos.X + 1 &&
                j != HorsePos.Y - 1 && j != HorsePos.Y && j != HorsePos.Y + 1 &&
                !GS->board2P->GetChess(i, j).IsValid())                                 // 排除掉马周围的位置以及有棋子占据的位置
            {
                Moves.Add({ i,j });
            }
        }
    return Moves;
}

void USoloRideMode::ResetSoloRideData()
{
    SoloRideEnemies.clear();
    SoloRideScore = 0;
    EnemyGenerateWave = 0;
}

