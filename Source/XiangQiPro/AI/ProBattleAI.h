#pragma once

#include "XiangQiPro/AI/ChessPosition.h"
#include "XiangQiPro/GameMode/ProBattleTypes.h"

// 实时决策只访问值类型快照，完整比较有限走法的行程与防守风险。
class FProBattleAI
{
public:
    static FChessMove2P ChooseMove(const FChessPosition& Board, const TArray<FProBattleUnit>& Units,
        EChessColor Side, float Supply, float EnemySupply, float Now, float Remaining,
        const FChessMove2P& LastMove = FChessMove2P(), int32 RandomSeed = 0, float RecoveryRate = 0.8f / 3.f);
};
