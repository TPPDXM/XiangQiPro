// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Introduce_Ending.h"
#include "Components/ListView.h"
#include "EndingGameData.h"
#include "XiangQiPro/SaveGame/SaveGameLibrary.h"
#include "XiangQiPro/Util/EndingLibrary.h"

// 构造界面并初始化控件与事件绑定。
void UUI_Introduce_Ending::NativeConstruct()
{
    Super::NativeConstruct();

    if (!IsValid(EndingGameList)) return;
    DataObjList.Reset();
    ListItems.Reset();
    EndingGameList->ClearListItems();

    int32 MaxIndex = FMath::Min(USaveGameLibrary::GetEndingGameLevel_Max(), UEndingLibrary::GetEndingGameNum() - 1);
    UserSelectedIndex = USaveGameLibrary::GetEndingGameLevel();
    for (int32 i = 0; i <= MaxIndex; i++)
    {
        UEndingGameData* DataObj = NewObject<UEndingGameData>(this);
        if (DataObj)
        {
            FOnEndingGameListItemClicked CallBackFunc;
            CallBackFunc.AddDynamic(this, &UUI_Introduce_Ending::OnListItemClicked);

            DataObj->Init(i, this, CallBackFunc);

            EndingGameList->AddItem(DataObj);
            DataObjList.Add(DataObj);
        }
    }
}

// 更新列表选择并通知对应业务。
void UUI_Introduce_Ending::OnListItemClicked(UUI_Item_EndingGame* ClickedItem)
{
    if (!ClickedItem) return;

    ClearSelection();

    // 设置新的选择
    UserSelectedIndex = ClickedItem->Index;
    USaveGameLibrary::SetEndingGameLevel(UserSelectedIndex); // 更新要玩的关卡
    ClickedItem->SetSelected(true);
}

// 选中指定索引的已生成列表项。
void UUI_Introduce_Ending::SetSelection(int32 Index)
{
    if (!IsValid(EndingGameList) || !DataObjList.IsValidIndex(Index)) return;
    UUI_Item_EndingGame* Entry = EndingGameList->GetEntryWidgetFromItem<UUI_Item_EndingGame>(DataObjList[Index]);
    if (Entry)
    {
        Entry->SetSelected(true);
        OnListItemClicked(Entry);
    }
}

// 清除所有已生成列表项的选中状态。
void UUI_Introduce_Ending::ClearSelection()
{
    if (!IsValid(EndingGameList)) return;
    ListItems.Empty();
    for (UEndingGameData*& Obj : DataObjList)
    {
        ListItems.Add(EndingGameList->GetEntryWidgetFromItem<UUI_Item_EndingGame>(Obj));
    }

    for (UUI_Item_EndingGame*& Item : ListItems)
    {
        if (Item)
        {
            Item->SetSelected(false);
        }
    }
    UserSelectedIndex = 0;
}

