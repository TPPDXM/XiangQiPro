// Copyright 2026 Ultimate Player All Rights Reserved.


#include "EndingLibrary.h"
#include "XiangQiPro/Util/ObjectManager.h"
#include "XiangQiPro/Util/Logger.h"

#define DATATABLE_ENDING_PATH TEXT("/Script/Engine.DataTable'/Game/DataTable/ChessGenerationInfos.ChessGenerationInfos'")

int32 UEndingLibrary::GetEndingGameNum()
{
    FChessGenerationInfo Infos;
    auto DataTable = OM::GetObject<UDataTable>(DATATABLE_ENDING_PATH);

    TArray<FName> rowName = DataTable->GetRowNames();
    return rowName.Num();
}

TArray<FChessGenerationInfo> UEndingLibrary::GetChessGenerateInfo(int32 Index)
{
    FChessGenerationInfo Infos;
    auto DataTable = OM::GetObject<UDataTable>(DATATABLE_ENDING_PATH);

    TArray<FName> rowName = DataTable->GetRowNames();

    if (Index < 0 || Index >= rowName.Num())
    {
        ULogger::LogError(TEXT("UEndingLibrary::GetChessGenerateInfo"), TEXT("Get generate information index out of bounds!"));
        Index = rowName.Num() - 1;
    }

    FString ContextString;
    auto TableData = DataTable->FindRow<FChessGenerationInfos>(rowName[Index], ContextString, false);
    if (TableData)
    {
        return TableData->Infos;
    }
    else
    {
        ULogger::LogError(TEXT("UEndingLibrary::GetChessGenerateInfo"), "Can't find row by name!");
        return TArray<FChessGenerationInfo>();
    }
}

FString UEndingLibrary::GetEndingGameTitle(int32 Index)
{
    FChessGenerationInfo Infos;
    auto DataTable = OM::GetObject<UDataTable>(DATATABLE_ENDING_PATH);

    TArray<FName> rowName = DataTable->GetRowNames();

    if (Index < 0 || Index >= rowName.Num())
    {
        ULogger::LogError(TEXT("UEndingLibrary::GetChessGenerateInfo"), TEXT("Get generate information index out of bounds!"));
        Index = rowName.Num() - 1;
    }

    FString ContextString;
    auto TableData = DataTable->FindRow<FChessGenerationInfos>(rowName[Index], ContextString, false);
    if (TableData)
    {
        return TableData->Title;
    }
    else
    {
        ULogger::LogError(TEXT("UEndingLibrary::GetChessGenerateInfo"), "Can't find row by name!");
        return FString();
    }
}
