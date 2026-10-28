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
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "ProBattleComponent.h"
#include "BattleStartComponent.h"
#include "XiangQiPro/Camera/Camera2PActor.h"
#include "EngineUtils.h"

// 设置默认对战类型和难度。
AXQPGameStateBase::AXQPGameStateBase() : battleType(EBattleType::P2_AI), AIDifficulty(EAI2PDifficulty::Hard)
{
    ProBattle = CreateDefaultSubobject<UProBattleComponent>(TEXT("ProBattle"));
    BattleStart = CreateDefaultSubobject<UBattleStartComponent>(TEXT("BattleStart"));
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
    GetWorld()->GetTimerManager().ClearTimer(ProResultTimer);
    ProBattle->Reset();
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
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AIReactionTimer);
    if (ActiveSearch.IsValid()) ActiveSearch->Cancel();
    ActiveSearch.Reset();
    bHasPendingAIResult = false;
    PendingAIResult = {};
}

// 暂停时允许后台完成计算，但暂停期间不提交走法。
void AXQPGameStateBase::GamePause(UObject* OwnerObject)
{
    if (IsLANGame()) return; // 联机菜单不暂停双方棋盘或连接。
    bAIPaused = true;
    UGameplayStatics::SetGamePaused(this, true);
    IIF_GameState::GamePause(OwnerObject);
}

// 恢复后提交已完成的结果，避免工作线程忙等暂停状态。
void AXQPGameStateBase::GameResume(UObject* OwnerObject)
{
    if (IsLANGame()) return;
    UGameplayStatics::SetGamePaused(this, false);
    bAIPaused = false;
    if (bHasPendingAIResult)
    {
        const FChessSearchResult Result = PendingAIResult;
        bHasPendingAIResult = false;
        ReceiveAIResult(AIRequestId, Result);
    }
    IIF_GameState::GameResume(OwnerObject);
}

// 等本次重开广播销毁旧棋子后，再初始化新局。
void AXQPGameStateBase::GamePlayAgain(UObject* OwnerObject)
{
    if (IsLANGame()) return; // 双方准备后由房主统一开新局。
    CancelAIRequest();
    bGameOver = true;
    GetWorld()->GetTimerManager().ClearTimer(ProResultTimer);
    BattleStart->Stop();
    ProBattle->Reset();
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
    GetWorld()->GetTimerManager().ClearTimer(ProResultTimer);
    ProBattle->Reset();
    UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>();
    if (!IsValid(GI) || !InBoard2PActor.IsValid() || bEndingPlay) return;
    board2P = GI->GetSubsystem<UChessBoard2P>();
    AI2P = GI->GetSubsystem<UAI2P>();
    if (!IsValid(board2P) || !IsValid(AI2P)) return;
    board2PActor = InBoard2PActor;
    bGameOver = false;
    CurrentEndingIndex = INDEX_NONE;
    bMoveInProgress = false;
    bAIPaused = false;
    battleTurn = EPlayerTag::P1;
    battleType = EBattleType::P2_AI;
    bEnableMachineLearning = GI->GetGameMode() == EXQPGameMode::AI2P && GI->GetSubsystem<UPlayerProfileSubsystem>()->bChallengeSelected;
    if (IsLANGame())
    {
        battleType = EBattleType::P2;
        MyPlayerTag = GI->GetSubsystem<UXQPLANSubsystem>()->GetLocalColor() == EChessColor::REDCHESS ? EPlayerTag::P1 : EPlayerTag::P2;
    }
    else MyPlayerTag = EPlayerTag::P1;
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
        CurrentEndingIndex = USaveGameLibrary::GetEndingGameLevel();
        EXEC_ONENDINGGAMESTART(CurrentEndingIndex);
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
    {
        FChessPosition Restored;
        EChessColor RestoredTurn = EChessColor::REDCHESS;
        UPlayerProfileSubsystem* Profile = GI->GetSubsystem<UPlayerProfileSubsystem>();
        if (bEnableMachineLearning)
            AIDifficulty = static_cast<EAI2PDifficulty>(Profile->BeginOrResumeRun(GI->AIDifficulty));
        if (bEnableMachineLearning && Profile->RestoreBoard(Restored, RestoredTurn))
        {
            board2PActor->GenerateFromPosition(Restored);
            battleTurn = RestoredTurn == EChessColor::REDCHESS ? EPlayerTag::P1 : EPlayerTag::AI;
            board2P->SetSideToMove(RestoredTurn);
        }
        else board2PActor->GenerateChesses();
        if (bEnableMachineLearning) Profile->SaveBoard(board2P->CreateSnapshot(), GetTurnColor());
        break;
    }
    }
    if (IsProBattle()) ProBattle->Start(GI->ProSettings, IsLANGame(), GI->GetGameMode() == EXQPGameMode::ProBattleAI);
    BattleStart->BeginSetup();
    if (HUD2P.IsValid()) HUD2P->ShowBattleStart();
    if (HUD2P.IsValid()) HUD2P->SetAITurn(false);
    UpdateScore();
    if (IsLANGame()) GI->GetSubsystem<UXQPLANSubsystem>()->NotifyBoardReady();
    // 恢复时也检查终局，覆盖落子动画或结算前退出的情况。
    if (bEnableMachineLearning && board2P->GenerateAllMoves(GetTurnColor()).IsEmpty())
    {
        const EChessColor Winner = FChessPosition::Opponent(GetTurnColor());
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Winner]() { Finish2PGame(Winner); }));
    }
    else if (battleTurn == EPlayerTag::AI) GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AXQPGameStateBase::RunAI2P);
}

// 验证执棋方和走法后锁定动画阶段，避免重复点击导致连续落子。
void AXQPGameStateBase::ApplyMove2P(TWeakObjectPtr<AChesses> Target, FChessMove2P Move)
{
    if (IsProBattle())
    {
        if (!IsMyTurn() || !Target.IsValid()) return;
        if (IsLANGame()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->SubmitProMove(Move);
        else ProBattle->TryMove(Target.Get(), Move, EChessColor::REDCHESS);
        return;
    }
    if (!BattleStart->HasStarted() || bGameOver || bEndingPlay || bAIPaused || bMoveInProgress || !IsValid(board2P) || !Target.IsValid()) return;
    const EChessColor Side = battleTurn == EPlayerTag::P1 ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    if (Target->GetColor() != Side || !board2P->IsLegalMove(Target, Move)) return;
    if (IsLANGame() && !bApplyingNetworkMove)
    {
        if (IsMyTurn()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->SubmitMove(Move);
        return;
    }
    bMoveInProgress = true;
    if (bEnableMachineLearning)
    {
        UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
        FChessPosition Snapshot = board2P->CreateSnapshot();
        if (Side == EChessColor::REDCHESS) Profile->ObserveMove(Snapshot, Move);
        Snapshot.MakeMove(Move);
        Profile->SaveBoard(Snapshot, FChessPosition::Opponent(Side));
    }
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
    if (IsProBattle() || !BattleStart->HasStarted()) return;
    if (bGameOver || bEndingPlay || bAIPaused || bMoveInProgress || battleTurn != EPlayerTag::AI ||
        ActiveSearch.IsValid() || bHasPendingAIResult || !IsValid(AI2P) || !IsValid(board2P)) return;
    ActiveSearch = AI2P->CreateSearch(board2P, EChessColor::BLACKCHESS, AIDifficulty);
    if (!ActiveSearch.IsValid()) return;
    SearchBoardKey = ActiveSearch->GetPositionKey();
    SearchBoardRevision = board2P->GetRevision();
    const uint64 RequestId = ++AIRequestId;
    AIActAfter = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.35f, 0.85f);
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
    const float ReactionLeft = FMath::Max(0.f, float(AIActAfter - GetWorld()->GetTimeSeconds()));
    if (bAIPaused || ReactionLeft > 0.f)
    {
        PendingAIResult = Result;
        bHasPendingAIResult = true;
        if (!bAIPaused)
            GetWorld()->GetTimerManager().SetTimer(AIReactionTimer, FTimerDelegate::CreateWeakLambda(this, [this, RequestId]()
            {
                if (RequestId != AIRequestId || !bHasPendingAIResult) return;
                const auto Ready = PendingAIResult;
                bHasPendingAIResult = false;
                ReceiveAIResult(RequestId, Ready);
            }), FMath::Max(0.01f, ReactionLeft), false);
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
    if (Result.Move.IsValid())
    {
        if (bEnableMachineLearning && Result.CompletedDepth > 0)
        {
            UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
            const FChessPosition Snapshot = board2P->CreateSnapshot();
            // 回加经验代价后再比较，避免把历史惩罚误当成一次新的局面损失。
            const int32 RawScore = Result.Move.score + Profile->LearningSnapshot().AvoidancePenalty(Snapshot, Result.Move);
            Profile->ReviewAIOutcome(RawScore);
            Profile->RecordAIChoice(Snapshot, Result.Move, RawScore);
        }
        ApplyMove2P(board2P->GetChess(Result.Move.from), Result.Move);
    }
}

// 统一残局通关和普通对局结束，残局只在确实获胜后推进关卡。
void AXQPGameStateBase::Finish2PGame(EChessColor Winner)
{
    if (bGameOver) return;
    UpdateScore();
    WinnerColor = Winner;
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
    if (bEnableMachineLearning) GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->CompleteRound(Winner == EChessColor::REDCHESS);
    if (IsLANGame()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->NotifyMatchFinished();
    if (HUD2P.IsValid())
    {
        HUD2P->SetAITurn(false);
        HUD2P->ShowMatchResult(Winner);
    }
    EXEC_GAMEOVER();
}

// 吃将和无合法走法共用同一结算入口。
void AXQPGameStateBase::NotifyGameOver(EChessColor Winner)
{
    if (IsProBattle()) { ProBattle->Finish(Winner); return; }
    Finish2PGame(Winner);
}

bool AXQPGameStateBase::IsProBattle() const
{
    const auto* GI = GetGameInstance<UXQPGameInstance>();
    return GI && (GI->GetGameMode() == EXQPGameMode::ProBattleAI || GI->GetGameMode() == EXQPGameMode::ProBattleLAN);
}

void AXQPGameStateBase::FinishProBattle(EChessColor Winner)
{
    if (bGameOver || bEndingPlay) return;
    bGameOver = true;
    WinnerColor = Winner;
    CancelAIRequest();
    DismissSettingPoint2P();
    if (IsLANGame()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->NotifyMatchFinished();
    for (TActorIterator<ACamera2PActor> It(GetWorld()); It; ++It) It->PlayProResultCamera();
    EXEC_GAMEOVER();
    GetWorld()->GetTimerManager().SetTimer(ProResultTimer, FTimerDelegate::CreateWeakLambda(this, [this, Winner]()
    {
        if (HUD2P.IsValid() && bGameOver) HUD2P->ShowMatchResult(Winner);
    }), 1.05f, false);
}

// 经典对弈显示相对于各自阵营的局面评估，单骑模式显示累计得分。
void AXQPGameStateBase::UpdateScore()
{
    score1 = 0;
    score2 = 0;
    score3 = 0;
    if (battleType == EBattleType::SoloRide) score1 = USoloRideMode::SoloRideScore;
    else if (IsValid(board2P))
    {
        const FChessPosition Snapshot = board2P->CreateSnapshot();
        score1 = Snapshot.Evaluate(EChessColor::REDCHESS);
        score2 = Snapshot.Evaluate(EChessColor::BLACKCHESS);
    }
    if (HUD2P.IsValid()) HUD2P->UpdateScore(GetScore1(), GetScore2());
}

// 更新可落子位置的提示。
void AXQPGameStateBase::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target) const
{
    if (!IsMyTurn() || !IsValid(board2P) || !Target.IsValid()) return;
    if (!IsProBattle()) DismissSettingPoint2P();
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
    if (HUD2P.IsValid() && !BattleStart->HasStarted()) HUD2P->ShowBattleStart();
    UpdateScore();
    if (HUD2P.IsValid()) HUD2P->SetAITurn(!bGameOver && battleTurn == EPlayerTag::AI);
}

// 获取玩家1得分。
int32 AXQPGameStateBase::GetScore1() const { return IsProBattle() ? ProBattle->GetStats(EChessColor::REDCHESS).CaptureScore : score1; }

// 获取玩家2得分。
int32 AXQPGameStateBase::GetScore2() const { return IsProBattle() ? ProBattle->GetStats(EChessColor::BLACKCHESS).CaptureScore : score2; }

// 获取玩家3得分。
int32 AXQPGameStateBase::GetScore3() const { return score3; }

// 动画、暂停及游戏结束时禁止玩家再次落子。
bool AXQPGameStateBase::IsMyTurn() const
{
    if (!BattleStart->HasStarted()) return false;
    if (IsProBattle())
        return !bGameOver && !bEndingPlay && !bAIPaused && ProBattle->IsRunning() &&
            (!IsLANGame() || GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->IsBattleReady());
    if (IsLANGame() && !GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->CanLocalMove()) return false;
    return !bGameOver && !bEndingPlay && !bAIPaused && !bMoveInProgress && MyPlayerTag == battleTurn;
}

bool AXQPGameStateBase::IsLANGame() const
{
    const UXQPGameInstance* GI = GetGameInstance<UXQPGameInstance>();
    return GI && (GI->GetGameMode() == EXQPGameMode::LAN || GI->GetGameMode() == EXQPGameMode::ProBattleLAN);
}

bool AXQPGameStateBase::CanApplyNetworkMove() const
{
    return BattleStart->HasStarted() && IsLANGame() && !bGameOver && !bEndingPlay && !bMoveInProgress && IsValid(board2P);
}

void AXQPGameStateBase::OnBattleStarted()
{
    if (!bGameOver && !IsProBattle() && battleTurn == EPlayerTag::AI) RunAI2P();
}

void AXQPGameStateBase::ApplyNetworkMove(const FChessMove2P& Move)
{
    if (!CanApplyNetworkMove()) return;
    TGuardValue<bool> Applying(bApplyingNetworkMove, true);
    ApplyMove2P(board2P->GetChess(Move.from), Move);
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
