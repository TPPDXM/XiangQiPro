#include "PlayerProfileSubsystem.h"
#include "SavePlayerProfile.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool ValidProfile(const USavePlayerProfile* P)
{
    if (!P || P->Version != 1 || P->CurrentStreak < 0 || P->BestStreak < P->CurrentStreak || P->BestStreak > 1000000 ||
        P->RunDifficulty > 3 || P->SavedTurn > 1 || P->PositionMoveCounts.Num() > 4096 || P->AICaution.Num() > 4096 ||
        P->HarmfulReplies.Num() > 4096)
        return false;
    if (P->ObservedMoves < 0 || P->ObservedMoves > 100000 || P->PieceCounts.Num() > 8 || P->FileCounts.Num() > 9)
        return false;
    for (int32 Count : P->PieceCounts)
        if (Count < 0 || Count > 100000)
            return false;
    for (int32 Count : P->FileCounts)
        if (Count < 0 || Count > 100000)
            return false;
    // 防止损坏的计数参与乘法后溢出，样本键也有明确长度上限。
    for (const auto& Pair : P->PositionMoveCounts)
        if (Pair.Key.Len() > 32 || Pair.Value < 1 || Pair.Value > 100000)
            return false;
    for (const auto& Pair : P->AICaution)
        if (Pair.Key.Len() > 32 || Pair.Value < 1 || Pair.Value > 100)
            return false;
    for (const auto& Pair : P->HarmfulReplies)
        if (Pair.Key.Len() > 32 || Pair.Value < 1 || Pair.Value > 100)
            return false;
    if (!P->SavedBoard.IsEmpty() && P->SavedBoard.Num() != 90)
        return false;
    for (uint8 Square : P->SavedBoard)
        if ((Square & 15) > 7 || (Square >> 4) > 1)
            return false;
    return true;
}
} // namespace

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 两份交替存档中选择代数更新且结构有效的一份。
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (!UGameplayStatics::DoesSaveGameExist(SlotName(Index), 0))
            continue;
        USavePlayerProfile* Loaded = Cast<USavePlayerProfile>(UGameplayStatics::LoadGameFromSlot(SlotName(Index), 0));
        if (ValidProfile(Loaded) && (!Profile || Loaded->Generation > Profile->Generation))
            Profile = Loaded;
    }
    if (!Profile)
        Profile = NewObject<USavePlayerProfile>(this);
    Profile->PieceCounts.SetNumZeroed(8);
    Profile->FileCounts.SetNumZeroed(9);
    bChallengeSelected = Profile->bRunActive;
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
    return Profile ? Profile->BestStreak : 0;
}
int32 UPlayerProfileSubsystem::GetCurrentStreak() const
{
    return Profile ? Profile->CurrentStreak : 0;
}
bool UPlayerProfileSubsystem::HasActiveRun() const
{
    return Profile && Profile->bRunActive;
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
    if (!Profile->bRunActive)
    {
        ClearLearning();
        Profile->bRunActive = true;
        Profile->RunDifficulty = FMath::Min<uint8>(3, Difficulty);
        Save();
    }
    return Profile->RunDifficulty;
}

bool UPlayerProfileSubsystem::RestoreBoard(FChessPosition& OutBoard, EChessColor& OutTurn) const
{
    // 90格只存棋子类型和颜色，不反序列化Actor或旧世界引用。
    if (!Profile->bRunActive || Profile->SavedBoard.Num() != 90)
        return false;
    FChessPosition Restored;
    for (int32 I = 0; I < 90; ++I)
        Restored.Squares[I] = {static_cast<EChessType>(Profile->SavedBoard[I] & 15),
                               static_cast<EChessColor>(Profile->SavedBoard[I] >> 4)};
    // 单方将帅缺失表示终局写入后进程退出，交给GameState补结算。
    if (!FChessPosition::IsInside(Restored.FindKing(EChessColor::REDCHESS)) &&
        !FChessPosition::IsInside(Restored.FindKing(EChessColor::BLACKCHESS)))
        return false;
    OutBoard = Restored;
    OutTurn = static_cast<EChessColor>(Profile->SavedTurn);
    return true;
}

void UPlayerProfileSubsystem::SaveBoard(const FChessPosition& Board, EChessColor Turn)
{
    // 每手保存已提交的逻辑棋盘，动画尚未完成也能继续正确回合。
    if (!Profile->bRunActive)
        return;
    Profile->SavedBoard.SetNum(90);
    for (int32 I = 0; I < 90; ++I)
        Profile->SavedBoard[I] =
            static_cast<uint8>(Board.Squares[I].Type) | (static_cast<uint8>(Board.Squares[I].Color) << 4);
    Profile->SavedTurn = static_cast<uint8>(Turn);
    Save();
}

void UPlayerProfileSubsystem::ObserveMove(const FChessPosition& Before, const FChessMove2P& Move)
{
    // 样本只来自连战中的玩家落子，普通模式和局域网不会污染模型。
    if (!Profile->bRunActive || !Move.IsValid() || Before.Get(Move.from).Color != EChessColor::REDCHESS)
        return;
    ++Profile->ObservedMoves;
    ++Profile->PieceCounts[static_cast<uint8>(Before.Get(Move.from).Type)];
    ++Profile->FileCounts[Move.to.Y];
    if (Before.Get(Move.to).Type != EChessType::EMPTY)
        ++Profile->CaptureMoves;
    if (Move.to.X > Move.from.X)
        ++Profile->ForwardMoves;
    FChessPosition After = Before;
    After.MakeMove(Move);
    if (After.IsInCheck(EChessColor::BLACKCHESS))
        ++Profile->CheckMoves;
    const FString Key = FChessLearning::SampleKey(Before, Move);
    // 保存这次反击，下一次AI搜索完成后用新评分检验上一手的预期。
    Profile->LastPlayerReplyKey = Key;
    if (Profile->PositionMoveCounts.Contains(Key) || Profile->PositionMoveCounts.Num() < 4096)
        ++Profile->PositionMoveCounts.FindOrAdd(Key);
    // 按比例衰减防止计数溢出，并逐渐适应近期风格。
    if (Profile->ObservedMoves >= 100000)
    {
        Profile->ObservedMoves /= 2;
        Profile->CaptureMoves /= 2;
        Profile->ForwardMoves /= 2;
        Profile->CheckMoves /= 2;
        for (int32& Count : Profile->PieceCounts)
            Count /= 2;
        for (int32& Count : Profile->FileCounts)
            Count /= 2;
        for (auto& Pair : Profile->PositionMoveCounts)
            Pair.Value = FMath::Max(1, Pair.Value / 2);
    }
}

void UPlayerProfileSubsystem::ClearLearning()
{
    Profile->CurrentStreak = 0;
    Profile->ObservedMoves = Profile->CaptureMoves = Profile->ForwardMoves = Profile->CheckMoves = 0;
    Profile->PieceCounts.Init(0, 8);
    Profile->FileCounts.Init(0, 9);
    Profile->PositionMoveCounts.Reset();
    Profile->SavedBoard.Reset();
    Profile->SavedTurn = 0;
    Profile->AICaution.Reset();
    Profile->HarmfulReplies.Reset();
    Profile->LastAIMoveKey.Reset();
    Profile->LastPlayerReplyKey.Reset();
}

// 比较相邻AI回合的搜索评分，至少损失一个兵才记录反例，减少估值噪声。
void UPlayerProfileSubsystem::ReviewAIOutcome(int32 CurrentScore)
{
    if (Profile->LastAIMoveKey.IsEmpty() || Profile->LastPlayerReplyKey.IsEmpty())
        return;
    const int32 Drop = Profile->LastExpectedScore - CurrentScore;
    if (Drop >= 70)
    {
        const int32 Severity = FMath::Clamp(Drop / 70, 1, 10);
        // 容量和计数都有上限，长时间挑战不会无限扩张存档。
        if (Profile->AICaution.Contains(Profile->LastAIMoveKey) || Profile->AICaution.Num() < 4096)
        {
            int32& Count = Profile->AICaution.FindOrAdd(Profile->LastAIMoveKey);
            Count = FMath::Min(100, Count + Severity);
        }
        if (Profile->HarmfulReplies.Contains(Profile->LastPlayerReplyKey) || Profile->HarmfulReplies.Num() < 4096)
        {
            int32& Count = Profile->HarmfulReplies.FindOrAdd(Profile->LastPlayerReplyKey);
            Count = FMath::Min(100, Count + Severity);
        }
    }
    Profile->LastAIMoveKey.Reset();
    Profile->LastPlayerReplyKey.Reset();
}

void UPlayerProfileSubsystem::RecordAIChoice(const FChessPosition& Before, const FChessMove2P& Move,
                                             int32 ExpectedScore)
{
    // 留下当前预期，暂停或退出后也能继续评价同一组AI落子/玩家回应。
    Profile->LastAIMoveKey = FChessLearning::SampleKey(Before, Move, EChessColor::BLACKCHESS);
    Profile->LastExpectedScore = ExpectedScore;
    Profile->LastPlayerReplyKey.Reset();
}

void UPlayerProfileSubsystem::CompleteRound(bool bPlayerWon)
{
    if (!Profile->bRunActive)
        return;
    if (bPlayerWon)
    {
        ReviewAIOutcome(-100000); // 终局没有下一次搜索，用败局本身记录最后一次反例。
        Profile->CurrentStreak = FMath::Min(Profile->CurrentStreak + 1, 1000000);
        Profile->BestStreak = FMath::Max(Profile->BestStreak, Profile->CurrentStreak);
        Profile->SavedBoard.Reset();
    }
    else
    {
        Profile->bRunActive = false;
        ClearLearning();
    }
    Save();
    if (!bPlayerWon)
        Save(); // 覆盖备份，防止旧槽恢复已结束的挑战。
}

FChessLearning UPlayerProfileSubsystem::LearningSnapshot() const
{
    // 按值复制后交给搜索线程，后续存档更新不会与后台读数据竞争。
    FChessLearning Result;
    Result.Observations = Profile->ObservedMoves;
    for (int32 I = 0; I < 8; ++I)
        Result.PieceCounts[I] = Profile->PieceCounts[I];
    for (int32 I = 0; I < 9; ++I)
        Result.FileCounts[I] = Profile->FileCounts[I];
    Result.Captures = Profile->CaptureMoves;
    Result.Advances = Profile->ForwardMoves;
    Result.Checks = Profile->CheckMoves;
    Result.PositionMoves = Profile->PositionMoveCounts;
    Result.AICaution = Profile->AICaution;
    Result.HarmfulReplies = Profile->HarmfulReplies;
    return Result;
}

void UPlayerProfileSubsystem::EndChallenge()
{
    Profile->BestStreak = FMath::Max(Profile->BestStreak, Profile->CurrentStreak);
    Profile->bRunActive = bChallengeSelected = false;
    ClearLearning();
    Save();
    Save(); // 两槽均写入结束状态，防止恢复上一份未结束的挑战。
}

bool UPlayerProfileSubsystem::Save()
{
    if (!Profile)
        return false;
    // UE完成单个槽的写入，另一槽保留上一份完整版本。
    ++Profile->Generation;
    const bool bOK = UGameplayStatics::SaveGameToSlot(Profile, SlotName(Profile->Generation % 2), 0);
    SaveError = bOK ? FString() : TEXT("保存失败，请检查本地存储空间和写入权限");
    if (!bOK)
        UE_LOG(LogTemp, Error, TEXT("XQP: %s"), *SaveError);
    return bOK;
}

FString UPlayerProfileSubsystem::SlotName(int32 Index) const
{
    return SlotPrefix + (Index == 0 ? TEXT("_A") : TEXT("_B"));
}
