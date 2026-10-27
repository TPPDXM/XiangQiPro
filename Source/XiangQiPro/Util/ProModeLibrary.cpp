// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ProModeLibrary.h"
#include "DataTableAccess.h"

// 不向调用方暴露数据表内部指针，避免资源释放后出现悬空行。
TArray<FProModeInfo> UProModeLibrary::GetProModeInfos()
{
    return XQPDataTables::ReadRows<FProModeInfo>(TEXT("/Game/DataTable/ProModeInfos.ProModeInfos"));
}
