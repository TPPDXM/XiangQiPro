// Copyright 2026 Ultimate Player All Rights Reserved.
#include "GameModeLibrary.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "DataTableAccess.h"

// 每个玩法在数据表中声明标题、封面和适配窗口；通用菜单不依赖任何专用玩法编号。
TArray<FGameModeInfo> UGameModeLibrary::GetGameModeInfos()
{
    auto Modes = XQPDataTables::ReadRows<FGameModeInfo>(PATH_DT_GAME_MODES);
    for (auto& Mode : Modes)
        if (Mode.SetupWidgetClass.IsNull())
            Mode.SetupWidgetClass = FSoftClassPath(PATH_UI_GAMEMODE_SETUP);
    return Modes;
}
