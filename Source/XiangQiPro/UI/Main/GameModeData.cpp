// Copyright 2026 Ultimate Player All Rights Reserved.


#include "GameModeData.h"

void UGameModeData::Init(FString InTitle, UTexture2D* InCover, int InGameModeID, TSubclassOf<UUI_GameModeSetup> InSetupClass)
{
    SetupClass = InSetupClass;
	Title = InTitle;
	Cover = InCover;
	GameModeID = InGameModeID;
}

FString UGameModeData::GetModeTitle()
{
	return Title;
}

UTexture2D* UGameModeData::GetModeCover()
{
	return Cover;
}

int UGameModeData::GetGameModeID()
{
	return GameModeID;
}
