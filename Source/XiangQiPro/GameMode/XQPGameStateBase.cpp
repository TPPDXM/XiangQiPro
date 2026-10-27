// Copyright 2026 Ultimate Player All Rights Reserved.

#include "XQPGameStateBase.h"
#include "XQPGameInstance.h"
#include "SoloRideMode.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "XiangQiPro/AI/AI2P.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/GameObject/ChessBoard2PActor.h"
#include "XiangQiPro/SaveGame/SaveGameLibrary.h"
#include "XiangQiPro/UI/InGame/UI_Battle2P_Base.h"
#include "XiangQiPro/Util/EndingLibrary.h"

// 设置默认对战类型和难度。
AXQPGameStateBase::AXQPGameStateBase() : battleType(EBattleType::P2_AI), AIDifficulty(EAI2PDifficulty::Hard)
{
}

// 读取游戏实例中的用户配置。
void AXQPGameStateBase::BeginPlay()
{
    Super::BeginPlay();
    if (const UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>())
        AIDifficulty = static_cast<EAI2PDifficulty>(FMath::Clamp(static_cast<int32>(GI->AIDifficulty), 0, 3));
}

// 离开关卡后旧任务仅会释放自身数据，不能回写GameState。
void AXQPGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    CancelAIRequest();
    if (IsValid(board2P)) board2P->ResetBoard();
    if (battleType == EBattleType::SoloRide) USoloRideMode::ResetSoloRideData();
    board2P = nullptr;
    AI2P = nullptr;
    board2PActor.Reset();
    HUD2P.Reset();
    Super::EndPlay(EndPlayReason);
}

// 原子取消后台计算，并递增请求号拒绝迟到的结果。
void AXQPGameStateBase::CancelAIRequest()
{
    ++AIRequestId;
    if (ActiveSearch.IsValid()) ActiveSearch->Cancel();
    ActiveSearch.Reset();
    bHasPendingAIResult = false;
    PendingAIResult = {};
}

// 暂停时允许后台完成计算，但暂停期间不提交走法。
void AXQPGameStateBase::GamePause(UObject* OwnerObject)
{
    bAIPaused = true;
    UGameplayStatics::SetGamePaused(this, true);
    IIF_GameState::GamePause(OwnerObject);
}

// 恢复后提交已完成的结果，避免工作线程忙等暂停状态。
void AXQPGameStateBase::GameResume(UObject* OwnerObject)
{
    UGameplayStatics::SetGamePaused(this, false);
    bAIPaused = false;
    if (bHasPendingAIResult)
    {
        const FChessSearchResult Result = PendingAIResult;
        bHasPendingAIResult = false;
        ApplyAIResult(Result);
    }
    IIF_GameState::GameResume(OwnerObject);
}

// 等本次重开广播销毁旧棋子后，再初始化新局。
void AXQPGameStateBase::GamePlayAgain(UObject* OwnerObject)
{
    CancelAIRequest();
    bGameOver = true;
    bMoveInProgress = false;
    bAIPaused = false;
    UGameplayStatics::SetGamePaused(this, false);
    const uint64 RestartId = AIRequestId;
    const TWeakObjectPtr<AXQPGameStateBase> WeakThis(this);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, RestartId]()
        {
            if (WeakThis.IsValid() && !WeakThis->bEndingPlay && WeakThis->AIRequestId == RestartId)
                WeakThis->Start2PGame(WeakThis->board2PActor);
        }));
    }
    IIF_GameState::GamePlayAgain(OwnerObject);
}

// 建立本局依赖并按模式生成棋子，子系统由GameInstance管理生命周期。
void AXQPGameStateBase::Start2PGame(TWeakObjectPtr<AChessBoard2PActor> InBoard2PActor)
{
    CancelAIRequest();
    UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>();
    if (!IsValid(GI) || !InBoard2PActor.IsValid() || bEndingPlay) return;
    board2P = GI->GetSubsystem<UChessBoard2P>();
    AI2P = GI->GetSubsystem<UAI2P>();
    if (!IsValid(board2P) || !IsValid(AI2P)) return;
    board2PActor = InBoard2PActor;
    bGameOver = false;
    bMoveInProgress = false;
    bAIPaused = false;
    battleTurn = EPlayerTag::P1;
    battleType = EBattleType::P2_AI;
    AIDifficulty = static_cast<EAI2PDifficulty>(FMath::Clamp(static_cast<int32>(GI->AIDifficulty), 0, 3));
    board2P->bVariantRules = GI->GetGameMode() == EXQPGameMode::GuessWho || GI->GetGameMode() == EXQPGameMode::SoloRide;
    board2P->bRequireKing = GI->GetGameMode() != EXQPGameMode::SoloRide;
    board2P->InitializeBoard(board2PActor);
    board2PActor->Init(board2P);
    switch (GI->GetGameMode())
    {
    case EXQPGameMode::Ending:
    {
        AIDifficulty = EAI2PDifficulty::Normal;
        EXEC_ONENDINGGAMESTART(USaveGameLibrary::GetEndingGameLevel());
        break;
    }
    case EXQPGameMode::SoloRide:
        battleType = EBattleType::SoloRide;
        USoloRideMode::InitSoloRideGame(this);
        USoloRideMode::SoloRideHorse = board2PActor->GenerateChessesForSoloRide();
        USoloRideMode::GenerateNewEnemies(3);
        break;
    case EXQPGameMode::GuessWho:
        AIDifficulty = EAI2PDifficulty::Easy;
        board2PActor->GenerateChessesForGuessWho();
        break;
    default:
        board2PActor->GenerateChesses();
        break;
    }
    if (HUD2P.IsValid()) HUD2P->SetAITurn(false);
    UpdateScore();
}

// 验证执棋方和走法后锁定动画阶段，避免重复点击导致连续落子。
void AXQPGameStateBase::ApplyMove2P(TWeakObjectPtr<AChesses> Target, FChessMove2P Move)
{
    if (bGameOver || bEndingPlay || bAIPaused || bMoveInProgress || !IsValid(board2P) || !Target.IsValid()) return;
    const EChessColor Side = battleTurn == EPlayerTag::P1 ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    if (Target->GetColor() != Side || !board2P->IsLegalMove(Target, Move)) return;
    bMoveInProgress = true;
    if (battleType == EBattleType::SoloRide) USoloRideMode::OnApplyMove(Move);
    if (HUD2P.IsValid()) HUD2P->AddOperatingRecord(battleTurn, Target, Move);
    SwitchBattleTurn(); // 棋谱记录当前执棋方后再轮换
    board2P->ApplyMove(Target, Move);
}

// 动画结束后更新身份、胜负和下一回合。
void AXQPGameStateBase::OnFinishMove2P(TWeakObjectPtr<AChesses> Target)
{
    bMoveInProgress = false;
    if (bGameOver || bEndingPlay || !IsValid(board2P)) return;
    if (battleType == EBattleType::SoloRide)
    {
        if (battleTurn == EPlayerTag::P1) USoloRideMode::OnFinishMove();
        UpdateScore();
        return;
    }
    const UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>();
    if (IsValid(GI) && GI->GetGameMode() == EXQPGameMode::GuessWho && Target.IsValid()) Target->SwitchToRealIdentity();
    const EChessColor Side = battleTurn == EPlayerTag::P1 ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    board2P->SetSideToMove(Side);
    UpdateScore();
    if (board2P->GenerateAllMoves(Side).IsEmpty())
    {
        Finish2PGame(FChessPosition::Opponent(Side));
        return;
    }
    if (HUD2P.IsValid()) HUD2P->SetAITurn(battleTurn == EPlayerTag::AI);
    if (battleTurn == EPlayerTag::AI) RunAI2P();
}

// 在主线程创建快照，后台闭包只持有搜索数据和弱回调目标。
void AXQPGameStateBase::RunAI2P()
{
    if (bGameOver || bEndingPlay || bAIPaused || bMoveInProgress || battleTurn != EPlayerTag::AI ||
        ActiveSearch.IsValid() || bHasPendingAIResult || !IsValid(AI2P) || !IsValid(board2P)) return;
    ActiveSearch = AI2P->CreateSearch(board2P, EChessColor::BLACKCHESS, AIDifficulty);
    if (!ActiveSearch.IsValid()) return;
    SearchBoardKey = ActiveSearch->GetPositionKey();
    SearchBoardRevision = board2P->GetRevision();
    const uint64 RequestId = ++AIRequestId;
    const auto Search = ActiveSearch;
    const TWeakObjectPtr<AXQPGameStateBase> WeakThis(this);
    if (HUD2P.IsValid()) HUD2P->SetAITurn(true);
    Async(EAsyncExecution::ThreadPool, [Search, WeakThis, RequestId]()
    {
        const FChessSearchResult Result = Search->FindBestMove();
        // 回到游戏线程后才解析弱引用，不跨线程读取GameState或棋子。
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Result]()
        {
            if (WeakThis.IsValid()) WeakThis->ReceiveAIResult(RequestId, Result);
        });
    });
}

// 只接收当前请求的结果，暂停期间缓存值类型结果。
void AXQPGameStateBase::ReceiveAIResult(uint64 RequestId, const FChessSearchResult& Result)
{
    if (bEndingPlay || bGameOver || RequestId != AIRequestId) return;
    ActiveSearch.Reset();
    if (Result.bCancelled) return;
    if (bAIPaused)
    {
        PendingAIResult = Result;
        bHasPendingAIResult = true;
        return;
    }
    ApplyAIResult(Result);
}

// 提交前重新核对局面，过期结果不能落到新棋盘。
void AXQPGameStateBase::ApplyAIResult(const FChessSearchResult& Result)
{
    if (bEndingPlay || bGameOver || battleTurn != EPlayerTag::AI || !IsValid(board2P)) return;
    if (board2P->GetRevision() != SearchBoardRevision || board2P->CreateSnapshot().Hash(EChessColor::BLACKCHESS) != SearchBoardKey)
    {
        RunAI2P(); // 当前棋盘发生变化，重新提交搜索
        return;
    }
    if (Result.bNoLegalMoves)
    {
        Finish2PGame(EChessColor::REDCHESS);
        return;
    }
    if (Result.Move.IsValid()) ApplyMove2P(board2P->GetChess(Result.Move.from), Result.Move);
}

// 统一残局通关和普通对局结束，残局只在确实获胜后推进关卡。
void AXQPGameStateBase::Finish2PGame(EChessColor Winner)
{
    if (bGameOver) return;
    const UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>();
    if (Winner == EChessColor::REDCHESS && IsValid(GI) && GI->GetGameMode() == EXQPGameMode::Ending)
    {
        bGameOver = true;
        CancelAIRequest();
        DismissSettingPoint2P();
        if (HUD2P.IsValid()) HUD2P->SetAITurn(false);
        if (USaveGameLibrary::GetEndingGameLevel() + 1 < UEndingLibrary::GetEndingGameNum())
            USaveGameLibrary::UpdateEndingGameLevelData();
        EXEC_ONJUESHA();
        return;
    }
    bGameOver = true;
    CancelAIRequest();
    DismissSettingPoint2P();
    if (HUD2P.IsValid())
    {
        HUD2P->SetAITurn(false);
        HUD2P->ShowGameOver(Winner);
    }
    EXEC_GAMEOVER();
}

// 吃将和无合法走法共用同一结算入口。
void AXQPGameStateBase::NotifyGameOver(EChessColor Winner)
{
    Finish2PGame(Winner);
}

// 计算双方子力，单骑模式显示累计得分。
void AXQPGameStateBase::UpdateScore()
{
    score1 = 0;
    score2 = 0;
    score3 = 0;
    if (battleType == EBattleType::SoloRide) score1 = USoloRideMode::SoloRideScore;
    else if (IsValid(board2P))
    {
        const FChessPosition Snapshot = board2P->CreateSnapshot();
        for (const FChessPiece2P& Piece : Snapshot.Squares)
        {
            if (Piece.Type == EChessType::JIANG) continue;
            if (Piece.Color == EChessColor::REDCHESS) score1 += FChessPosition::PieceValue(Piece.Type);
            else if (Piece.Color == EChessColor::BLACKCHESS) score2 += FChessPosition::PieceValue(Piece.Type);
        }
    }
    if (HUD2P.IsValid()) HUD2P->UpdateScore(score1, score2);
}

// 更新可落子位置的提示。
void AXQPGameStateBase::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target) const
{
    if (!IsMyTurn() || !IsValid(board2P) || !Target.IsValid()) return;
    DismissSettingPoint2P();
    board2P->ShowSettingPoint2P(MoveTemp(Moves), Target);
}

// 安全清除当前落子提示。
void AXQPGameStateBase::DismissSettingPoint2P() const
{
    if (IsValid(board2P)) board2P->DismissSettingPoint2P();
}

// 获取棋盘数据的弱引用。
TWeakObjectPtr<UChessBoard2P> AXQPGameStateBase::GetChessBoard2P() { return board2P; }

// 获取棋盘Actor的弱引用。
TWeakObjectPtr<AChessBoard2PActor> AXQPGameStateBase::GetChessBoard2PActor() const { return board2PActor; }

// 获取本局对战类型。
EBattleType AXQPGameStateBase::GetBattleType() const { return battleType; }

// 获取当前执棋方。
EPlayerTag AXQPGameStateBase::GetBattleTurn() const { return battleTurn; }

// 修改执棋方时取消旧回合的搜索。
void AXQPGameStateBase::SetBattleTurn(EPlayerTag BT)
{
    if (battleTurn != BT) CancelAIRequest();
    battleTurn = BT;
}

// 绑定界面并同步当前得分和回合。
void AXQPGameStateBase::SetHUD2P(TWeakObjectPtr<UUI_Battle2P_Base> InHUD)
{
    HUD2P = InHUD;
    UpdateScore();
    if (HUD2P.IsValid()) HUD2P->SetAITurn(!bGameOver && battleTurn == EPlayerTag::AI);
}

// 获取玩家1得分。
int32 AXQPGameStateBase::GetScore1() const { return score1; }

// 获取玩家2得分。
int32 AXQPGameStateBase::GetScore2() const { return score2; }

// 获取玩家3得分。
int32 AXQPGameStateBase::GetScore3() const { return score3; }

// 动画、暂停及游戏结束时禁止玩家再次落子。
bool AXQPGameStateBase::IsMyTurn() const
{
    return !bGameOver && !bEndingPlay && !bAIPaused && !bMoveInProgress && MyPlayerTag == battleTurn;
}

// 根据对战模式切换下一执棋方。
void AXQPGameStateBase::SwitchBattleTurn()
{
    switch (battleType)
    {
    case EBattleType::P2: battleTurn = battleTurn == EPlayerTag::P1 ? EPlayerTag::P2 : EPlayerTag::P1; break;
    case EBattleType::P2_AI: battleTurn = battleTurn == EPlayerTag::AI ? EPlayerTag::P1 : EPlayerTag::AI; break;
    default: break;
    }
}
