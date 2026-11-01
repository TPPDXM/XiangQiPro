// Copyright 2026 Ultimate Player All Rights Reserved.
#include "GameModeLibrary.h"
#include "DataTableAccess.h"

// 每个玩法在数据表中声明标题、封面和适配窗口；通用菜单不依赖任何专用玩法编号。
TArray<FGameModeInfo> UGameModeLibrary::GetGameModeInfos()
{
    auto Modes = XQPDataTables::ReadRows<FGameModeInfo>(TEXT("/Game/DataTable/GameModeInfos.GameModeInfos"));
    for (auto& Mode : Modes)
        if (Mode.SetupWidgetClass.IsNull())
            Mode.SetupWidgetClass = FSoftClassPath(TEXT("/Game/UMG/Main/BP_UI_GameModeSetup.BP_UI_GameModeSetup_C"));
    return Modes;
}
