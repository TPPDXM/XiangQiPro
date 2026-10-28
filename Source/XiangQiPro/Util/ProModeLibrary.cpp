// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ProModeLibrary.h"
#include "DataTableAccess.h"

// 不向调用方暴露数据表内部指针，避免资源释放后出现悬空行。
TArray<FProModeInfo> UProModeLibrary::GetProModeInfos()
{
    auto Modes = XQPDataTables::ReadRows<FProModeInfo>(TEXT("/Game/DataTable/ProModeInfos.ProModeInfos"));
    if (!Modes.ContainsByPredicate([](const FProModeInfo& Info) { return Info.GameModeID == 6; }))
    {
        FProModeInfo Info;
        Info.Title = TEXT("烽火弈");
        Info.GameModeID = 6;
        if (!Modes.IsEmpty()) Info.Covers = Modes[0].Covers;
        Modes.Insert(Info, 0);
    }
    return Modes;
}
