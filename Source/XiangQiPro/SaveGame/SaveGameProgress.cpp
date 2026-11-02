// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SaveGameProgress.h"

USaveGameProgress::USaveGameProgress() :
	EndingGameLevel(0),
	EndingGameLevel_Max(0)
{
}

void USaveGameProgress::MigrateLegacyData()
{
    if (Version != 1) return;
    Progress.CurrentLevel = EndingGameLevel;
    Progress.MaxUnlockedLevel = EndingGameLevel_Max;
    Progress.BestSteps = MoveTemp(UsingSteps);
    Progress.LevelOrder = MoveTemp(EndingLevelOrder);
    Version = 2;
}
