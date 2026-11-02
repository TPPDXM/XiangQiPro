#include "UI_LANLobby.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPUIStyle.h"

void UUI_LANLobby::NativeConstruct()
{
    Super::NativeConstruct();
    XQPUIStyle::SetFactionText(RedFactionLabel, EChessColor::REDCHESS, true);
    XQPUIStyle::SetFactionText(BlackFactionLabel, EChessColor::BLACKCHESS, true);
    if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("JoinRoomLabel"))))
        Label->SetText(FText::FromString(TEXT("加入房间")));
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->SelectGameType(GameType);
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    Room->OnModeMismatch.RemoveAll(this);
    Room->OnModeMismatch.AddUObject(this, &UUI_LANLobby::ShowModeMismatch);
    // 选择器复用原WB_ComboBox，业务层只使用它已有的ComboBox数据接口。
    RoomChoice = RoomSelector ? RoomSelector->ComboBox : nullptr;
    if (!XQPButtonBinding::GetButton(CreateButton) || !RoomChoice)
        return;
    // 只绑定UMG资产中的控件，不生成布局或样式。
    XQPButtonBinding::GetButton(CreateButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnCreate);
    XQPButtonBinding::GetButton(SearchButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnSearch);
    XQPButtonBinding::GetButton(JoinButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnJoin);
    XQPButtonBinding::GetButton(StartButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnStart);
    XQPButtonBinding::GetButton(ReadyButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnReady);
    XQPButtonBinding::GetButton(ColorButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnColor);
    XQPButtonBinding::GetButton(LeaveButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANLobby::OnLeave);
    // 原字符串仍作为选项标识；只约束生成控件的宽度，省略显示不会影响选中及加入目标。
    RoomChoice->OnGenerateWidgetEvent.BindDynamic(this, &UUI_LANLobby::GenerateRoomOption);
    RoomChoice->OnSelectionChanged.AddUniqueDynamic(this, &UUI_LANLobby::OnRoomSelected);
    if (RoomChoice->GetOptionCount() == 0)
    {
        RoomChoice->AddOption(TEXT("选择房间"));
        RoomChoice->SetSelectedIndex(0);
    }
    if (Address) Address->SetHintText(FText::FromString(TEXT("6位联机码")));
    RefreshTime = 1.f;
    XQPButtonBinding::SetCaption(CreateButton, FText::FromString(TEXT("创建房间")));
    XQPButtonBinding::SetCaption(SearchButton, FText::FromString(TEXT("搜索房间")));
    XQPButtonBinding::SetCaption(JoinButton, FText::FromString(TEXT("加入房间")));
    XQPButtonBinding::SetCaption(StartButton, FText::FromString(TEXT("开始对弈")));
    XQPButtonBinding::SetCaption(ColorButton, FText::FromString(TEXT("交换红黑")));
    XQPButtonBinding::SetCaption(LeaveButton, FText::FromString(TEXT("离开房间")));
    if (Room->GetState() == EXQPRoomState::Idle || Room->GetState() == EXQPRoomState::Disconnected)
        Room->FindRooms();
}

void UUI_LANLobby::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    UpdateRoomOptionWidths();
    // 浏览搜索不锁定加入操作。逐帧更新以便选中房间/输入联机码后立即可点；
    // 已提交的联机码查找属于加入事务，期间禁止重复提交或改写目标。
    if (JoinButton && Address)
    {
        const auto* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
        const auto State = LAN->GetState();
        const bool Browsing = State == EXQPRoomState::Idle || State == EXQPRoomState::Searching || State == EXQPRoomState::Disconnected;
        JoinButton->SetIsEnabled(Browsing && !LAN->IsJoiningRoom() && !Address->GetText().ToString().TrimStartAndEnd().IsEmpty());
    }
    RefreshTime += DeltaTime;
    if (RefreshTime < 0.2f || !CreateButton)
        return;
    RefreshTime = 0;
    UXQPLANSubsystem* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    const auto State = LAN->GetState();
    const bool InRoom = State == EXQPRoomState::Creating || State == EXQPRoomState::Lobby || LAN->IsJoiningRoom();
    const bool Hosting = InRoom && LAN->IsHost();
    // 在原介绍框中切换浏览和房间状态，外层经典菜单保持可用。
    BrowserPanel->SetVisibility(InRoom ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    RoomPanel->SetVisibility(InRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RoomName->SetIsEnabled(!InRoom);
    Address->SetIsEnabled(!InRoom);
    SearchButton->SetIsEnabled(State != EXQPRoomState::Searching);
    XQPButtonBinding::SetCaption(SearchButton, FText::FromString(State == EXQPRoomState::Searching ? TEXT("搜索中…") : TEXT("搜索房间")));
    XQPButtonBinding::SetCaption(ReadyButton, FText::FromString(LAN->IsLocalReady() ? TEXT("取消准备") : TEXT("准备对弈")));
    XQPButtonBinding::SetVisibility(ColorButton, Hosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetVisibility(StartButton, Hosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StartButton->SetIsEnabled(LAN->HasPeer() && LAN->IsPeerReady());
    XQPButtonBinding::SetVisibility(ReadyButton, InRoom && !Hosting && !LAN->IsSpectator() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ReadyButton->SetIsEnabled(LAN->HasPeer());
    const FString ReadyStatus = LAN->IsSpectator() ? TEXT("观战席 · 等待双方开局") : Hosting ? (LAN->IsPeerReady() ? TEXT("对方已准备") : TEXT("等待对方准备"))
                                       : (LAN->IsLocalReady() ? TEXT("已准备，等待房主开局") : TEXT("尚未准备"));
    if (RedPlayersText && BlackPlayersText)
    {
        RedPlayersText->SetText(FText::FromString(LAN->GetRedName()));
        BlackPlayersText->SetText(FText::FromString(LAN->GetBlackName()));
        if (PlayersText) PlayersText->SetText(FText::FromString(ReadyStatus));
    }
    else if (PlayersText)
        PlayersText->SetText(FText::FromString(FString::Printf(TEXT("红方：%s\n黑方：%s\n%s"), *LAN->GetRedName(), *LAN->GetBlackName(), *ReadyStatus)));
    const FString Status = LAN->GetStatus().IsEmpty() ? TEXT("双方连接同一局域网，搜索房间或输入联机码加入。") : LAN->GetStatus();
    StatusText->SetText(FText::FromString(Status));
    if (RoomCodePanel)
        RoomCodePanel->SetVisibility(Hosting ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (RoomCodeText)
        RoomCodeText->SetText(FText::FromString(Hosting ? (LAN->GetRoomCode().IsEmpty() ? TEXT("------") : LAN->GetRoomCode()) : FString()));
    // 发现结果改变时才刷新选项，避免每帧重建打断用户选择。
    const auto Rooms = LAN->GetRooms();
    const FString JoinCode = Address->GetText().ToString().TrimStartAndEnd().ToUpper();
    const auto* MatchingRoom = Rooms.FindByPredicate([&JoinCode](const FXQPLANRoom& Room) { return Room.Code == JoinCode; });
    XQPButtonBinding::SetCaption(JoinButton, FText::FromString(MatchingRoom && (MatchingRoom->bFull || MatchingRoom->bInProgress) ? TEXT("加入观战") : TEXT("加入房间")));
    FString Signature;
    for (const auto& Room : Rooms)
        Signature += Room.Code + Room.RoomId + Room.Address + Room.Name + Room.HostName +
            (Room.bFull ? TEXT("full") : TEXT("open")) + (Room.bInProgress ? TEXT("playing") : TEXT("waiting")) + FString::FromInt(Room.Spectators);
    if (Signature != RoomSignature)
    {
        RoomSignature = Signature;
        const FString SelectedCode = Address->GetText().ToString();
        RoomChoice->ClearOptions();
        ListedRooms = Rooms;
        RoomChoice->AddOption(TEXT("选择房间"));
        int32 SelectedIndex = 0;
        for (int32 Index = 0; Index < Rooms.Num(); ++Index)
        {
            const auto& Room = Rooms[Index];
            RoomChoice->AddOption(Room.Code + TEXT(" - ") + Room.Name +
                (Room.bFull || Room.bInProgress ? TEXT("  [可观战]") : TEXT("  [可加入]")));
            if (SelectedRoom.IsSet() ? Room.RoomId == SelectedRoom->RoomId : Room.Code == SelectedCode)
                SelectedIndex = Index + 1;
        }
        // 保留已选房间及输入的联机码，搜索结果变化不替用户改写连接目标。
        RoomChoice->SetSelectedIndex(SelectedIndex);
    }
}
void UUI_LANLobby::OnCreate()
{
    // 网络子系统使用设置页保存的名称，联机页不再提供名字编辑框。
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->CreateRoom(RoomName->GetText().ToString());
}
void UUI_LANLobby::OnSearch()
{
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->FindRooms();
}
void UUI_LANLobby::OnJoin()
{
    auto* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    const FString Code = Address->GetText().ToString().TrimStartAndEnd().ToUpper();
    if (SelectedRoom.IsSet() && SelectedRoom->Code == Code) LAN->JoinDiscoveredRoom(SelectedRoom.GetValue());
    else LAN->JoinRoom(Code);
}
void UUI_LANLobby::OnStart()
{
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->StartMatch();
}
void UUI_LANLobby::OnReady()
{
    auto* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    LAN->SetReady(!LAN->IsLocalReady());
}
void UUI_LANLobby::OnColor()
{
    auto* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    LAN->SetHostColor(FChessPosition::Opponent(LAN->GetHostColor()));
}
void UUI_LANLobby::OnLeave()
{
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
}
void UUI_LANLobby::OnRoomSelected(FString Item, ESelectInfo::Type Selection)
{
    // 第一项为占位文本，后续索引对应真实的房间发现结果。
    const int32 Index = RoomChoice->FindOptionIndex(Item) - 1;
    if (ListedRooms.IsValidIndex(Index))
    {
        SelectedRoom = ListedRooms[Index];
        Address->SetText(FText::FromString(SelectedRoom->Code));
    }
    else if (Selection != ESelectInfo::Direct) SelectedRoom.Reset();
}
UWidget* UUI_LANLobby::GenerateRoomOption(FString Item)
{
    auto* Box = NewObject<USizeBox>(this);
    auto* Label = NewObject<UTextBlock>(this);
    // 状态独立占用自动宽度；主文本使用中间省略号，房间再长也不会挤掉可加入/可观战。
    FString MainText, Status;
    if (!Item.Split(TEXT("  ["), &MainText, &Status)) MainText = Item;
    Label->SetText(FText::FromString(MainText));
    Label->SetFont(RoomChoice->GetFont());
    Label->SetColorAndOpacity(RoomChoice->GetForegroundColor());
    Label->SetTextOverflowPolicy(ETextOverflowPolicy::MiddleEllipsis);
    Label->SetClipping(EWidgetClipping::ClipToBounds);
    auto* Row = NewObject<UHorizontalBox>(this);
    Row->AddChildToHorizontalBox(Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (!Status.IsEmpty())
    {
        auto* StateLabel = NewObject<UTextBlock>(this);
        StateLabel->SetText(FText::FromString(TEXT("[") + Status));
        StateLabel->SetFont(RoomChoice->GetFont());
        StateLabel->SetColorAndOpacity(RoomChoice->GetForegroundColor());
        Row->AddChildToHorizontalBox(StateLabel)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    }
    Box->AddChild(Row);
    RoomOptionBoxes.Add(Box);
    UpdateRoomOptionWidths();
    return Box;
}

void UUI_LANLobby::UpdateRoomOptionWidths()
{
    if (!RoomChoice) return;
    // 使用控件自身的 Slate 单位，已经包含移动端 DPI/SafeZone 缩放。
    // 额外给箭头、行边距和滚动条留空；首帧尚无几何时使用保守宽度，下一帧自动校正。
    const float Width = RoomChoice->GetCachedGeometry().GetLocalSize().X;
    const float ContentWidth = Width > 0.f ? FMath::Max(1.f, Width - 80.f) : 160.f;
    RoomOptionBoxes.RemoveAll([](const TWeakObjectPtr<USizeBox>& Box) { return !Box.IsValid(); });
    for (const auto& Box : RoomOptionBoxes) Box->SetWidthOverride(ContentWidth);
}

void UUI_LANLobby::NativeDestruct()
{
    // 换到其他经典模式条目时退出未开局的房间，开局切关时保留连接。
    if (auto* GI = GetGameInstance())
        if (auto* LAN = GI->GetSubsystem<UXQPLANSubsystem>())
        {
            LAN->OnModeMismatch.RemoveAll(this);
            if (LAN->GetState() != EXQPRoomState::Playing && LAN->GetState() != EXQPRoomState::Finished)
                LAN->LeaveRoom();
        }
    Super::NativeDestruct();
}

void UUI_LANLobby::ShowModeMismatch(const FString& Reason)
{
    if (MismatchDialog.IsValid() && MismatchDialog->IsInViewport()) return;
    auto* DialogClass = LoadClass<UUI_CommonDialog>(nullptr, PATH_UI_COMMON_DIALOG);
    auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetGameInstance(), DialogClass);
    if (!Dialog) return;
    MismatchDialog = Dialog;
    Dialog->Configure(FText::FromString(TEXT("玩法不匹配")), FText::FromString(Reason), FText::FromString(TEXT("知道了")), FText::GetEmpty());
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(Dialog, &UUI_CommonDialog::Dismiss);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
}
