// Copyright 2026 Ultimate Player All Rights Reserved.

#include "AI2P.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"

// 固定快照在游戏线程内生成，后台任务只接触自己的值类型数据。
TSharedPtr<FChessSearch, ESPMode::ThreadSafe> UAI2P::CreateSearch(TWeakObjectPtr<UChessBoard2P> Board, EChessColor Color, EAI2PDifficulty Difficulty)
{
    check(IsInGameThread());
    StopThinkingImmediately();
    if (!Board.IsValid() || (Color != EChessColor::REDCHESS && Color != EChessColor::BLACKCHESS)) return nullptr;
    static constexpr int32 Depths[] = {3, 5, 7, 10};
    static constexpr double Seconds[] = {0.15, 0.6, 2.0, 5.0};
    const int32 Index = FMath::Clamp(static_cast<int32>(Difficulty), 0, 3);
    CurrentBoard = Board;
    FChessLearning Learning;
    int32 Depth = Depths[Index];
    double Budget = Seconds[Index];
    const AXQPGameStateBase* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (State && State->bEnableMachineLearning)
    {
        UPlayerProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
        Learning = Profile->LearningSnapshot();
        const int32 Wins = Profile->GetCurrentStreak();
        Depth = FMath::Min(12, Depth + Wins / 2 + FMath::Min(2, Learning.Observations / 100));
        Budget = FMath::Min(8.0, Budget * (1.0 + 0.45 * Wins) + FMath::Min(1.5, Learning.Observations * 0.005));
    }
    ActiveSearch = MakeShared<FChessSearch, ESPMode::ThreadSafe>(Board->CreateSnapshot(), Color, Depth, Budget,
        MoveTemp(Learning), Board->GetCheckHistory());
    return ActiveSearch;
}

// 同步兼容入口也通过独立搜索对象执行。
FChessMove2P UAI2P::GetBestMove(TWeakObjectPtr<UChessBoard2P> Board, EChessColor Color, EAI2PDifficulty Difficulty)
{
    const auto Search = CreateSearch(Board, Color, Difficulty);
    return Search.IsValid() ? Search->FindBestMove().Move : FChessMove2P();
}

// 仅发布原子取消信号；后台任务持有共享引用直到自身退出。
void UAI2P::StopThinkingImmediately()
{
    check(IsInGameThread());
    if (ActiveSearch.IsValid()) ActiveSearch->Cancel();
    ActiveSearch.Reset();
}

// 缓存弱引用，不延长棋盘生命周期。
void UAI2P::SetBoard(TWeakObjectPtr<UChessBoard2P> NewBoard)
{
    check(IsInGameThread());
    CurrentBoard = NewBoard;
}

// 使用与搜索相同的规则判断无合法走法。
bool UAI2P::IsJueSha(EChessColor Color)
{
    check(IsInGameThread());
    if (!CurrentBoard.IsValid()) return false;
    FChessPosition Snapshot = CurrentBoard->CreateSnapshot();
    // 保留远端的无将判负修复；单骑玩法允许没有将帅。
    if (Snapshot.bRequireKing && !FChessPosition::IsInside(Snapshot.FindKing(Color))) return true;
    return Snapshot.GenerateLegalMoves(Color).IsEmpty();
}

// 释放棋盘引用并中止后台搜索。
void UAI2P::Deinitialize()
{
    StopThinkingImmediately();
    CurrentBoard.Reset();
    Super::Deinitialize();
}
