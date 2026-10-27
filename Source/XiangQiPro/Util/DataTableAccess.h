// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ObjectManager.h"

namespace XQPDataTables
{
    // 读取指定行的值副本，避免把数据表内部地址交给长期持有者。
    template<typename RowType>
    TOptional<RowType> ReadRow(const TCHAR* Path, int32 Index)
    {
        UDataTable* Table = UObjectManager::GetObject<UDataTable>(Path);
        if (!IsValid(Table)) return {};
        const TArray<FName> Names = Table->GetRowNames();
        if (!Names.IsValidIndex(Index)) return {};
        const RowType* Row = Table->FindRow<RowType>(Names[Index], TEXT("ReadRow"), false);
        return Row ? TOptional<RowType>(*Row) : TOptional<RowType>();
    }

    // 获取数据表有效行的值副本，资源不存在时返回空数组。
    template<typename RowType>
    TArray<RowType> ReadRows(const TCHAR* Path)
    {
        TArray<RowType> Result;
        UDataTable* Table = UObjectManager::GetObject<UDataTable>(Path);
        if (!IsValid(Table)) return Result;
        for (const FName Name : Table->GetRowNames())
            if (const RowType* Row = Table->FindRow<RowType>(Name, TEXT("ReadRows"), false)) Result.Add(*Row);
        return Result;
    }
}
