// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once
#include "XiangQiPro/Util/AssetPaths.h"

#include "CoreMinimal.h"
#include "Chesses.h"
#include "Chess_Jv.generated.h"


/**
 * 
 */
UCLASS()
class XIANGQIPRO_API AChess_Jv : public AChesses
{
	GENERATED_BODY()

public:

	AChess_Jv();

	virtual void Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P) override;

	virtual void GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target) override;
	
};
