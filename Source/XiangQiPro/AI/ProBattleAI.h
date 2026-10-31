#pragma once

#include "XiangQiPro/AI/ChessPosition.h"
#include "XiangQiPro/GameMode/ProBattleTypes.h"

struct FProAIProfile
{
    float ReactionSeconds;
    float ThinkInterval;
    int32 Candidates;
    int32 Continuations;
    float Noise;
};

// 搜索仅访问值类型快照；按到达事件推演实时吃子、逃离和粮草恢复。
class FProBattleAI
{
public:
    static FProAIProfile Profile(EProAIDifficulty Difficulty);
    static FChessMove2P ChooseMove(const FChessPosition& Board, const TArray<FProBattleUnit>& Units,
        EChessColor Side, float Supply, float EnemySupply, float Now, float Remaining,
        const FChessMove2P& LastMove = FChessMove2P(), int32 RandomSeed = 0, float RecoveryRate = 0.8f / 3.f,
        EProAIDifficulty Difficulty = EProAIDifficulty::Hard, float MaxSupply = 10.f,
        const TArray<FChessMove2P>* RecentMoves = nullptr);
};
