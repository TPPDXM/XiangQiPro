// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPGameStateBase.h"
#include "XQPGameInstance.h"
#include "Async/Async.h"

#include "XiangQiPro/AI/AI2P.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/GameObject/ChessBoard2PActor.h"
#include "XiangQiPro/SaveGame/SaveGameLibrary.h"

#include "XiangQiPro/UI/InGame/UI_Battle2P_Base.h"

#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/Util/ChessMove.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/AsyncWorker.h"
#include "XiangQiPro/Util/EndingLibrary.h"

void AXQPGameStateBase::UpdateScore()
{
    //score1 = AI2P->Evaluate(EChessColor::REDCHESS);
    //score2 = AI2P->Evaluate(EChessColor::BLACKCHESS);
    if (HUD2P.IsValid())
    {
        HUD2P->UpdateScore(score1, score2);
    }
    else
    {
        ULogger::LogWarning(TEXT("AXQPGameStateBase::UpdateScore: HUD2P is nullptr."));
    }
}

AXQPGameStateBase::AXQPGameStateBase() : Super(), battleType(EBattleType::P2_AI)
{
}

void AXQPGameStateBase::BeginPlay()
{
	Super::BeginPlay();
    if (UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>())
    {
        AIDifficulty = static_cast<EAI2PDifficulty>(GI->AIDifficulty);
    }
}

void AXQPGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AIAsync)
    {
        if (AIAsync->IsRunning())
        {
            AI2P->StopThinkingImmediately(); // 停止AI工作
            AIAsync->StopAsyncWork(); // 停止异步任务
        }
    }

    // 释放掉UObject对象
    if (board2P)
        board2P->RemoveFromRoot();
    board2P = nullptr;

    if (AI2P)
        AI2P->RemoveFromRoot();
    AI2P = nullptr;

    board2PActor.Reset();

    Super::EndPlay(EndPlayReason);
}

void AXQPGameStateBase::GamePause(UObject* OwnerObject)
{
    UGameplayStatics::SetGamePaused(this, true);
    if (AIAsync)
    {
        if (AIAsync->IsRunning())
        {
            AIAsync->PauseAsyncWork(); // 暂停AI
        }
    }
    IIF_GameState::GamePause(OwnerObject);
}

void AXQPGameStateBase::GameResume(UObject* OwnerObject)
{
    UGameplayStatics::SetGamePaused(this, false);
    if (AIAsync)
    {
        if (AIAsync->IsPaused())
        {
            AIAsync->ResumeAsyncWork(); // 恢复AI
        }
    }
    IIF_GameState::GameResume(OwnerObject);
}

void AXQPGameStateBase::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target)
{
    DismissSettingPoint2P();
    board2P->ShowSettingPoint2P(Moves, Target);
}

void AXQPGameStateBase::DismissSettingPoint2P()
{
    board2P->DismissSettingPoint2P();
}

TWeakObjectPtr<UChessBoard2P> AXQPGameStateBase::GetChessBoard2P()
{
    return board2P;
}

EBattleType AXQPGameStateBase::GetBattleType()
{
    return battleType;
}

EPlayerTag AXQPGameStateBase::GetBattleTurn()
{
    return battleTurn;
}

void AXQPGameStateBase::SetHUD2P(TWeakObjectPtr<UUI_Battle2P_Base> hud2P)
{
    HUD2P = hud2P;
}

int32 AXQPGameStateBase::GetScore1() const
{
    return score1;
}

int32 AXQPGameStateBase::GetScore2() const
{
    return score2;
}

int32 AXQPGameStateBase::GetScore3() const
{
    return score3;
}

void AXQPGameStateBase::Start2PGame(TWeakObjectPtr<AChessBoard2PActor> InBoard2PActor)
{
    bGameOver = false;
    bIsJueSha = false;
    battleTurn = EPlayerTag::P1;

    board2PActor = InBoard2PActor;
    if (board2PActor.IsValid())
    {
        if (!board2P)
        {
            board2P = GetGameInstance()->GetSubsystem<UChessBoard2P>();
            board2P->AddToRoot();
        }

        board2P->InitializeBoard(board2PActor); // 初始化棋盘
        board2PActor->Init(board2P);

        switch (Cast<UXQPGameInstance>(GetGameInstance())->GetGameMode())
        {
        case EGameMode::Ending:
        {
            EXEC_ONENDINGGAMESTART(USaveGameLibrary::GetEndingGameLevel());
            break;
        }
        case EGameMode::AI2P:
            board2PActor->GenerateChesses(); // 棋盘Actor生成所有象棋并对其初始化
            break;
        case EGameMode::SoloRide: // 千里走单骑模式
            battleTurn = EPlayerTag::P1; // 玩家先行
            battleType = EBattleType::SoloRide;
            SoloRideHorse = board2PActor->GenerateChessesForSoloRide(); // 调用新的棋盘生成函数
            break;
        default:
            break;
        }

        if (!AI2P)
        {
            AI2P = GetGameInstance()->GetSubsystem<UAI2P>();
            AI2P->AddToRoot();
        }
    }
    else
    {
        ULogger::LogError(TEXT("AXQPGameStateBase::Start2PGame: Can't start two players game, because board2P actor is nullptr!"));
    }
}

void AXQPGameStateBase::ApplyMove2P(TWeakObjectPtr<AChesses> target, FChessMove2P move)
{
    if (battleType == EBattleType::SoloRide) // 千里走单骑模式
    {
        if (battleTurn == EPlayerTag::P1) // 玩家执棋
        {
            TWeakObjectPtr<AChesses> chess = board2P->GetChess(move.to);
            if (chess.IsValid())
            {
                EChessType Type = chess->GetType(); // 获取被吃掉的子的类型
                if (Type == EChessType::BING)
                {
                    SoloRideScore += 5; // 吃掉兵加5分
                }
                else
                {
                    SoloRideScore += 10; // 其他加10分
                }
                HUD2P->UpdateScore(SoloRideScore, 0); // 更新得分显示
            }
        }
    }
    if (HUD2P.IsValid())
    {
        HUD2P->AddOperatingRecord(battleTurn, target, move); // 记录走子
    }
    else
    {
        ULogger::LogWarning(TEXT("AXQPGameStateBase::ApplyMove2P"), TEXT("HUD2P is nullptr!"));
    }
    board2P->ApplyMove(target, move);
}

void AXQPGameStateBase::OnFinishMove2P()
{
    if (bGameOver)
    {
        ULogger::Log(TEXT("AXQPGameStateBase::OnFinishMove2P: GameOver"));
        return;
    }

    if (battleType == EBattleType::SoloRide && battleTurn == EPlayerTag::P1)
    {
        if (!SoloRideHorse.IsValid())
        {
            ULogger::LogError(TEXT("AXQPGameStateBase::OnFinishMove2P"), TEXT("SoloRideHorse is nullptr."));
            return;
        }
        Position horsePos = SoloRideHorse->GetPosition();
        auto horseMoves = board2P->GenerateMovesForChess(horsePos.X, horsePos.Y, SoloRideHorse);

        if (horseMoves.Num() <= 0)
        {
            EXEC_ONSOLORIDEDEFEAT(SoloRideScore); // 执行失败事件
            return;
        }

        auto moves = board2P->GenerateAllMoves(EChessColor::BLACKCHESS);
        for (const auto& move : moves)
        {
            // 检查玩家移动后是否被吃
            if (move.to == SoloRideHorse->GetPosition())
            {
                battleTurn = EPlayerTag::P2;
                ApplyMove2P(board2P->GetChess(move.from), move); // 吃掉玩家的马
                EXEC_ONSOLORIDEDEFEAT(SoloRideScore);            // 执行失败事件
                return;
            }
        }

        // 生成新的敌人
        GenerateNewEnemies();
        return;
    }

    SwitchBattleTurn(); // 轮换执棋

    switch (battleTurn) // 表示当前该谁了
    {
    case EPlayerTag::P1:
        HUD2P->SetAITurn(false); // 更新AI回合结束
        board2P->SetSideToMove(EChessColor::REDCHESS);
        break;
    case EPlayerTag::AI:
        board2P->SetSideToMove(EChessColor::BLACKCHESS);
        RunAI2P(); // 轮到AI
        break;
    case EPlayerTag::P2:
        board2P->SetSideToMove(EChessColor::BLACKCHESS);
        break;
    }
    UpdateScore(); // 更新得分
}

void AXQPGameStateBase::RunAI2P()
{
    HUD2P->SetAITurn(true);

    AIAsync = UAsyncWorker::CreateAndStartWorker(
         [this](UAsyncWorker* WorkerInstance)
         {
             if (Cast<UXQPGameInstance>(GetGameInstance())->GetGameMode() == EGameMode::Ending)
             {
                 AIDifficulty = EAI2PDifficulty::Normal;
                 AI2P->SetBoard(board2P);
                 if (AI2P->IsJueSha(EChessColor::BLACKCHESS))
                 {
                     bIsJueSha = true;
                     ULogger::Log(UTF8_TO_TCHAR("绝杀"));
                     return;
                 }
             }
             // 获取最佳移动方式和要移动的棋子 
             AIMove2P = AI2P->GetBestMove(board2P, EChessColor::BLACKCHESS, AIDifficulty);

             while (WorkerInstance->IsPaused())
             {
                 FPlatformProcess::Sleep(0.1f);
             }
         },
         [this](EAsyncWorkerState State)
         {
             if (State != EAsyncWorkerState::Cancelled) // 任务正常执行完成
             {
                 if (bIsJueSha)
                 {
                     bIsJueSha = false;
                     int32 EndingGameLevel = USaveGameLibrary::GetEndingGameLevel();
                     int32 EndingGameLevel_Max = USaveGameLibrary::GetEndingGameLevel_Max();
                     if (EndingGameLevel != EndingGameLevel_Max ||                      // 并非最大关卡
                         EndingGameLevel_Max + 1 < UEndingLibrary::GetEndingGameNum())  // 未到达峰值
                     {
                         USaveGameLibrary::UpdateEndingGameLevelData(); // 更新残局关卡数据
                     }
                     EXEC_ONJUESHA();
                     return;
                 }

                 if (AIMove2P.IsValid())
                 {
                     // 应用棋子的移动
                     AIMovedChess = board2P->GetChess(AIMove2P.from.X, AIMove2P.from.Y);
                     ApplyMove2P(AIMovedChess, AIMove2P);
                     ULogger::Log(TEXT("AXQPGameStateBase::RunAI2P: AI FINISH"));
                 }
                 else
                 {
                     ULogger::LogWarning(TEXT("AXQPGameStateBase::RunAI2P: Invalid move."));
                 }
             }
             else
             {
                 ULogger::LogWarning(TEXT("AXQPGameStateBase::RunAI2P: AI's work has been cancelled."));
             }
         }
    );
}

bool AXQPGameStateBase::IsMyTurn() const
{
    return MyPlayerTag == battleTurn;
}

void AXQPGameStateBase::SwitchBattleTurn()
{
    switch (battleType)
    {
    case EBattleType::P2:
        battleTurn = battleTurn == EPlayerTag::P1 ? EPlayerTag::P2 : EPlayerTag::P1;
        break;
    case EBattleType::P2_AI:
        battleTurn = battleTurn == EPlayerTag::AI ? EPlayerTag::P1 : EPlayerTag::AI;
        break;
    case EBattleType::P3:
        break;
    default:
        break;
    }
}

void AXQPGameStateBase::NotifyGameOver(EChessColor winner)
{
    bGameOver = true;
    HUD2P->ShowGameOver(winner);

    EXEC_GAMEOVER(); // 调用游戏结束事件
}

void AXQPGameStateBase::GenerateNewEnemies()
{
    EnemyGenerateWave++;

    // 1. 计算生成数量和类型概率
    int32 generateCount = 1;//Math::RandomIntegerInRange(1, 3);
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
    TArray<Position> safeMoves = GetAvailableMove(SoloRideHorse); // 获取当前安全移动位置

    // 3. 生成新敌人
    for (int32 i = 0; i < generateCount && safeMoves.Num() > 0; ++i)
    {
        // 从安全位置中随机选一个，确保生成后马至少还有一处可走
        int32 randomIndex = FMath::RandRange(0, safeMoves.Num() - 1);
        Position spawnPos = safeMoves[randomIndex];
        safeMoves.RemoveAt(randomIndex); // 移除此位置，避免下次重复

        // 随机选择棋子类型
        TSubclassOf<AChesses> chessClass = classPool[FMath::RandRange(0, classPool.Num() - 1)];

        // 生成敌人
        AChesses* newEnemy = board2PActor->SpawnChessAt(chessClass, spawnPos);
        newEnemy->Init(EChessColor::BLACKCHESS, spawnPos, board2P);
        newEnemy->FinishSpawning(FTransform(board2P->BoardLocs[spawnPos.X][spawnPos.Y]));
        board2P->AllChess[spawnPos.X][spawnPos.Y] = newEnemy;
    }
}

TArray<Position> AXQPGameStateBase::GetAvailableMove(TWeakObjectPtr<AChesses> Horse)
{
    Position HorsePos = SoloRideHorse->GetPosition();
    TArray<Position> Moves;
    for (int32 i = 0; i < 10; i++)
        for (int32 j = 0; j < 9; j++)
        {
            if (i != HorsePos.X - 1 && i != HorsePos.X && i != HorsePos.X + 1 &&
                j != HorsePos.Y - 1 && j != HorsePos.Y && j != HorsePos.Y + 1 &&
                !board2P->GetChess(i, j).IsValid())                                 // 排除掉马周围的位置以及有棋子占据的位置
            {
                Moves.Add({ i,j });
            }
        }
    return Moves;
}
