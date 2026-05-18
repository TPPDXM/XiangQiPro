// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include <vector>
#include <utility>

#include "XiangQiPro/Util/ChessMove.h"
#include "XiangQiPro/Interface/IF_ProMode.h"

#include "XQPGameStateBase.h"
#include "Kismet/KismetMathLibrary.h"

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoloRideMode.generated.h"

class AChesses;
class AXQPGameStateBase;

/**
 * 
 */
UCLASS()
class XIANGQIPRO_API USoloRideMode : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	static AXQPGameStateBase* GS;

	/*
	* 千里走单骑的对手棋子，以及销毁的轮数
	*/
	static std::vector<std::pair<TWeakObjectPtr<AChesses>, int32>> SoloRideEnemies;

public:

	// 千里走单骑的马
	static TWeakObjectPtr<AChesses> SoloRideHorse;

	// 红方马得分
	static int32 SoloRideScore;

	// 敌人生成波次/回合数，用于控制生成概率
	static int32 EnemyGenerateWave;

	// 初始化
	static void InitSoloRideGame(AXQPGameStateBase* InGameState);

	// 应用棋子移动，返回为真时更新得分
	static bool OnApplyMove(FChessMove2P move);

	// 玩家结束移动时调用
	static void OnFinishMove();

	// 生成敌人，默认随机1~2个
	static void GenerateNewEnemies(int32 GenerateCount = UKismetMathLibrary::RandomIntegerInRange(1, 2));

	// 获取可用的生成位置
	static TArray<Position> GetAvailablePos(TWeakObjectPtr<AChesses> Horse);

	// 重置数据，例如得分、轮数
	static void ResetSoloRideData();
	
};
