// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_ClassicGameMenu.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "UI_Item_ClassicGame.h"
#include "ClassicGameData.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "UI_LANLobby.h"
#include "XiangQiPro/UI/UIManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"

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
        if (auto* Button = FindButton(TEXT("ClassicLANButton"))) Button->OnClicked.AddUniqueDynamic(this, &UUI_ClassicGameMenu::OpenMobileLAN);
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
void UUI_ClassicGameMenu::OpenMobileLAN() { SetSelection(2); }

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

void UUI_ClassicGameMenu::ShowLANIntroduction()
{
    UCanvasPanel* Frame = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("IntroduceLayout")));
    if (!Frame || !LANIntroductionClass) return;
    // 只实例化已保存的UMG页面，嵌入方式与人机/残局简介相同。
    UUI_LANLobby* Page = XQPMobile::CreateWidget<UUI_LANLobby>(GetGameInstance(), LANIntroductionClass);
    if (!Page) return;
    Frame->ClearChildren();
    UCanvasPanelSlot* ContentSlot = Frame->AddChildToCanvas(Page);
    ContentSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ContentSlot->SetOffsets(FMargin(0));
}

// 选中指定索引的已生成列表项。
void UUI_ClassicGameMenu::SetSelection(int32 Index)
{
    if (auto* Selection = GetWidgetFromName(TEXT("ModeSelectionPage")))
    {
        if (Index < 0 || Index > 2) return;
        SelectedIndex = Index;
        Selection->SetVisibility(ESlateVisibility::Collapsed);
        if (auto* Detail = GetWidgetFromName(TEXT("IntroduceLayout")))
            Detail->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        XQPButtonBinding::SetCaption(Cast<UUserWidget>(GetWidgetFromName(TEXT("WB_Btn_Quit"))), FText::FromString(TEXT("返回模式选择")));
        OnSelectedChange(Index);
        return;
    }
    if (!IsValid(ItemListView) || !DataObjList.IsValidIndex(Index)) return;
    UUI_Item_ClassicGame* Entry = ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(DataObjList[Index]);
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
        if (auto* Entry = ItemListView->GetEntryWidgetFromItem<UUI_Item_ClassicGame>(DataObjList[SelectedIndex]))
            Entry->SetSelected(true);
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

