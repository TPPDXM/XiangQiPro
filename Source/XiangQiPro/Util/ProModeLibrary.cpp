// Copyright 2026 Ultimate Player All Rights Reserved.


#include "ProModeLibrary.h"
#include "XiangQiPro/Util/ObjectManager.h"
#include "XiangQiPro/Util/Logger.h"

#define DATATABLE_PATH TEXT("/Script/Engine.DataTable'/Game/DataTable/ProModeInfos.ProModeInfos'")

auto UProModeLibrary::GetProModeInfos() -> TArray<FProModeInfo*>
{
    TArray<FProModeInfo*> Infos;
    auto DataTable = OM::GetObject<UDataTable>(DATATABLE_PATH);

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
