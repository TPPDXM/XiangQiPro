// Copyright 2026 Ultimate Player All Rights Reserved.

#include "EndingLibrary.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "DataTableAccess.h"


// 获取关卡总数，数据表缺失时返回0。
int32 UEndingLibrary::GetEndingGameNum()
{
    const UDataTable* Table = UObjectManager::GetObject<UDataTable>(PATH_DT_ENDINGS);
    return IsValid(Table) ? Table->GetRowNames().Num() : 0;
}

TArray<FName> UEndingLibrary::GetEndingLevelOrder()
{
    const UDataTable* Table = UObjectManager::GetObject<UDataTable>(PATH_DT_ENDINGS);
    return IsValid(Table) ? Table->GetRowNames() : TArray<FName>();
}

// 难度跟随关卡行保存，重新排序不会让高难残局退回普通AI。
uint8 UEndingLibrary::GetDefenceDifficulty(int32 Index)
{
    const auto Row = XQPDataTables::ReadRow<FChessGenerationInfos>(PATH_DT_ENDINGS, Index);
    return Row.IsSet() ? FMath::Min<uint8>(4, Row->DefenceDifficulty) : 1;
}

// 返回指定关卡的棋子生成数据，越界或空表不再读取负数索引。
TArray<FChessGenerationInfo> UEndingLibrary::GetChessGenerateInfo(int32 Index)
{
    const auto Row = XQPDataTables::ReadRow<FChessGenerationInfos>(PATH_DT_ENDINGS, Index);
    return Row.IsSet() ? Row->Infos : TArray<FChessGenerationInfo>();
}

// 获取指定关卡标题，非法索引返回空文本。
FString UEndingLibrary::GetEndingGameTitle(int32 Index)
{
    const auto Row = XQPDataTables::ReadRow<FChessGenerationInfos>(PATH_DT_ENDINGS, Index);
    return Row.IsSet() ? Row->Title : FString();
}
