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

	UTexture2D* Cover;

	int GameModeID;

public:

	void Init(FString InTitle, UTexture2D* InCover, int InGameModeID);

	FString GetModeTitle();

	UTexture2D* GetModeCover();

	int GetGameModeID();
	
};
