#include "UI_Reenactment.h"
#include "XiangQiPro/SaveGame/ReenactmentLibrary.h"
#include "XiangQiPro/GameMode/ReenactmentComponent.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/Camera/Camera2PActor.h"
#include "EngineUtils.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "XiangQiPro/UI/Components/UI_ToggleSwitch.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanelSlot.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

namespace
{
    // 列表项双击判定窗口：同一条目在此时长内再次点击即视为双击直接开局。
    constexpr double EntryDoubleClickSeconds = .35;

    // 页面可直接放置UButton，也可复用WB_Btn/WB_MenuIconButton。
    // 显隐作用于外层完整组件，事件绑定才下钻到内部点击层。
    UWidget* ActionRoot(UUserWidget* Widget, const TCHAR* Name)
    {
        UWidget* Found = Widget->GetWidgetFromName(*(FString(TEXT("Btn_"))+Name));
        if (!Found) Found = Widget->GetWidgetFromName(Name);
        return Found;
    }
    UButton* Action(UUserWidget* Widget, const TCHAR* Name)
    {
        UWidget* Found = ActionRoot(Widget, Name);
        if (auto* Button = Cast<UButton>(Found)) return Button;
        return XQPButtonBinding::GetButton(Cast<UUserWidget>(Found));
    }
    UReenactmentComponent* Editor(const UUserWidget* Widget)
    {
        const auto* State = Widget->GetWorld()->GetGameState<AXQPGameStateBase>();
        return State ? State->Reenactment.Get() : nullptr;
    }
    void Label(UUserWidget* Widget, const TCHAR* Name, const FString& Value)
    {
        if (auto* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(Name))) Text->SetText(FText::FromString(Value));
    }
    UComboBoxString* DifficultyCombo(UUserWidget* Widget)
    {
        if (auto* Host = Cast<UUI_XQP_ComboBox>(Widget->GetWidgetFromName(TEXT("DifficultyHost")))) return Host->ComboBox;
        return Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("Difficulty")));
    }
}

void UUI_ReenactmentEntry::InitEntry(UUI_Reenactment* InOwner, const FString& InId, const FString& Name, const FString& Author)
{
    Page = InOwner; Id = InId;
    Label(this,TEXT("Title"),Name);
    Label(this,TEXT("Author"),TEXT("作者：")+Author);
    if (auto* Button = Action(this,TEXT("Select"))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Choose);
    if (auto* Button = Action(this,TEXT("Delete"))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::RequestRemoval);
}

void UUI_ReenactmentEntry::Choose() { if (Page.IsValid()) Page->HandleEntryClick(Id); }
void UUI_ReenactmentEntry::RequestRemoval() { if (Page.IsValid()) Page->RequestRemoveEntry(Id); }
void UUI_ReenactmentEntry::SetSelected(bool bSelected)
{
    // 删除按钮与选中层是UMG中的独立兄弟节点，点击删除不会触发整张卡片的选择按钮。
    if (auto* DeleteSlot = GetWidgetFromName(TEXT("DeleteSlot")))
        DeleteSlot->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (auto* Button = Action(this,TEXT("Select"))) Button->SetStyle(bSelected ? SelectedStyle : UnselectedStyle);
    for (const TCHAR* Name : {TEXT("Title"),TEXT("Author")})
        if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(Name)))
            Text->SetColorAndOpacity(bSelected ? SelectedTextColor : UnselectedTextColor);
}

void UUI_Reenactment::NativeConstruct()
{
    Super::NativeConstruct();
    // 控件名称是资产与行为的明确契约；所有位置、字号、按钮样式都留在UMG中。
#define BIND_ACTION(Name, Method) if (auto* Button = Action(this,TEXT(Name))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Method)
    BIND_ACTION("NewGame",NewGame); BIND_ACTION("EditGame",EditGame); BIND_ACTION("StartGame",StartGame);
    BIND_ACTION("Save",SaveGame); BIND_ACTION("OpenSave",OpenSave); BIND_ACTION("Export",ExportGame);
    BIND_ACTION("OpenImport",OpenImport); BIND_ACTION("Import",ImportGame); BIND_ACTION("Paste",PasteImport);
    BIND_ACTION("Back",ClosePage); BIND_ACTION("MainMenu",MainMenu); BIND_ACTION("Settings",OpenSettings);
    BIND_ACTION("Menu",OpenPause); BIND_ACTION("Clear",ClearBoard); BIND_ACTION("Standard",StandardBoard);
    BIND_ACTION("SwitchColor",SwitchColor); BIND_ACTION("Remove",RemovePiece);
    BIND_ACTION("Piece1",SelectKing); BIND_ACTION("Piece2",SelectAdvisor); BIND_ACTION("Piece3",SelectElephant);
    BIND_ACTION("Piece4",SelectHorse); BIND_ACTION("Piece5",SelectChariot); BIND_ACTION("Piece6",SelectCannon); BIND_ACTION("Piece7",SelectPawn);
#undef BIND_ACTION
    const auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    if (Screen == 0) RefreshLibrary();
    if (Screen == 1)
    {
        if (auto* Switch = Cast<UUI_ToggleSwitch>(GetWidgetFromName(TEXT("AISwitch"))))
        {
            // 与人机对弈共用开关；初始化同步不会广播用户事件或把已保存棋局标脏。
            Switch->OnChanged.RemoveDynamic(this,&ThisClass::AIChanged);
            Switch->SetIsChecked(Library->GetDraft().bAI);
            Switch->OnChanged.AddUniqueDynamic(this,&ThisClass::AIChanged);
        }
        if (auto* Combo = DifficultyCombo(this))
        {
            // 页面从保存/设置弹窗恢复时也会再次Construct；先解绑再重建选项，
            // 否则ClearOptions的Direct通知会把原难度误改为简单，并标记为未保存。
            Combo->OnSelectionChanged.RemoveDynamic(this,&ThisClass::DifficultyChanged);
            Combo->ClearOptions();
            for (const TCHAR* Name : {TEXT("简单"),TEXT("普通"),TEXT("困难"),TEXT("大师"),TEXT("棋圣")}) Combo->AddOption(Name);
            Combo->SetSelectedIndex(Library->GetDraft().Difficulty);
            Combo->OnSelectionChanged.AddUniqueDynamic(this,&ThisClass::DifficultyChanged);
        }
        RefreshEditor(); UpdateCanTick();
    }
    if (Screen == 2)
    {
        // 三种入口共用一个表单，但说明与完成行为必须和本次入口一致。
        switch (SaveCompletion)
        {
        case ESaveCompletion::StartBattle: SetStatus(TEXT("保存棋局后开始对弈，红方先手。")); break;
        case ESaveCompletion::ReturnToMainMenu: SetStatus(TEXT("保存当前棋局后返回主界面，未完成的布阵也可以保存。")); break;
        default: SetStatus(TEXT("保存当前布阵并继续编辑，未完成的布阵也可以保存。")); break;
        }
        if (auto* Name = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("GameName")))) Name->SetText(FText::FromString(Library->GetDraft().Name));
        if (auto* Author = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("AuthorName"))))
        {
            const FXQPArrangement& Draft = Library->GetDraft();
            // 作者只在“尚未保存过”的新棋局中可改；保存后草稿已有ID，再次打开即锁定。
            const bool bFirstSave = Draft.Id.IsEmpty();
            Author->SetText(FText::FromString(Draft.Author));
            Author->SetIsEnabled(bFirstSave);

            if (bFirstSave)
            if (auto* Caption = Cast<UTextBlock>(GetWidgetFromName(TEXT("AuthorNameCaption"))))
            {
                // 作者保存后不可更改，在标题处给出提示；Contains 防止页面重复构造时反复拼接。
                const FString Hint = TEXT("(保存后不可更改)");
                const FString Base = Caption->GetText().ToString();
                if (!Base.Contains(Hint)) Caption->SetText(FText::FromString(Base + Hint));
            }
        }
    }
}

void UUI_Reenactment::SetStatus(const FString& Message) { Label(this,TEXT("Status"),Message); }

void UUI_Reenactment::RefreshLibrary()
{
    auto* Grid = Cast<UUniformGridPanel>(GetWidgetFromName(TEXT("GamesGrid")));
    if (!Grid) return;
    // 滚动容器和网格间距均保存在UMG中。只移除动态条目，保留桌面资产中的
    // 第二列占位：只有一个存档时卡片仍占半行，不会因UniformGrid自动收列而拉满。
    for (int32 Index = Grid->GetChildrenCount() - 1; Index >= 0; --Index)
        if (Cast<UUI_ReenactmentEntry>(Grid->GetChildAt(Index))) Grid->RemoveChildAt(Index);
    const int32 Columns = XQPMobile::IsPortrait(this) ? 1 : 2;
    const auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    auto* Class = LoadClass<UUI_ReenactmentEntry>(nullptr,PATH_UI_REENACTMENT_ENTRY);
    int32 EntryIndex = 0;
    if (Class) for (const auto& Game : Library->GetGames())
        if (auto* Entry = XQPMobile::CreateWidget<UUI_ReenactmentEntry>(GetOwningPlayer(),Class))
        {
            auto* Cell = Grid->AddChildToUniformGrid(Entry, EntryIndex / Columns, EntryIndex % Columns);
            Cell->SetHorizontalAlignment(HAlign_Fill);
            Cell->SetVerticalAlignment(VAlign_Fill);
            ++EntryIndex;
            Entry->InitEntry(this,Game.Id,Game.Name,Game.Author);
            Entry->SetSelected(Game.Id == SelectedId);
        }
    SetStatus(Library->GetGames().IsEmpty() ? TEXT("暂无自定义棋局，点击“新建棋局”开始布阵。") : TEXT("选择棋局直接对弈，或编辑后再次保存。"));
    // PC端使用WB_Btn复合组件，显示/禁用必须作用于整个组件，而不只操作内部点击层。
    if (auto* Edit = ActionRoot(this,TEXT("EditGame"))) Edit->SetVisibility(SelectedId.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (auto* Start = ActionRoot(this,TEXT("StartGame"))) Start->SetIsEnabled(!SelectedId.IsEmpty());
}

void UUI_Reenactment::SelectEntry(const FString& Id) { SelectedId = Id; RefreshLibrary(); }

void UUI_Reenactment::HandleEntryClick(const FString& Id)
{
    const double Now = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
    // 双击同一条目：直接选中并进入对弈，等价于“先点条目再点开始对弈”。
    // 首次点击会重建列表，因此双击状态放在页面而不是条目实例上，避免重建后计时丢失。
    if (!Id.IsEmpty() && Id == LastEntryClickId && Now - LastEntryClickTime <= EntryDoubleClickSeconds)
    {
        LastEntryClickId.Reset();
        LastEntryClickTime = -1.0;
        SelectEntry(Id);
        StartGame();
        return;
    }
    // 单击或切换条目：仅更新选中状态，并记录本次点击供下一次双击判定使用。
    LastEntryClickId = Id;
    LastEntryClickTime = Now;
    SelectEntry(Id);
}

void UUI_Reenactment::RequestRemoveEntry(const FString& Id)
{
    // 只接受当前选中项的删除请求，避免过时条目或重复点击删除了别的棋局。
    if (Id.IsEmpty() || Id != SelectedId || (RemoveDialog.IsValid() && RemoveDialog->IsInViewport())) return;
    const auto& Games = GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->GetGames();
    const auto* Game = Games.FindByPredicate([&Id](const FXQPArrangement& Item) { return Item.Id == Id; });
    if (!Game) { SelectedId.Reset(); RefreshLibrary(); return; }
    auto* Class = LoadClass<UUI_CommonDialog>(nullptr,PATH_UI_COMMON_DIALOG);
    auto* Dialog = Class ? XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(),Class) : nullptr;
    if (!Dialog) return;
    PendingRemoveId = Id;
    RemoveDialog = Dialog;
    Dialog->Configure(FText::FromString(TEXT("移除棋局")),
        FText::FromString(FString::Printf(TEXT("确认移除棋局“%s”？"),*Game->Name)),
        FText::FromString(TEXT("移除")),FText::FromString(TEXT("取消")));
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(this,&ThisClass::ConfirmRemoveEntry);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
}

void UUI_Reenactment::ConfirmRemoveEntry()
{
    FString Error;
    if (!GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->RemoveGame(PendingRemoveId,Error))
    {
        if (RemoveDialog.IsValid()) RemoveDialog->SetMessage(FText::FromString(Error));
        return;
    }
    if (SelectedId == PendingRemoveId) SelectedId.Reset();
    PendingRemoveId.Reset();
    RefreshLibrary();
    if (RemoveDialog.IsValid()) RemoveDialog->Dismiss();
    RemoveDialog.Reset();
}

void UUI_Reenactment::OpenPage(const TCHAR* Path)
{
    if (UClass* Class = LoadClass<UUI_Reenactment>(nullptr,Path))
        if (auto* Page = XQPMobile::CreateWidget<UUI_Reenactment>(GetOwningPlayer(),Class))
            GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Page);
}

void UUI_Reenactment::TravelToBattle()
{
    if (bOpeningBattle) return;
    bOpeningBattle = true;
    if (auto* Edit = Editor(this)) Edit->HidePreview();
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Reenactment);
    // 统一转场同时提供保存后的渐隐/渐显，并在新世界从已保存布阵重建规则状态。
    UXQPGameInstance::OpenGameLevel(this,PATH_MAP_BATTLE2P);
}

void UUI_Reenactment::NewGame() { GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->NewGame(); TravelToBattle(); }
void UUI_Reenactment::EditGame()
{
    FString Error;
    if (GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->SelectGame(SelectedId,true,Error)) TravelToBattle();
    else SetStatus(Error);
}

void UUI_Reenactment::StartGame()
{
    auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    FString Error;
    if (Screen == 0)
    {
        if (Library->SelectGame(SelectedId,false,Error)) TravelToBattle(); else SetStatus(Error);
        return;
    }
    if (Library->IsDirty()) { OpenSaveWithCompletion(ESaveCompletion::StartBattle); return; }
    if (!Library->GetDraft().Validate(Error,true)) { SetStatus(Error); return; }
    if (Library->BeginBattle(Error)) TravelToBattle(); else SetStatus(Error);
}

void UUI_Reenactment::OpenSave() { OpenSaveWithCompletion(ESaveCompletion::KeepEditing); }

void UUI_Reenactment::OpenSaveWithCompletion(ESaveCompletion Completion)
{
    auto* Class = LoadClass<UUI_Reenactment>(nullptr,PATH_UI_REENACTMENT_SAVE);
    auto* Page = Class ? XQPMobile::CreateWidget<UUI_Reenactment>(GetOwningPlayer(),Class) : nullptr;
    if (!Page) return;
    if (auto* Edit = Editor(this)) Edit->HidePreview();
    // 必须在AddUI触发NativeConstruct前传入，避免表单先显示错误说明或复用旧入口状态。
    Page->SaveCompletion = Completion;
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Page);
}

void UUI_Reenactment::SaveGame()
{
    auto* Name = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("GameName")));
    auto* Author = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("AuthorName")));
    if (!Name || !Author) return;
    FString Error;
    auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    // 已保存过的棋局作者不可修改，保存时以草稿中的作者为准，忽略被禁用的输入框文本。
    const bool bFirstSave = Library->GetDraft().Id.IsEmpty();
    const FString AuthorName = bFirstSave ? Author->GetText().ToString() : Library->GetDraft().Author;
    if (!Library->SaveDraft(Name->GetText().ToString(),AuthorName,Error)) { SetStatus(Error); return; }
    if (SaveCompletion == ESaveCompletion::ReturnToMainMenu) { TravelToMainMenu(); return; }
    if (SaveCompletion == ESaveCompletion::KeepEditing)
    {
        GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this);
        return;
    }
    if (Library->BeginBattle(Error)) TravelToBattle();
    else SetStatus(TEXT("棋局已保存。")+Error);
}

void UUI_Reenactment::ExportGame()
{
    auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    if (auto* Name = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("GameName"))))
        if (auto* Author = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("AuthorName"))))
        {
            FString Error;
            if (!Library->SaveDraft(Name->GetText().ToString(),Author->GetText().ToString(),Error)) { SetStatus(Error); return; }
        }
    FString Error;
    const FString Json = Library->ExportDraft(Error);
    if (Json.IsEmpty()) { SetStatus(Error); return; }
    // 文件使用GUID命名，作者或棋局名称不能形成路径穿越；移动端同时复制分享数据。
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Reenactments"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Path = FPaths::Combine(Directory,Library->GetDraft().Id+TEXT(".xqp.json"));
    const bool bSaved = FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformApplicationMisc::ClipboardCopy(*Json);
    SetStatus(bSaved ? TEXT("棋局已导出，分享数据已复制。文件：")+FPaths::ConvertRelativePathToFull(Path) : TEXT("文件导出失败，分享数据已复制，可粘贴发送。"));
}

void UUI_Reenactment::OpenImport() { OpenPage(PATH_UI_REENACTMENT_IMPORT); }
void UUI_Reenactment::PasteImport()
{
    FString Text; FPlatformApplicationMisc::ClipboardPaste(Text);
    if (auto* Input = Cast<UMultiLineEditableTextBox>(GetWidgetFromName(TEXT("ImportText")))) Input->SetText(FText::FromString(Text.Left(32768)));
}
void UUI_Reenactment::ImportGame()
{
    auto* Input = Cast<UMultiLineEditableTextBox>(GetWidgetFromName(TEXT("ImportText")));
    if (!Input) return;
    FString Text = Input->GetText().ToString().TrimStartAndEnd(), Error;
    // PC可填写导出文件路径，移动端可直接粘贴分享数据；大小上限在读入前检查。
    if (!Text.StartsWith(TEXT("{")) && IFileManager::Get().FileSize(*Text) > 0 && IFileManager::Get().FileSize(*Text) <= 32768)
        FFileHelper::LoadFileToString(Text,*FString(Text));
    if (!GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->ImportGame(Text,Error)) { SetStatus(Error); return; }
    auto* Manager = GetGameInstance()->GetSubsystem<UUIManager>();
    Manager->RemoveUI(this);
    // 刷新主菜单内已有列表，不额外叠加一份相同页面，返回键仍只退一层。
    TArray<UUserWidget*> Pages;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Pages,StaticClass(),false);
    const FString ImportedId = GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->GetDraft().Id;
    for (auto* Page : Pages) if (auto* LibraryPage = Cast<UUI_Reenactment>(Page); LibraryPage && LibraryPage->Screen == 0)
        LibraryPage->SelectEntry(ImportedId);
}

void UUI_Reenactment::ClosePage() { GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this); }
void UUI_Reenactment::MainMenu()
{
    const auto* Library = GetGameInstance()->GetSubsystem<UReenactmentLibrary>();
    if (!Library->IsEditing() || !Library->IsDirty()) { TravelToMainMenu(); return; }
    auto* Class = LoadClass<UUI_CommonDialog>(nullptr,PATH_UI_COMMON_DIALOG);
    auto* Dialog = Class ? XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(),Class) : nullptr;
    if (!Dialog) return;
    // 复用已有确认弹窗。打开保存页前关闭提示，不保存分支不改写原有存档。
    Dialog->Configure(FText::FromString(TEXT("保存棋局")),FText::FromString(TEXT("当前棋局尚未保存，是否保存后返回主界面？")),
        FText::FromString(TEXT("保存")),FText::FromString(TEXT("不保存")));
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(this,&ThisClass::SaveBeforeMainMenu);
    Dialog->OnSecondaryAction = FSimpleDelegate::CreateUObject(this,&ThisClass::TravelToMainMenu);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog);
}

void UUI_Reenactment::SaveBeforeMainMenu()
{
    GetGameInstance()->GetSubsystem<UUIManager>()->CloseDialogs();
    OpenSaveWithCompletion(ESaveCompletion::ReturnToMainMenu);
}

void UUI_Reenactment::TravelToMainMenu()
{
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Default);
    UXQPGameInstance::OpenGameLevel(this,PATH_MAP_MAIN);
}
void UUI_Reenactment::OpenSettings()
{
    if (auto* Class = LoadClass<UUserWidget>(nullptr,PATH_UI_SETTINGS_MENU))
        GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(XQPMobile::CreateWidget<UUserWidget>(GetOwningPlayer(),Class));
}
void UUI_Reenactment::OpenPause() { GetGameInstance()->GetSubsystem<UUIManager>()->FinishUI(); }
void UUI_Reenactment::ClearBoard()
{
    auto* Class = LoadClass<UUI_CommonDialog>(nullptr,PATH_UI_COMMON_DIALOG);
    auto* Dialog = Class ? XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(),Class) : nullptr;
    if (!Dialog) return;
    Dialog->Configure(FText::FromString(TEXT("清空棋盘")),FText::FromString(TEXT("移除所有棋子并返还库存？已保存的棋局仍会保留。")),FText::FromString(TEXT("清空")),FText::FromString(TEXT("取消")));
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(this,&ThisClass::ConfirmClear);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog);
}
void UUI_Reenactment::ConfirmClear() { if (auto* Edit = Editor(this)) Edit->ClearBoard(); GetGameInstance()->GetSubsystem<UUIManager>()->CloseDialogs(); }
void UUI_Reenactment::StandardBoard() { if (auto* Edit = Editor(this)) Edit->ClearBoard(true); }
void UUI_Reenactment::SwitchColor() { if (auto* Edit = Editor(this)) Edit->ToggleColor(); }
void UUI_Reenactment::RemovePiece() { if (auto* Edit = Editor(this)) Edit->RemoveSelected(); }
void UUI_Reenactment::AIChanged(bool bChecked) { GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->EditDraft().bAI = bChecked; RefreshEditor(); }
void UUI_Reenactment::DifficultyChanged(FString Item,ESelectInfo::Type Selection)
{
    if (Selection == ESelectInfo::Direct || Item.IsEmpty()) return;
    if (auto* Combo = DifficultyCombo(this))
        GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->EditDraft().Difficulty = FMath::Clamp(Combo->FindOptionIndex(Item),0,4);
}

void UUI_Reenactment::RefreshEditor()
{
    auto* Edit = Editor(this);
    if (!Edit) return;
    if (!XQPMobile::IsPortrait(this))
    {
        if (!EditorCamera.IsValid())
        {
            TActorIterator<ACamera2PActor> CameraIt(GetWorld());
            if (CameraIt) EditorCamera = *CameraIt;
        }
        if (EditorCamera.IsValid())
        {
            // 桌面同样为物品栏及两侧工具预留空间，首次构图后保留用户主动旋转/缩放的选择。
            const FVector2D Viewport = XQPMobile::ViewportSize(this);
            const float DPI = UWidgetLayoutLibrary::GetViewportScale(this);
            EditorCamera->FitProBoardToViewport(Viewport,FBox2D(FVector2D(140,90)*DPI,Viewport-FVector2D(280,175)*DPI));
        }
    }
    const bool Red = Edit->Color == EChessColor::REDCHESS;
    const TCHAR* RedNames[] = {TEXT(""),TEXT("帅"),TEXT("仕"),TEXT("相"),TEXT("马"),TEXT("车"),TEXT("炮"),TEXT("兵")};
    const TCHAR* BlackNames[] = {TEXT(""),TEXT("将"),TEXT("士"),TEXT("象"),TEXT("马"),TEXT("车"),TEXT("炮"),TEXT("卒")};
    for (int32 Type = 1; Type <= 7; ++Type)
    {
        const FString Suffix = FString::FromInt(Type);
        Label(this,*(TEXT("Count")+Suffix),FString::FromInt(Edit->Remaining(static_cast<EChessType>(Type))));
        if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(*(TEXT("Glyph")+Suffix))))
        { Text->SetText(FText::FromString(Red ? RedNames[Type] : BlackNames[Type])); Text->SetColorAndOpacity(Red ? FLinearColor(.72f,.12f,.07f) : FLinearColor(.08f,.1f,.12f)); }
        if (auto* Mark = GetWidgetFromName(*(TEXT("Selected")+Suffix))) Mark->SetVisibility(static_cast<int32>(Edit->SelectedType) == Type ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (auto* Switch = Action(this,TEXT("SwitchColor")))
        Switch->SetToolTipText(FText::FromString(Red ? TEXT("当前红方，切换为黑方棋子") : TEXT("当前黑方，切换为红方棋子")));
    Label(this,TEXT("Dirty"),GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->IsDirty() ? TEXT("未保存") : TEXT("已保存"));
    if (auto* Combo = DifficultyCombo(this))
    {
        const auto ComboVisibility = GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->GetDraft().bAI ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        // 整个布局占位一起收起；下拉框的样式与触摸行为直接复用WB_ComboBox。
        if (auto* Frame = GetWidgetFromName(TEXT("DifficultySize"))) Frame->SetVisibility(ComboVisibility);
        else if (auto* Host = GetWidgetFromName(TEXT("DifficultyHost"))) Host->SetVisibility(ComboVisibility);
        else Combo->SetVisibility(ComboVisibility);
    }
    if (auto* Menu = GetWidgetFromName(TEXT("PieceMenu")))
    {
        Menu->SetVisibility(Edit->SelectedChess.IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (Edit->SelectedChess.IsValid())
        {
            FVector2D ScreenPos;
            if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Edit->SelectedChess->GetActorLocation()+FVector(0,0,3),ScreenPos,false))
                if (auto* MenuSlot = Cast<UCanvasPanelSlot>(Menu->Slot))
                {
                    // 投影坐标先转回安全区Canvas的局部坐标，挖孔屏上不会多偏移一段Insets。
                    FVector2D Local = ScreenPos+FVector2D(16,-30);
                    if (auto* Parent = Menu->GetParent(); Parent && !Parent->GetCachedGeometry().GetLocalSize().IsNearlyZero())
                    {
                        const auto& ParentGeometry = Parent->GetCachedGeometry();
                        Local = ParentGeometry.AbsoluteToLocal(UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(ScreenPos))+FVector2D(16,-30);
                        const FVector2D Size = ParentGeometry.GetLocalSize();
                        Local.X = FMath::Clamp(Local.X,0.f,FMath::Max(0.f,static_cast<float>(Size.X)-100.f));
                        Local.Y = FMath::Clamp(Local.Y,0.f,FMath::Max(0.f,static_cast<float>(Size.Y)-42.f));
                    }
                    MenuSlot->SetPosition(Local);
                }
        }
    }
}

void UUI_Reenactment::NativeTick(const FGeometry& Geometry,float DeltaTime) { Super::NativeTick(Geometry,DeltaTime); if (Screen == 1) RefreshEditor(); }
void UUI_Reenactment::SelectKing() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::JIANG); }
void UUI_Reenactment::SelectAdvisor() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::SHI); }
void UUI_Reenactment::SelectElephant() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::XIANG); }
void UUI_Reenactment::SelectHorse() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::MA); }
void UUI_Reenactment::SelectChariot() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::JV); }
void UUI_Reenactment::SelectCannon() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::PAO); }
void UUI_Reenactment::SelectPawn() { if (auto* E = Editor(this)) E->SelectPiece(EChessType::BING); }
