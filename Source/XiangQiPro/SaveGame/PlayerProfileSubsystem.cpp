#include "PlayerProfileSubsystem.h"
#include "SavePlayerProfile.h"
#include "Kismet/GameplayStatics.h"

namespace
{
const FXQPSaveKey ProfileKey{EXQPSaveDataType::PlayerProfile, EXQPGameMode::Default};

bool ValidProfile(USavePlayerProfile* Profile)
{
    if (!Profile || (Profile->Version != 1 && Profile->Version != 2) || Profile->Generation < 0) return false;
    Profile->MigrateLegacyData();
    for (const auto& Pair : Profile->ModeProgress)
    {
        const auto& P = Pair.Value;
        if (P.CurrentStreak < 0 || P.BestStreak < P.CurrentStreak || P.BestStreak > 1000000 ||
            P.RunDifficulty > 4 || P.SavedTurn > 1 || (!P.SavedBoard.IsEmpty() && P.SavedBoard.Num() != 90) ||
            !P.SavedDrawHistory.IsValid()) return false;
        for (uint8 Square : P.SavedBoard)
            if ((Square & 15) > 7 || (Square >> 4) > 1) return false;
    }
    for (const auto& Pair : Profile->ModeLearning)
    {
        const auto& P = Pair.Value;
        if (P.PositionMoveCounts.Num() > 4096 || P.AICaution.Num() > 4096 || P.HarmfulReplies.Num() > 4096 ||
            P.ObservedMoves < 0 || P.ObservedMoves > 100000 || P.PieceCounts.Num() > 8 || P.FileCounts.Num() > 9 ||
            P.CaptureMoves < 0 || P.CaptureMoves > P.ObservedMoves || P.ForwardMoves < 0 ||
            P.ForwardMoves > P.ObservedMoves || P.CheckMoves < 0 || P.CheckMoves > P.ObservedMoves) return false;
        for (int32 Count : P.PieceCounts) if (Count < 0 || Count > 100000) return false;
        for (int32 Count : P.FileCounts) if (Count < 0 || Count > 100000) return false;
        for (const auto& Item : P.PositionMoveCounts)
            if (Item.Key.Len() > 32 || Item.Value < 1 || Item.Value > 100000) return false;
        for (const auto& Item : P.AICaution)
            if (Item.Key.Len() > 32 || Item.Value < 1 || Item.Value > 100) return false;
        for (const auto& Item : P.HarmfulReplies)
            if (Item.Key.Len() > 32 || Item.Value < 1 || Item.Value > 100) return false;
    }
    return true;
}
}

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Profile = XQPSaveStorage::Load<USavePlayerProfile>(ProfileKey, ValidProfile);
    bool bMigrated = false;
    if (!Profile)
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const FString Slot = Index == 0 ? TEXT("XQP_PlayerProfile_A") : TEXT("XQP_PlayerProfile_B");
            if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) continue;
            auto* Loaded = Cast<USavePlayerProfile>(XQPSaveStorage::LoadSlot(Slot, 0, true));
            if (ValidProfile(Loaded) && (!Profile || Loaded->Generation > Profile->Generation))
            {
                Profile = Loaded;
                bMigrated = true;
            }
        }
    if (!Profile)
    {
        Profile = NewObject<USavePlayerProfile>(this);
        Profile->MigrateLegacyData();
    }
    Profile->ModeProgress.FindOrAdd(EXQPGameMode::AI2P);
    Profile->ModeLearning.FindOrAdd(EXQPGameMode::AI2P);
    LearningData().PieceCounts.SetNumZeroed(8);
    LearningData().FileCounts.SetNumZeroed(9);
    bChallengeSelected = ProgressData().bRunActive;
    if (bMigrated) Save();
}

FXQPChallengeProgress& UPlayerProfileSubsystem::ProgressData()
{
    return Profile->ModeProgress.FindChecked(EXQPGameMode::AI2P);
}

FXQPModeLearningData& UPlayerProfileSubsystem::LearningData()
{
    return Profile->ModeLearning.FindChecked(EXQPGameMode::AI2P);
}

const FXQPChallengeProgress& UPlayerProfileSubsystem::ProgressData() const
{
    const USavePlayerProfile* Data = Profile;
    return Data->ModeProgress.FindChecked(EXQPGameMode::AI2P);
}

const FXQPModeLearningData& UPlayerProfileSubsystem::LearningData() const
{
    const USavePlayerProfile* Data = Profile;
    return Data->ModeLearning.FindChecked(EXQPGameMode::AI2P);
}

void UPlayerProfileSubsystem::Deinitialize()
{
    Save();
    Super::Deinitialize();
}
FString UPlayerProfileSubsystem::GetPlayerName() const
{
    return Profile ? Profile->PlayerName : TEXT("棋友");
}
int32 UPlayerProfileSubsystem::GetBestStreak() const
{
    return Profile ? ProgressData().BestStreak : 0;
}
int32 UPlayerProfileSubsystem::GetCurrentStreak() const
{
    return Profile ? ProgressData().CurrentStreak : 0;
}
bool UPlayerProfileSubsystem::HasActiveRun() const
{
    return Profile && ProgressData().bRunActive;
}

bool UPlayerProfileSubsystem::SetPlayerName(const FString& Name)
{
    // 去掉控制字符和首尾空白，名字限制为16个UTF-16字符。
    FString Clean;
    for (TCHAR C : Name.TrimStartAndEnd())
        if (!FChar::IsControl(C))
            Clean.AppendChar(C);
    Clean = Clean.Left(16).TrimStartAndEnd();
    if (Clean.IsEmpty() || !Profile)
        return false;
    Profile->PlayerName = Clean;
    return Save();
}

uint8 UPlayerProfileSubsystem::BeginOrResumeRun(uint8 Difficulty)
{
    // 已有挑战的模型、棋局、连胜和起始难度都保留。
    if (!ProgressData().bRunActive)
    {
        ClearLearning();
        ProgressData().bRunActive = true;
        ProgressData().RunDifficulty = FMath::Min<uint8>(4, Difficulty);
        Save();
    }
    return ProgressData().RunDifficulty;
}

bool UPlayerProfileSubsystem::RestoreBoard(FChessPosition& OutBoard, EChessColor& OutTurn) const
{
    // 90格只存棋子类型和颜色，不反序列化Actor或旧世界引用。
    if (!ProgressData().bRunActive || ProgressData().SavedBoard.Num() != 90)
        return false;
    FChessPosition Restored;
    for (int32 I = 0; I < 90; ++I)
        Restored.Squares[I] = {static_cast<EChessType>(ProgressData().SavedBoard[I] & 15),
                               static_cast<EChessColor>(ProgressData().SavedBoard[I] >> 4)};
    // 单方将帅缺失表示终局写入后进程退出，交给GameState补结算。
    if (!FChessPosition::IsInside(Restored.FindKing(EChessColor::REDCHESS)) &&
        !FChessPosition::IsInside(Restored.FindKing(EChessColor::BLACKCHESS)))
        return false;
    OutBoard = Restored;
    OutTurn = static_cast<EChessColor>(ProgressData().SavedTurn);
    return true;
}

void UPlayerProfileSubsystem::RestoreMoveScores(int32& RedMoveScore, int32& BlackMoveScore) const
{
    RedMoveScore = ProgressData().SavedRedMoveScore;
    BlackMoveScore = ProgressData().SavedBlackMoveScore;
}

bool UPlayerProfileSubsystem::RestoreDrawHistory(FChessDrawHistory& OutHistory, const FChessPosition& Board, EChessColor Turn) const
{
    // 旧存档没有该字段；只有局面与执棋方匹配时才沿用计数，避免移植到另一局。
    if (!ProgressData().SavedDrawHistory.Matches(Board, Turn)) return false;
    OutHistory = ProgressData().SavedDrawHistory;
    return true;
}

void UPlayerProfileSubsystem::SaveBoard(const FChessPosition& Board, EChessColor Turn, int32 RedMoveScore, int32 BlackMoveScore,
    const FChessDrawHistory* DrawHistory)
{
    // 每手保存已提交的逻辑棋盘，动画尚未完成也能继续正确回合。
    if (!ProgressData().bRunActive)
        return;
    ProgressData().SavedBoard.SetNum(90);
    for (int32 I = 0; I < 90; ++I)
        ProgressData().SavedBoard[I] =
            static_cast<uint8>(Board.Squares[I].Type) | (static_cast<uint8>(Board.Squares[I].Color) << 4);
    ProgressData().SavedTurn = static_cast<uint8>(Turn);
    ProgressData().SavedRedMoveScore = RedMoveScore;
    ProgressData().SavedBlackMoveScore = BlackMoveScore;
    ProgressData().SavedDrawHistory = DrawHistory ? *DrawHistory : FChessDrawHistory{};
    Save();
}

void UPlayerProfileSubsystem::ObserveMove(const FChessPosition& Before, const FChessMove2P& Move)
{
    // 样本只来自连战中的玩家落子，普通模式和局域网不会污染模型。
    if (!ProgressData().bRunActive || !Move.IsValid() || Before.Get(Move.from).Color != EChessColor::REDCHESS)
        return;
    ++LearningData().ObservedMoves;
    ++LearningData().PieceCounts[static_cast<uint8>(Before.Get(Move.from).Type)];
    ++LearningData().FileCounts[Move.to.Y];
    if (Before.Get(Move.to).Type != EChessType::EMPTY)
        ++LearningData().CaptureMoves;
    if (Move.to.X > Move.from.X)
        ++LearningData().ForwardMoves;
    FChessPosition After = Before;
    After.MakeMove(Move);
    if (After.IsInCheck(EChessColor::BLACKCHESS))
        ++LearningData().CheckMoves;
    const FString Key = FChessLearning::SampleKey(Before, Move);
    // 保存这次反击，下一次AI搜索完成后用新评分检验上一手的预期。
    LearningData().LastPlayerReplyKey = Key;
    if (LearningData().PositionMoveCounts.Contains(Key) || LearningData().PositionMoveCounts.Num() < 4096)
        ++LearningData().PositionMoveCounts.FindOrAdd(Key);
    // 按比例衰减防止计数溢出，并逐渐适应近期风格。
    if (LearningData().ObservedMoves >= 100000)
    {
        LearningData().ObservedMoves /= 2;
        LearningData().CaptureMoves /= 2;
        LearningData().ForwardMoves /= 2;
        LearningData().CheckMoves /= 2;
        for (int32& Count : LearningData().PieceCounts)
            Count /= 2;
        for (int32& Count : LearningData().FileCounts)
            Count /= 2;
        for (auto& Pair : LearningData().PositionMoveCounts)
            Pair.Value = FMath::Max(1, Pair.Value / 2);
    }
}

void UPlayerProfileSubsystem::ClearLearning()
{
    auto& Progress = ProgressData();
    Progress.CurrentStreak = 0;
    Progress.SavedBoard.Reset();
    Progress.SavedDrawHistory = {};
    Progress.SavedTurn = 0;
    Progress.SavedRedMoveScore = Progress.SavedBlackMoveScore = 0;
    auto& Learning = LearningData();
    Learning = FXQPModeLearningData{};
    Learning.PieceCounts.Init(0, 8);
    Learning.FileCounts.Init(0, 9);
}

// 比较相邻AI回合的搜索评分，至少损失一个兵才记录反例，减少估值噪声。
void UPlayerProfileSubsystem::ReviewAIOutcome(int32 CurrentScore)
{
    if (LearningData().LastAIMoveKey.IsEmpty() || LearningData().LastPlayerReplyKey.IsEmpty())
        return;
    const int32 Drop = LearningData().LastExpectedScore - CurrentScore;
    if (Drop >= 70)
    {
        const int32 Severity = FMath::Clamp(Drop / 70, 1, 10);
        // 容量和计数都有上限，长时间挑战不会无限扩张存档。
        if (LearningData().AICaution.Contains(LearningData().LastAIMoveKey) || LearningData().AICaution.Num() < 4096)
        {
            int32& Count = LearningData().AICaution.FindOrAdd(LearningData().LastAIMoveKey);
            Count = FMath::Min(100, Count + Severity);
        }
        if (LearningData().HarmfulReplies.Contains(LearningData().LastPlayerReplyKey) || LearningData().HarmfulReplies.Num() < 4096)
        {
            int32& Count = LearningData().HarmfulReplies.FindOrAdd(LearningData().LastPlayerReplyKey);
            Count = FMath::Min(100, Count + Severity);
        }
    }
    LearningData().LastAIMoveKey.Reset();
    LearningData().LastPlayerReplyKey.Reset();
}

void UPlayerProfileSubsystem::RecordAIChoice(const FChessPosition& Before, const FChessMove2P& Move,
                                             int32 ExpectedScore)
{
    // 留下当前预期，暂停或退出后也能继续评价同一组AI落子/玩家回应。
    LearningData().LastAIMoveKey = FChessLearning::SampleKey(Before, Move, EChessColor::BLACKCHESS);
    LearningData().LastExpectedScore = ExpectedScore;
    LearningData().LastPlayerReplyKey.Reset();
}

void UPlayerProfileSubsystem::CompleteRound(bool bPlayerWon)
{
    if (!ProgressData().bRunActive)
        return;
    if (bPlayerWon)
    {
        ReviewAIOutcome(-100000); // 终局没有下一次搜索，用败局本身记录最后一次反例。
        ProgressData().CurrentStreak = FMath::Min(ProgressData().CurrentStreak + 1, 1000000);
        ProgressData().BestStreak = FMath::Max(ProgressData().BestStreak, ProgressData().CurrentStreak);
        ProgressData().SavedBoard.Reset();
        ProgressData().SavedDrawHistory = {};
        ProgressData().SavedRedMoveScore = ProgressData().SavedBlackMoveScore = 0;
    }
    else
    {
        ProgressData().bRunActive = false;
        ClearLearning();
    }
    Save();
    if (!bPlayerWon)
        Save(); // 覆盖备份，防止旧槽恢复已结束的挑战。
}

void UPlayerProfileSubsystem::CompleteDrawRound()
{
    if (!ProgressData().bRunActive) return;
    // 和棋不递增连胜，也不清空已有连胜/模型；仅结束当前盘以便同难度重赛。
    ProgressData().SavedBoard.Reset();
    ProgressData().SavedDrawHistory = {};
    ProgressData().SavedTurn = 0;
    ProgressData().SavedRedMoveScore = ProgressData().SavedBlackMoveScore = 0;
    LearningData().LastAIMoveKey.Reset();
    LearningData().LastPlayerReplyKey.Reset();
    Save();
}

void UPlayerProfileSubsystem::RestartCurrentRound()
{
    // 与和棋后的重赛共用清理规则，不触发胜负结算，也不重新创建挑战模型。
    // Start2PGame 随后会写入全新的初始棋盘，退出续接仍然可靠。
    CompleteDrawRound();
}

FChessLearning UPlayerProfileSubsystem::LearningSnapshot() const
{
    // 按值复制后交给搜索线程，后续存档更新不会与后台读数据竞争。
    FChessLearning Result;
    Result.Observations = LearningData().ObservedMoves;
    for (int32 I = 0; I < 8; ++I)
        Result.PieceCounts[I] = LearningData().PieceCounts[I];
    for (int32 I = 0; I < 9; ++I)
        Result.FileCounts[I] = LearningData().FileCounts[I];
    Result.Captures = LearningData().CaptureMoves;
    Result.Advances = LearningData().ForwardMoves;
    Result.Checks = LearningData().CheckMoves;
    Result.PositionMoves = LearningData().PositionMoveCounts;
    Result.AICaution = LearningData().AICaution;
    Result.HarmfulReplies = LearningData().HarmfulReplies;
    return Result;
}

void UPlayerProfileSubsystem::EndChallenge()
{
    ProgressData().BestStreak = FMath::Max(ProgressData().BestStreak, ProgressData().CurrentStreak);
    ProgressData().bRunActive = bChallengeSelected = false;
    ClearLearning();
    Save();
    Save(); // 两槽均写入结束状态，防止恢复上一份未结束的挑战。
}

bool UPlayerProfileSubsystem::Save()
{
    if (!Profile)
        return false;
    // UE完成单个槽的写入，另一槽保留上一份完整版本。
    const bool bOK = XQPSaveStorage::Save(Profile, ProfileKey);
    SaveError = bOK ? FString() : TEXT("保存失败，请检查本地存储空间和写入权限");
    if (!bOK)
        UE_LOG(LogTemp, Error, TEXT("XQP: %s"), *SaveError);
    return bOK;
}
