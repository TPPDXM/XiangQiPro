// Copyright 2026 Ultimate Player All Rights Reserved.


#include "ProModeLibrary.h"
#include "XiangQiPro/Util/ObjectManager.h"
#include "XiangQiPro/Util/Logger.h"

#define DATATABLE_PATH TEXT("/Script/Engine.DataTable'/Game/DataTable/ProModeInfos.ProModeInfos'")

TArray<FProModeInfo*> UProModeLibrary::GetProModeInfos()
{
    TArray<FProModeInfo*> Infos;
    auto DataTable = OM::GetObject<UDataTable>(DATATABLE_PATH);

    if (!DataTable)
    {
        ULogger::LogError(TEXT("UProModeLibrary::GetProModeInfos"), TEXT("DataTable is nullptr!"));
        return TArray<FProModeInfo*>();
    }
    TArray<FName> rowName = DataTable->GetRowNames();

    for (const FName& name : rowName)
    {
        FString ContextString;
        auto data = DataTable->FindRow<FProModeInfo>(name, ContextString, false);
        if (data)
        {
            Infos.Add(data);
        }
        else
        {
            ULogger::LogError(TEXT("UProModeLibrary::GetProModeInfos"), ContextString);
        }
    }
    return Infos;
}
