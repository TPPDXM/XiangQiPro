// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chesses.h"
#include "Chess_Jiang.generated.h"

#define PATH_MI_CHESSMASK_RAD_JIANG TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Material/Decal/ChessMask/red/MI_ChessMask_rad_shuai.MI_ChessMask_rad_shuai'")
#define PATH_MI_CHESSMASK_BLACK_JIANG TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Material/Decal/ChessMask/black/MI_ChessMask_black_jiang.MI_ChessMask_black_jiang'")

/**
 *
 */
UCLASS()
class XIANGQIPRO_API AChess_Jiang : public AChesses
{
	GENERATED_BODY()

public:
	// 初始化默认属性和组件。
	AChess_Jiang();
	// 初始化将帅名称和阵营材质。
	virtual void Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P) override;
	// 播放棋子被吃效果，并处理派生棋子的结束逻辑。
	virtual void Defeated() override;
	// 生成选中棋子的合法走法并显示提示。
	virtual void GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target) override;

};
