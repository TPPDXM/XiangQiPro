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

    EXEC_GAMEOVER(); // 调用游戏介绍事件
}
