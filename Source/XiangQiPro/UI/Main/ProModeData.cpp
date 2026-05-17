// Copyright 2026 Ultimate Player All Rights Reserved.


#include "ProModeData.h"

void UProModeData::Init(FString InTitle, UTexture2D* InCover, int InGameModeID)
{
	Title = InTitle;
	Cover = InCover;
	GameModeID = InGameModeID;
}

FString UProModeData::GetModeTitle()
{
	return Title;
}

UTexture2D* UProModeData::GetModeCover()
{
	return Cover;
}

int UProModeData::GetGameModeID()
{
	return GameModeID;
}
