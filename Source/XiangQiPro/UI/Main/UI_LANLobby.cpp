#include "UI_LANLobby.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "Components/ComboBoxString.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPUIStyle.h"

void UUI_LANLobby::NativeConstruct()
{
    Super::NativeConstruct();
    XQPUIStyle::SetFactionText(RedFactionLabel, EChessColor::REDCHESS, true);
    XQPUIStyle::SetFactionText(BlackFactionLabel, EChessColor::BLACKCHESS, true);
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->SelectFengHuoMode(bFengHuoLobby);
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
    RoomChoice->OnSelectionChanged.AddUniqueDynamic(this, &UUI_LANLobby::OnRoomSelected);
    if (RoomChoice->GetOptionCount() == 0)
    {
        RoomChoice->AddOption(TEXT("选择房间"));
        RoomChoice->SetSelectedIndex(0);
    }
    RefreshTime = 1.f;
    XQPButtonBinding::SetCaption(CreateButton, FText::FromString(TEXT("创建房间")));
    XQPButtonBinding::SetCaption(SearchButton, FText::FromString(TEXT("搜索房间")));
    XQPButtonBinding::SetCaption(JoinButton, FText::FromString(TEXT("加入房间")));
    XQPButtonBinding::SetCaption(StartButton, FText::FromString(TEXT("开始对弈")));
    XQPButtonBinding::SetCaption(ColorButton, FText::FromString(TEXT("交换红黑")));
    XQPButtonBinding::SetCaption(LeaveButton, FText::FromString(TEXT("离开房间")));
}

void UUI_LANLobby::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshTime += DeltaTime;
    if (RefreshTime < 0.2f || !CreateButton)
        return;
    RefreshTime = 0;
    UXQPLANSubsystem* LAN = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    const auto State = LAN->GetState();
    const bool InRoom = State == EXQPRoomState::Lobby || State == EXQPRoomState::Connecting;
    const bool Hosting = InRoom && LAN->IsHost();
    // 在原介绍框中切换浏览和房间状态，外层经典菜单保持可用。
    BrowserPanel->SetVisibility(InRoom ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    RoomPanel->SetVisibility(InRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RoomName->SetIsEnabled(!InRoom);
    Address->SetIsEnabled(!InRoom);
    SearchButton->SetIsEnabled(State != EXQPRoomState::Searching);
    JoinButton->SetIsEnabled(State != EXQPRoomState::Connecting && !Address->GetText().ToString().TrimStartAndEnd().IsEmpty());
    XQPButtonBinding::SetCaption(SearchButton, FText::FromString(State == EXQPRoomState::Searching ? TEXT("搜索中…") : TEXT("搜索房间")));
    XQPButtonBinding::SetCaption(ReadyButton, FText::FromString(LAN->IsLocalReady() ? TEXT("取消准备") : TEXT("准备对弈")));
    XQPButtonBinding::SetVisibility(ColorButton, Hosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetVisibility(StartButton, Hosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StartButton->SetIsEnabled(LAN->HasPeer() && LAN->IsPeerReady());
    XQPButtonBinding::SetVisibility(ReadyButton, InRoom && !Hosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ReadyButton->SetIsEnabled(LAN->HasPeer());
    const FString ReadyStatus = Hosting ? (LAN->IsPeerReady() ? TEXT("对方已准备") : TEXT("等待对方准备"))
                                       : (LAN->IsLocalReady() ? TEXT("已准备，等待房主开局") : TEXT("尚未准备"));
    if (RedPlayersText && BlackPlayersText)
    {
        RedPlayersText->SetText(FText::FromString(LAN->GetRedName()));
        BlackPlayersText->SetText(FText::FromString(LAN->GetBlackName()));
        if (PlayersText) PlayersText->SetText(FText::FromString(ReadyStatus));
    }
    else if (PlayersText)
        PlayersText->SetText(FText::FromString(FString::Printf(TEXT("红方：%s\n黑方：%s\n%s"), *LAN->GetRedName(), *LAN->GetBlackName(), *ReadyStatus)));
    const FString Status = LAN->GetStatus().IsEmpty() ? TEXT("双方连接同一局域网，搜索房间或输入房主 IP 加入。") : LAN->GetStatus();
    StatusText->SetText(FText::FromString(Status + (Hosting ? TEXT("\n房主 IP：") + LAN->GetLocalAddress() : FString())));
    // 发现结果改变时才刷新选项，避免每帧重建打断用户选择。
    const auto Rooms = LAN->GetRooms();
    FString Signature;
    for (const auto& Room : Rooms)
        Signature += Room.Address + Room.Name;
    if (Signature != RoomSignature)
    {
        RoomSignature = Signature;
        const FString SelectedAddress = Address->GetText().ToString();
        RoomChoice->ClearOptions();
        RoomChoice->AddOption(TEXT("选择房间"));
        int32 SelectedIndex = 0;
        for (int32 Index = 0; Index < Rooms.Num(); ++Index)
        {
            const auto& Room = Rooms[Index];
            RoomChoice->AddOption(Room.Name + TEXT(" · ") + Room.HostName + TEXT(" · ") + Room.Address);
            if (Room.Address == SelectedAddress) SelectedIndex = Index + 1;
        }
        // 保留已选房间及手输IP，搜索结果变化不替用户改写连接目标。
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
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->JoinRoom(Address->GetText().ToString().TrimStartAndEnd());
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
    const auto Rooms = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->GetRooms();
    const int32 Index = RoomChoice->FindOptionIndex(Item) - 1;
    if (Rooms.IsValidIndex(Index))
        Address->SetText(FText::FromString(Rooms[Index].Address));
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
    auto* DialogClass = LoadClass<UUI_CommonDialog>(nullptr, TEXT("/Game/UMG/Components/BP_UI_CommonDialog.BP_UI_CommonDialog_C"));
    auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetGameInstance(), DialogClass);
    if (!Dialog) return;
    MismatchDialog = Dialog;
    Dialog->Configure(FText::FromString(TEXT("玩法不匹配")), FText::FromString(Reason), FText::FromString(TEXT("知道了")), FText::GetEmpty());
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(Dialog, &UUI_CommonDialog::Dismiss);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog, false);
}
