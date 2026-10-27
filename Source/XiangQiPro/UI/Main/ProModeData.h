// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProModeData.generated.h"

/**
 *
 */
UCLASS()
class XIANGQIPRO_API UProModeData : public UObject
{
	GENERATED_BODY()

private:

	FString Title;

	UPROPERTY()
	TObjectPtr<UTexture2D> Cover = nullptr;

	int GameModeID;

public:
	// 保存模式标题、封面与模式编号。
	void Init(FString InTitle, UTexture2D* InCover, int InGameModeID);
	// 获取模式标题。
	FString GetModeTitle();
	// 获取模式封面资源。
	UTexture2D* GetModeCover();
	// 获取模式编号。
	int GetGameModeID();

};
