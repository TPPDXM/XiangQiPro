// Copyright 2026 Ultimate Player All Rights Reserved.

#include "EndingLibrary.h"
#include "DataTableAccess.h"

namespace
{
    constexpr const TCHAR* EndingTablePath = TEXT("/Game/DataTable/ChessGenerationInfos.ChessGenerationInfos");
}

// 获取关卡总数，数据表缺失时返回0。
int32 UEndingLibrary::GetEndingGameNum()
{
    const UDataTable* Table = UObjectManager::GetObject<UDataTable>(EndingTablePath);
    return IsValid(Table) ? Table->GetRowNames().Num() : 0;
}

TArray<FName> UEndingLibrary::GetEndingLevelOrder()
{
    const UDataTable* Table = UObjectManager::GetObject<UDataTable>(EndingTablePath);
    return IsValid(Table) ? Table->GetRowNames() : TArray<FName>();
}

// 返回指定关卡的棋子生成数据，越界或空表不再读取负数索引。
TArray<FChessGenerationInfo> UEndingLibrary::GetChessGenerateInfo(int32 Index)
{
    const auto Row = XQPDataTables::ReadRow<FChessGenerationInfos>(EndingTablePath, Index);
    return Row.IsSet() ? Row->Infos : TArray<FChessGenerationInfo>();
}

// 获取指定关卡标题，非法索引返回空文本。
FString UEndingLibrary::GetEndingGameTitle(int32 Index)
{
    const auto Row = XQPDataTables::ReadRow<FChessGenerationInfos>(EndingTablePath, Index);
    return Row.IsSet() ? Row->Title : FString();
}
