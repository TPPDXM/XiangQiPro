// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_ClassicGameMenu.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "UI_Item_ClassicGame.h"
#include "ClassicGameData.h"
#include "Components/ListView.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI_LANLobby.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/Util/AssetPaths.h"

// 构造界面并初始化控件与事件绑定。
void UUI_ClassicGameMenu::NativeConstruct()
{
    Super::NativeConstruct();
    if (GetWidgetFromName(TEXT("ModeSelectionPage")))
    {
        const auto FindButton = [this](const TCHAR* Name)
        {
            auto* Button = XQPButtonBinding::GetButton(Cast<UUserWidget>(GetWidgetFromName(Name)));
            if (Button) Button->OnClicked.Clear();
            return Button;
        };
        if (auto* Button = FindButton(TEXT("ClassicAIButton"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::OpenMobileAI);
        if (auto* Button = FindButton(TEXT("ClassicEndingButton"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::OpenMobileEnding);
        if (auto* Button = FindButton(TEXT("ClassicReenactmentButton"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::OpenMobileReenactment);
        if (auto* Button = FindButton(TEXT("ClassicLANButton"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::OpenMobileLAN);
        XQPButtonBinding::SetCaption(Cast<UUserWidget>(GetWidgetFromName(TEXT("ClassicReenactmentButton"))),FText::FromString(TEXT("战局演绎")));
        if (auto* Button = FindButton(TEXT("WB_Btn_Quit"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::MobileBackClicked);
        ShowMobileSelection();
    }
}

void UUI_ClassicGameMenu::ShowMobileSelection()
{
    if (auto* Selection = GetWidgetFromName(TEXT("ModeSelectionPage")))
        Selection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (auto* Detail = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("IntroduceLayout"))))
    {
        Detail->ClearChildren();
        Detail->SetVisibility(ESlateVisibility::Collapsed);
    }
    SelectedIndex = -1;
    XQPButtonBinding::SetCaption(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_Quit"))), FText::FromString(TEXT("返回主界面")));
}

bool UUI_ClassicGameMenu::HandleMobileBack()
{
    if (!GetWidgetFromName(TEXT("ModeSelectionPage")) || SelectedIndex < 0) return false;
    ShowMobileSelection();
    return true;
}

void UUI_ClassicGameMenu::MobileBackClicked()
{
    if (!HandleMobileBack()) GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this);
}
void UUI_ClassicGameMenu::OpenMobileAI() { SetSelection(0); }
void UUI_ClassicGameMenu::OpenMobileEnding() { SetSelection(1); }
void UUI_ClassicGameMenu::OpenMobileLAN() { SetSelection(3); }
void UUI_ClassicGameMenu::OpenMobileReenactment() { SetSelection(2); }

// 根据名称和图标生成菜单条目。
void UUI_ClassicGameMenu::InitializeList(const TArray<FString>& Items, const TArray<UTexture2D*>& Icons)
{
    if (!ModeList && !ItemListView) return;

    DataObjList.Reset();
    ListItems.Reset();
    if (ModeList) ModeList->ClearChildren();
    else ItemListView->ClearListItems();

    TArray<FString> Titles = Items;
    TArray<UTexture2D*> Covers = Icons;

    for (int32 i = 0; i < Titles.Num(); i++)
    {
        UClassicGameData* DataObj = NewObject<UClassicGameData>();
        if (DataObj)
        {
            FOnClassicGameListItemClicked CallBackFunc;
            CallBackFunc.AddDynamic(this, &UUI_ClassicGameMenu::OnListItemClicked);

            DataObj->Init(i, FText::FromString(Titles[i]), Covers.IsValidIndex(i) ? Covers[i] : nullptr, CallBackFunc);

            if (ModeList)
            {
                if (auto* Entry = CreateWidget<UUI_Item_ClassicGame>(GetOwningPlayer(),ListItemClass))
                {
                    auto* Row = ModeList->AddChildToVerticalBox(Entry);
                    Row->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                    Row->SetHorizontalAlignment(HAlign_Fill);
                    Row->SetVerticalAlignment(VAlign_Fill);
                    // 条目自身的UMG保留原边距；容器只负责均分高度，不再依赖ListView虚拟化。
                    Entry->NativeOnListItemObjectSet(DataObj);
                    ListItems.Add(Entry);
                }
            }
            else ItemListView->AddItem(DataObj);
            DataObjList.Add(DataObj);
        }
    }
    if (ModeList) SetSelection(DataObjList.IsValidIndex(SelectedIndex) ? SelectedIndex : 0);
}

UUI_Item_ClassicGame* UUI_ClassicGameMenu::FindEntry(int32 Index) const
{
    if (ModeList)
    {
        for (auto* Entry : ListItems) if (Entry && Entry->GetIndex() == Index) return Entry;
        return nullptr;
    }
    return ItemListView && DataObjList.IsValidIndex(Index)
        ? ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(DataObjList[Index]) : nullptr;
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
    if (auto* Selection = GetWidgetFromName(TEXT("ModeSelectionPage")))
    {
        if (Index < 0 || Index > 3) return;
        SelectedIndex = Index;
        Selection->SetVisibility(ESlateVisibility::Collapsed);
        if (auto* Detail = GetWidgetFromName(TEXT("IntroduceLayout")))
            Detail->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        XQPButtonBinding::SetCaption(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_Quit"))), FText::FromString(TEXT("返回模式选择")));
        OnSelectedChange(Index);
        return;
    }
    if (!DataObjList.IsValidIndex(Index)) return;
    UUI_Item_ClassicGame* Entry = FindEntry(Index);
    if (Entry)
    {
        Entry->SetSelected(true);
        OnListItemClicked(Entry);
    }
    else
    {
        // 列表尚未生成对应的Slate条目时，也先保存逻辑选择并切换内容。
        ClearSelection();
        SelectedIndex = Index;
        OnSelectedChange(Index);
    }
}

void UUI_ClassicGameMenu::EnsureInitialSelection()
{
    if (GetWidgetFromName(TEXT("ModeSelectionPage"))) return;
    if (SelectedIndex < 0)
    {
        SetSelection(0);
        return;
    }
    // 后续条目生成或窗口重排只恢复选中样式，不重新跳回默认页面。
    if (DataObjList.IsValidIndex(SelectedIndex))
        if (auto* Entry = FindEntry(SelectedIndex))
            Entry->SetSelected(true);
}

// 清除所有已生成列表项的选中状态。
void UUI_ClassicGameMenu::ClearSelection()
{
    if (!ModeList)
    {
        if (!ItemListView) return;
        ListItems.Reset();
        for (auto* Data : DataObjList) ListItems.Add(ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(Data));
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

