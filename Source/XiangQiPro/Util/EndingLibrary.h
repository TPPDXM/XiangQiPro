// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/ChessMove.h"
#include "XiangQiPro/Chess/AllChessHeader.h"
#include "EndingLibrary.generated.h"


USTRUCT(BlueprintType)
struct FChessGenerationInfo
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AChesses> Class;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChessColor Color;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Pos;

	// 构造空的棋子生成信息。
	FChessGenerationInfo() : Class(nullptr), Color(EChessColor::REDCHESS), Pos(Position(0, 0))
	{ }

	// 根据棋子类、阵营和坐标建立生成信息。
	FChessGenerationInfo(TSubclassOf<AChesses> InClass, EChessColor InColor, Position InPos) : Class(InClass), Color(InColor), Pos(InPos)
	{ }
};

USTRUCT(BlueprintType)
struct FChessGenerationInfos : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FChessGenerationInfo> Infos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Title = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 PerfectStepNum = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 GoodStepNum = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 PassStep = 8;
};

/**
 * 残局库
 */
UCLASS()
class XIANGQIPRO_API UEndingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 获取关卡总数，数据表缺失时返回0。
	UFUNCTION(BlueprintPure)
	static int32 GetEndingGameNum();
	// 返回指定关卡的棋子生成数据，越界或空表不再读取负数索引。
	UFUNCTION(BlueprintPure)
	static TArray<FChessGenerationInfo> GetChessGenerateInfo(int32 Index);
	// 获取指定关卡标题，非法索引返回空文本。
	UFUNCTION(BlueprintPure)
	static FString GetEndingGameTitle(int32 Index);
};
