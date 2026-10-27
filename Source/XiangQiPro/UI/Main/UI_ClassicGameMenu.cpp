// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_ClassicGameMenu.h"
#include "UI_Item_ClassicGame.h"
#include "ClassicGameData.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"

// 构造界面并初始化控件与事件绑定。
void UUI_ClassicGameMenu::NativeConstruct()
{
    Super::NativeConstruct();

}

// 根据名称和图标生成菜单条目。
void UUI_ClassicGameMenu::InitializeList(const TArray<FString>& Items, const TArray<UTexture2D*>& Icons)
{
    if (!ItemListView) return;

    DataObjList.Reset();
    ListItems.Reset();
    ItemListView->ClearListItems();

    for (int32 i = 0; i < Items.Num(); i++)
    {
        UClassicGameData* DataObj = NewObject<UClassicGameData>();
        if (DataObj)
        {
            FOnClassicGameListItemClicked CallBackFunc;
            CallBackFunc.AddDynamic(this, &UUI_ClassicGameMenu::OnListItemClicked);

            DataObj->Init(i, FText::FromString(Items[i]), Icons.IsValidIndex(i) ? Icons[i] : nullptr, CallBackFunc);

            ItemListView->AddItem(DataObj);
            DataObjList.Add(DataObj);
        }
    }
}

// 更新列表选择并通知对应业务。
void UUI_ClassicGameMenu::OnListItemClicked(UUI_Item_ClassicGame* ClickedItem)
{
    if (!ClickedItem) return;

    ClearSelection();

    // 设置新的选择
    SelectedIndex = ClickedItem->GetIndex();
    ClickedItem->SetSelected(true);

    // 触发蓝图事件
    OnSelectedChange(SelectedIndex);
}

// 选中指定索引的已生成列表项。
void UUI_ClassicGameMenu::SetSelection(int32 Index)
{
    if (!IsValid(ItemListView) || !DataObjList.IsValidIndex(Index)) return;
    UUI_Item_ClassicGame* Entry = ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(DataObjList[Index]);
    if (Entry)
    {
        Entry->SetSelected(true);
        OnListItemClicked(Entry);
    }
}

// 清除所有已生成列表项的选中状态。
void UUI_ClassicGameMenu::ClearSelection()
{
    if (!IsValid(ItemListView)) return;
    ListItems.Empty();
    for (UClassicGameData*& Obj : DataObjList)
    {
        ListItems.Add(ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(Obj));
    }

    for (UUI_Item_ClassicGame*& Item : ListItems)
    {
        if (Item)
        {
            Item->SetSelected(false);
        }
    }
    SelectedIndex = -1;
}

