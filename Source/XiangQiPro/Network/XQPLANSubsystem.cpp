#include "XQPLANSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/GameMode/ProBattleComponent.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"

namespace
{
constexpr int32 DiscoveryPort = 42137;
constexpr int32 GamePort = 42138;
constexpr int32 MaxPacket = 8192;
TSharedRef<FJsonObject> Packet(const TCHAR* Type)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("type"), Type);
    return Result;
}
FString CleanName(const FString& Input)
{
    FString Out;
    for (TCHAR C : Input.Left(32))
        if (!FChar::IsControl(C))
            Out.AppendChar(C);
    Out = Out.TrimStartAndEnd();
    return Out.IsEmpty() ? TEXT("棋友") : Out;
}
void DestroySocket(FSocket*& Socket)
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
}
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Value, int32 Min, int32 Max)
{
    double Number;
    if (!Object->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number) || Number < Min || Number > Max ||
        FMath::FloorToDouble(Number) != Number)
        return false;
    Value = static_cast<int32>(Number);
    return true;
}
FChessPosition InitialBoard()
{
    FChessPosition Board;
    const EChessType Back[] = {EChessType::JV,    EChessType::MA,    EChessType::XIANG,
                               EChessType::SHI,   EChessType::JIANG, EChessType::SHI,
                               EChessType::XIANG, EChessType::MA,    EChessType::JV};
    for (int32 Color = 0; Color < 2; ++Color)
    {
        for (int32 File = 0; File < 9; ++File)
            Board.Set({Color ? 9 : 0, File}, {Back[File], static_cast<EChessColor>(Color)});
        for (int32 File : {1, 7})
            Board.Set({Color ? 7 : 2, File}, {EChessType::PAO, static_cast<EChessColor>(Color)});
        for (int32 File : {0, 2, 4, 6, 8})
            Board.Set({Color ? 6 : 3, File}, {EChessType::BING, static_cast<EChessColor>(Color)});
    }
    return Board;
}
} // namespace

void UXQPLANSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UPlayerProfileSubsystem>();
}
void UXQPLANSubsystem::Deinitialize()
{
    CloseSockets();
    Super::Deinitialize();
}

void UXQPLANSubsystem::CloseSockets()
{
    if (State == EXQPRoomState::Playing && bProRoom)
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr) GS->ProBattle->Suspend();
    DestroySocket(Peer);
    DestroySocket(Listener);
    DestroySocket(Discovery);
    SendBuffer.Reset();
    ReceiveBuffer.Reset();
    PendingMoves.Reset();
    PendingProSnapshot.Reset();
    PendingStartSnapshot.Reset();
    bHandshake = bPeerReady = bLocalReady = bLocalBoardReady = bPeerBoardReady = bMoveRequested = false;
}
void UXQPLANSubsystem::LeaveRoom()
{
    if (Peer && bHandshake)
    {
        Send(Packet(TEXT("leave")));
        FlushSend();
    }
    CloseSockets();
    State = EXQPRoomState::Idle;
    Status = TEXT("已离开房间");
}
void UXQPLANSubsystem::Disconnect(const FString& Reason)
{
    UE_LOG(LogTemp, Warning, TEXT("XQP LAN disconnected: %s"), *Reason);
    CloseSockets();
    State = EXQPRoomState::Disconnected;
    Status = Reason;
    OnRoomUpdated.Broadcast();
}

bool UXQPLANSubsystem::CreateRoom(const FString& Name)
{
    // TCP固定端口负责一位对手；UDP发现失败时仍允许通过IP加入。
    LeaveRoom();
    bHost = true;
    HostColor = EChessColor::REDCHESS;
    RoomName = CleanName(Name);
    LocalName = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->GetPlayerName();
    PeerName.Reset();
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Listener = Sockets->CreateSocket(NAME_Stream, TEXT("XQP LAN Host"), false);
    auto Address = Sockets->CreateInternetAddr();
    Address->SetAnyAddress();
    Address->SetPort(GamePort);
    if (!Listener || !Listener->SetNonBlocking(true) || !Listener->Bind(*Address) || !Listener->Listen(2))
    {
        Disconnect(TEXT("建房失败，端口42138可能已被其他房间占用"));
        return false;
    }
    Discovery = Sockets->CreateSocket(NAME_DGram, TEXT("XQP Discovery Host"), false);
    Address->SetPort(DiscoveryPort);
    if (Discovery)
    {
        Discovery->SetNonBlocking(true);
        Discovery->SetReuseAddr(true);
        Discovery->SetBroadcast(true);
        if (!Discovery->Bind(*Address))
            DestroySocket(Discovery);
    }
    State = EXQPRoomState::Lobby;
    Status = TEXT("房间已建立，等待棋友加入");
    return true;
}

void UXQPLANSubsystem::FindRooms()
{
    // 搜索端绑定临时端口，允许同一台机器上同时运行房主与客户端。
    if (Peer || Listener)
        return;
    DestroySocket(Discovery);
    Rooms.Reset();
    bHost = false;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Discovery = Sockets->CreateSocket(NAME_DGram, TEXT("XQP Discover"), false);
    auto Address = Sockets->CreateInternetAddr();
    Address->SetAnyAddress();
    Address->SetPort(0);
    if (!Discovery)
    {
        Status = TEXT("无法搜索房间，请使用IP加入");
        return;
    }
    Discovery->SetNonBlocking(true);
    Discovery->SetBroadcast(true);
    if (!Discovery->Bind(*Address))
    {
        DestroySocket(Discovery);
        Status = TEXT("无法搜索房间，请使用IP加入");
        return;
    }
    SearchDeadline = FPlatformTime::Seconds() + 3;
    LastDiscovery = 0;
    State = EXQPRoomState::Searching;
    Status = TEXT("正在搜索局域网房间…");
}

bool UXQPLANSubsystem::JoinRoom(const FString& Input)
{
    // 使用非阻塞连接，界面继续更新；握手由TickConnection完成。
    LeaveRoom();
    bHost = false;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    auto Address = Sockets->CreateInternetAddr();
    bool bValid = false;
    Address->SetIp(*Input.TrimStartAndEnd(), bValid);
    Address->SetPort(GamePort);
    if (!bValid)
    {
        Status = TEXT("请输入有效的IPv4地址，例如192.168.1.8");
        return false;
    }
    Peer = Sockets->CreateSocket(NAME_Stream, TEXT("XQP LAN Client"), false);
    if (!Peer)
    {
        Status = TEXT("无法创建连接");
        return false;
    }
    Peer->SetNonBlocking(true);
    Peer->SetNoDelay(true);
    LocalName = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->GetPlayerName();
    Peer->Connect(*Address);
    LastReceive = LastPing = FPlatformTime::Seconds();
    State = EXQPRoomState::Connecting;
    Status = TEXT("正在连接房间…");
    return true;
}

FString UXQPLANSubsystem::GetLocalAddress() const
{
    bool bCanBind = false;
    return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind)->ToString(false);
}
FString UXQPLANSubsystem::GetRedName() const
{
    return (HostColor == EChessColor::REDCHESS) == bHost ? LocalName : PeerName;
}
FString UXQPLANSubsystem::GetBlackName() const
{
    return (HostColor == EChessColor::BLACKCHESS) == bHost ? LocalName : PeerName;
}

void UXQPLANSubsystem::Send(const TSharedRef<FJsonObject>& Message)
{
    // 协议每行一个JSON，必须使用紧凑格式，禁止格式化器在消息内部插入换行。
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Message, Writer);
    Json.AppendChar('\n');
    FTCHARToUTF8 Bytes(*Json);
    if (SendBuffer.Num() + Bytes.Length() > MaxPacket * 4)
    {
        Disconnect(TEXT("连接拥塞，房间已断开"));
        return;
    }
    SendBuffer.Append(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
}
void UXQPLANSubsystem::FlushSend()
{
    // TCP一次Send可能只写出部分字节，余下数据保留到下一帧。
    if (!Peer || SendBuffer.IsEmpty())
        return;
    int32 Sent = 0;
    if (Peer->Send(SendBuffer.GetData(), SendBuffer.Num(), Sent) && Sent > 0)
        SendBuffer.RemoveAt(0, Sent, EAllowShrinking::No);
}

void UXQPLANSubsystem::TickDiscovery(double Now)
{
    // 广播只携带发现协议，真正加入和身份交换使用TCP。
    if (!Discovery)
        return;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!bHost && State == EXQPRoomState::Searching && Now - LastDiscovery > 0.6)
    {
        auto Broadcast = Sockets->CreateInternetAddr();
        Broadcast->SetBroadcastAddress();
        Broadcast->SetPort(DiscoveryPort);
        const ANSICHAR Query[] = "XQP_DISCOVER_V1";
        int32 Sent;
        Discovery->SendTo(reinterpret_cast<const uint8*>(Query), sizeof(Query) - 1, Sent, *Broadcast);
        LastDiscovery = Now;
    }
    for (int32 Count = 0; Count < 16; ++Count)
    {
        uint32 Pending;
        if (!Discovery->HasPendingData(Pending))
            break;
        uint8 Buffer[2048];
        int32 Read = 0;
        auto Sender = Sockets->CreateInternetAddr();
        if (!Discovery->RecvFrom(Buffer, sizeof(Buffer) - 1, Read, *Sender) || Read <= 0)
            break;
        Buffer[Read] = 0;
        const FString Payload = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Buffer));
        if (bHost && State == EXQPRoomState::Lobby && !Peer && Payload == TEXT("XQP_DISCOVER_V1"))
        {
            auto Reply = Packet(TEXT("XQP_ROOM_V1"));
            Reply->SetStringField(TEXT("name"), RoomName);
            Reply->SetStringField(TEXT("host"), LocalName);
            Reply->SetBoolField(TEXT("pro"), bProRoom);
            FString Text;
            FJsonSerializer::Serialize(Reply, TJsonWriterFactory<>::Create(&Text));
            FTCHARToUTF8 Bytes(*Text);
            int32 Sent;
            Discovery->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Sent, *Sender);
        }
        else if (!bHost && State == EXQPRoomState::Searching)
        {
            TSharedPtr<FJsonObject> Reply;
            FString Type, Name, Host;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Reply) || !Reply ||
                !Reply->TryGetStringField(TEXT("type"), Type) || Type != TEXT("XQP_ROOM_V1") ||
                !Reply->TryGetStringField(TEXT("name"), Name) || !Reply->TryGetStringField(TEXT("host"), Host))
                continue;
            bool Pro = false;
            Reply->TryGetBoolField(TEXT("pro"), Pro);
            if (Pro != bProRoom) continue;
            const FString IP = Sender->ToString(false);
            if (!Rooms.ContainsByPredicate([&IP](const FXQPLANRoom& R) { return R.Address == IP; }) && Rooms.Num() < 32)
            {
                FXQPLANRoom Room;
                Room.Name = CleanName(Name);
                Room.HostName = CleanName(Host);
                Room.Address = IP;
                Rooms.Add(Room);
            }
        }
    }
    if (!bHost && State == EXQPRoomState::Searching && Now >= SearchDeadline)
    {
        DestroySocket(Discovery);
        State = EXQPRoomState::Idle;
        Status =
            Rooms.IsEmpty() ? TEXT("未发现房间，可输入房主IP加入") : FString::Printf(TEXT("找到%d个房间"), Rooms.Num());
    }
}

void UXQPLANSubsystem::TickConnection(double Now)
{
    // 已有对手时拒绝额外连接，避免第三个人接管现有对局。
    if (Listener)
    {
        bool Pending = false;
        if (Listener->HasPendingConnection(Pending) && Pending)
        {
            FSocket* Accepted = Listener->Accept(TEXT("XQP Peer"));
            if (Peer)
                DestroySocket(Accepted);
            else if (Accepted)
            {
                Peer = Accepted;
                Peer->SetNonBlocking(true);
                Peer->SetNoDelay(true);
                LastReceive = LastPing = Now;
            }
        }
    }
    if (!Peer)
        return;
    if (State == EXQPRoomState::Connecting)
    {
        if (Peer->GetConnectionState() == SCS_Connected)
        {
            auto Hello = Packet(TEXT("hello"));
            Hello->SetNumberField(TEXT("version"), 1);
            Hello->SetStringField(TEXT("name"), LocalName);
            Hello->SetBoolField(TEXT("pro"), bProRoom);
            Send(Hello);
            State = EXQPRoomState::Lobby;
            Status = TEXT("正在进入房间…");
        }
        else if (Now - LastReceive > 8)
        {
            Disconnect(TEXT("连接超时，请检查房主IP和防火墙"));
            return;
        }
    }
    if (Now - LastReceive > (bHandshake ? 30.0 : 10.0))
    {
        Disconnect(TEXT("对方连接已中断，请返回大厅重新建房"));
        return;
    }
    if (bHandshake && Now - LastPing >= 2)
    {
        Send(Packet(TEXT("ping")));
        LastPing = Now;
    }
    FlushSend();
    // 把任意分包/粘包重新组装成完整消息，每帧处理量与消息大小都有上限。
    for (int32 Count = 0; Peer && Count < 16; ++Count)
    {
        uint32 Pending;
        if (!Peer->HasPendingData(Pending))
            break;
        uint8 Buffer[2048];
        int32 Read = 0;
        if (!Peer->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0)
            break;
        ReceiveBuffer.Append(Buffer, Read);
        LastReceive = Now;
        if (ReceiveBuffer.Num() > MaxPacket * 2)
        {
            Disconnect(TEXT("收到无效房间数据"));
            return;
        }
        int32 Newline;
        while (Peer && ReceiveBuffer.Find(static_cast<uint8>('\n'), Newline))
        {
            if (Newline > MaxPacket)
            {
                Disconnect(TEXT("收到过长房间消息"));
                return;
            }
            FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(ReceiveBuffer.GetData()), Newline);
            FString Text(Decoded.Length(), Decoded.Get());
            ReceiveBuffer.RemoveAt(0, Newline + 1, EAllowShrinking::No);
            TSharedPtr<FJsonObject> Message;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Message) || !Message)
            {
                Disconnect(TEXT("房间数据格式不正确"));
                return;
            }
            HandleMessage(Message);
        }
    }
}

void UXQPLANSubsystem::SendLobby()
{
    auto Message = Packet(TEXT("lobby"));
    Message->SetStringField(TEXT("name"), LocalName);
    Message->SetStringField(TEXT("room"), RoomName);
    Message->SetBoolField(TEXT("pro"), bProRoom);
    Message->SetNumberField(TEXT("color"), static_cast<uint8>(HostColor));
    Send(Message);
}
void UXQPLANSubsystem::SetHostColor(EChessColor Color)
{
    if (!bHost || State != EXQPRoomState::Lobby || static_cast<uint8>(Color) > 1)
        return;
    HostColor = Color;
    bPeerReady = false;
    if (bHandshake)
        SendLobby();
}
void UXQPLANSubsystem::SetReady(bool bReady)
{
    if (bHost || !bHandshake || (State != EXQPRoomState::Lobby && State != EXQPRoomState::Finished))
        return;
    bLocalReady = bReady;
    auto Message = Packet(TEXT("ready"));
    Message->SetBoolField(TEXT("value"), bReady);
    Send(Message);
    OnRoomUpdated.Broadcast();
}
bool UXQPLANSubsystem::StartMatch()
{
    // 房主和来访者准备状态一致后才创建新棋局，双方从同一标准局面开始。
    if (!bHost || !bHandshake || !bPeerReady || (State != EXQPRoomState::Lobby && State != EXQPRoomState::Finished))
        return false;
    Sequence = 0;
    AuthoritativeBoard = InitialBoard();
    Turn = EChessColor::REDCHESS;
    bLocalBoardReady = bPeerBoardReady = bMoveRequested = false;
    PendingMoves.Reset();
    auto Start = Packet(TEXT("start"));
    Start->SetBoolField(TEXT("pro"), bProRoom);
    Start->SetNumberField(TEXT("color"), static_cast<uint8>(HostColor));
    Send(Start);
    FlushSend();
    OpenBattleMap();
    return true;
}
void UXQPLANSubsystem::OpenBattleMap()
{
    ProRequestSerial = LastProPeerRequest = 0;
    PendingProSnapshot.Reset();
    PendingStartSnapshot.Reset();
    bPeerResigned = bLocalResigned = false;
    State = EXQPRoomState::Playing;
    Status = TEXT("等待双方棋盘就绪…");
    bLocalReady = bPeerReady = false;
    if (UXQPGameInstance* GI = Cast<UXQPGameInstance>(GetGameInstance()))
        GI->SetGameMode(bProRoom ? EXQPGameMode::ProBattleLAN : EXQPGameMode::LAN);
    UXQPGameInstance::OpenGameLevel(GetWorld(), TEXT("/Game/Map/Battle2P"));
}
void UXQPLANSubsystem::NotifyBoardReady()
{
    bLocalBoardReady = true;
    Send(Packet(TEXT("board_ready")));
    FlushSend();
}
void UXQPLANSubsystem::NotifyMatchFinished()
{
    if (State == EXQPRoomState::Playing)
    {
        State = EXQPRoomState::Finished;
        Status = TEXT("本局结束");
        bLocalReady = bPeerReady = false;
    }
}
bool UXQPLANSubsystem::CanLocalMove() const
{
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!GS || !GS->BattleStart->HasStarted()) return false;
    return State == EXQPRoomState::Playing && bHandshake && bLocalBoardReady && bPeerBoardReady && !bMoveRequested &&
           Turn == GetLocalColor() && PendingMoves.IsEmpty();
}
bool UXQPLANSubsystem::SubmitMove(const FChessMove2P& Move)
{
    if (!CanLocalMove())
        return false;
    if (bHost)
        return CommitMove(Move, GetLocalColor(), Sequence);
    auto Request = Packet(TEXT("request"));
    Request->SetNumberField(TEXT("seq"), Sequence);
    Request->SetNumberField(TEXT("from"), Move.from.X * 9 + Move.from.Y);
    Request->SetNumberField(TEXT("to"), Move.to.X * 9 + Move.to.Y);
    Send(Request);
    bMoveRequested = true;
    return true;
}
bool UXQPLANSubsystem::CommitMove(const FChessMove2P& Move, EChessColor Requester, int32 ExpectedSequence)
{
    // 不信任客户端提交的坐标、颜色或步号，逐项对照权威棋盘。
    if (State != EXQPRoomState::Playing || !bLocalBoardReady || !bPeerBoardReady || ExpectedSequence != Sequence ||
        Turn != Requester || !Move.IsValid() || AuthoritativeBoard.Get(Move.from).Color != Requester ||
        !PendingMoves.IsEmpty())
        return false;
    const AXQPGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!GS || !GS->CanApplyNetworkMove())
        return false;
    const auto Moves = AuthoritativeBoard.GenerateLegalMoves(Turn);
    if (!Moves.ContainsByPredicate([&Move](const FChessMove2P& M) { return M.from == Move.from && M.to == Move.to; }))
        return false;
    AuthoritativeBoard.MakeMove(Move);
    Turn = FChessPosition::Opponent(Turn);
    ++Sequence;
    // 广播同一个权威结果给对方，并在本机排队播放；双方都不预测落子。
    auto Message = Packet(TEXT("move"));
    Message->SetNumberField(TEXT("seq"), Sequence);
    Message->SetNumberField(TEXT("from"), Move.from.X * 9 + Move.from.Y);
    Message->SetNumberField(TEXT("to"), Move.to.X * 9 + Move.to.Y);
    Message->SetStringField(
        TEXT("hash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(AuthoritativeBoard.Hash(Turn))));
    Send(Message);
    PendingMoves.Add(Move);
    return true;
}

void UXQPLANSubsystem::HandleMessage(const TSharedPtr<FJsonObject>& M)
{
    // 握手前只接受版本与姓名，后续消息还要符合当前房间阶段。
    FString Type;
    if (!M->TryGetStringField(TEXT("type"), Type))
    {
        Disconnect(TEXT("房间消息缺少类型"));
        return;
    }
    if (Type == TEXT("hello") && bHost && !bHandshake)
    {
        int32 Version;
        FString Name;
        if (!ReadInt(M, TEXT("version"), Version, 1, 1) || !M->TryGetStringField(TEXT("name"), Name))
        {
            Disconnect(TEXT("双方游戏版本不一致"));
            return;
        }
        PeerName = CleanName(Name);
        bool PeerPro = false;
        if (!M->TryGetBoolField(TEXT("pro"), PeerPro) || PeerPro != bProRoom)
        {
            auto Failure = Packet(TEXT("mode_mismatch"));
            Failure->SetStringField(TEXT("message"), bProRoom ? TEXT("该房间为烽火弈，请从烽火弈的联机入口加入。") : TEXT("该房间为经典象棋，请从经典模式的联机入口加入。"));
            Send(Failure); FlushSend();
            DestroySocket(Peer); ReceiveBuffer.Reset(); SendBuffer.Reset();
            PeerName.Reset();
            Status = TEXT("对方玩法不匹配，房间继续等待加入");
            OnModeMismatch.Broadcast(TEXT("加入者选择的玩法与房间不同，已拒绝连接。房间保持开放。"));
            return;
        }
        bHandshake = true;
        Status = TEXT("棋友已加入，请分配执棋颜色");
        SendLobby();
        return;
    }
    if (Type == TEXT("lobby") && !bHost && State == EXQPRoomState::Lobby)
    {
        FString Name, Room;
        int32 Color;
        if (!M->TryGetStringField(TEXT("name"), Name) || !M->TryGetStringField(TEXT("room"), Room) ||
            !ReadInt(M, TEXT("color"), Color, 0, 1))
        {
            Disconnect(TEXT("房间信息不完整"));
            return;
        }
        PeerName = CleanName(Name);
        RoomName = CleanName(Room);
        HostColor = static_cast<EChessColor>(Color);
        bool Pro = false;
        M->TryGetBoolField(TEXT("pro"), Pro);
        if (Pro != bProRoom) { OnModeMismatch.Broadcast(TEXT("房间玩法不同，请从对应玩法的联机入口加入。")); Disconnect(TEXT("房间玩法不同，请选择对应玩法后加入")); return; }
        bHandshake = true;
        bLocalReady = false;
        Status = TEXT("选择颜色后，请准备开始");
        return;
    }
    if (Type == TEXT("mode_mismatch") && !bHost)
    {
        FString Reason;
        if (!M->TryGetStringField(TEXT("message"), Reason)) Reason = TEXT("双方玩法不同，请选择对应玩法后加入。");
        OnModeMismatch.Broadcast(Reason);
        Disconnect(Reason);
        return;
    }
    if (!bHandshake)
    {
        Disconnect(TEXT("房间握手失败"));
        return;
    }
    if (Type == TEXT("ping"))
        return;
    if (Type == TEXT("leave"))
    {
        Disconnect(TEXT("对方已离开房间"));
        return;
    }
    if (Type == TEXT("ready") && bHost && (State == EXQPRoomState::Lobby || State == EXQPRoomState::Finished))
    {
        bool Ready;
        if (M->TryGetBoolField(TEXT("value"), Ready))
        {
            bPeerReady = Ready;
            OnRoomUpdated.Broadcast();
        }
        return;
    }
    if (Type == TEXT("start") && !bHost && (State == EXQPRoomState::Lobby || State == EXQPRoomState::Finished))
    {
        // 客户端只有亲自准备后才接受开局，换色会使旧准备状态失效。
        int32 Color;
        if (!bLocalReady || !ReadInt(M, TEXT("color"), Color, 0, 1))
        {
            Disconnect(TEXT("开局状态不一致"));
            return;
        }
        HostColor = static_cast<EChessColor>(Color);
        bool Pro = false;
        M->TryGetBoolField(TEXT("pro"), Pro);
        if (Pro != bProRoom) { OnModeMismatch.Broadcast(TEXT("开局玩法不一致，请重新选择对应房间。")); Disconnect(TEXT("开局玩法不一致")); return; }
        Sequence = 0;
        AuthoritativeBoard = InitialBoard();
        Turn = EChessColor::REDCHESS;
        bLocalBoardReady = bPeerBoardReady = bMoveRequested = false;
        PendingMoves.Reset();
        OpenBattleMap();
        return;
    }
    if (Type == TEXT("board_ready") && State == EXQPRoomState::Playing)
    {
        bPeerBoardReady = true;
        Status = TEXT("对弈进行中");
        return;
    }
    if (Type == TEXT("battle_confirm") && bHost && IsBattleReady())
    {
        if (auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>()) GS->BattleStart->ConfirmSide(FChessPosition::Opponent(HostColor));
        return;
    }
    if (Type == TEXT("battle_start_state") && !bHost && State == EXQPRoomState::Playing)
    {
        PendingStartSnapshot = M;
        return;
    }
    if (Type == TEXT("pro_request") && bHost && bProRoom && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        int32 From, To, Request;
        bool Applied = false;
        if (ReadInt(M, TEXT("from"), From, 0, 89) && ReadInt(M, TEXT("to"), To, 0, 89) &&
            ReadInt(M, TEXT("request"), Request, 1, 1000000) && Request > LastProPeerRequest)
        {
            LastProPeerRequest = Request;
            if (IsBattleReady())
                if (auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>(); GS && GS->board2P)
                    Applied = GS->ProBattle->TryMove(GS->board2P->GetChess({From / 9, From % 9}).Get(),
                        {{From / 9, From % 9}, {To / 9, To % 9}}, FChessPosition::Opponent(HostColor));
        }
        if (!Applied) Send(Packet(TEXT("pro_rejected")));
        return;
    }
    if (Type == TEXT("pro_rejected") && !bHost && bProRoom) { Status = TEXT("落子未生效：请检查粮草及棋子状态"); return; }
    if (Type == TEXT("pro_state") && !bHost && bProRoom && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        PendingProSnapshot = M;
        return;
    }
    if (Type == TEXT("request") && bHost && State == EXQPRoomState::Playing)
    {
        if (bProRoom) { Disconnect(TEXT("房间落子协议不一致")); return; }
        // 请求只表达落子意图，失败时不改变棋盘并解除客户端等待锁。
        int32 From, To, Seq;
        if (!ReadInt(M, TEXT("from"), From, 0, 89) || !ReadInt(M, TEXT("to"), To, 0, 89) ||
            !ReadInt(M, TEXT("seq"), Seq, 0, 1000000) ||
            !CommitMove({{From / 9, From % 9}, {To / 9, To % 9}}, FChessPosition::Opponent(HostColor), Seq))
            Send(Packet(TEXT("rejected")));
        return;
    }
    if (Type == TEXT("rejected") && !bHost)
    {
        bMoveRequested = false;
        Status = TEXT("落子未生效，请等待对方落子完成后重试");
        return;
    }
    if (Type == TEXT("move") && !bHost && State == EXQPRoomState::Playing)
    {
        if (bProRoom) { Disconnect(TEXT("房间落子协议不一致")); return; }
        // 按步号检查有序性，再独立验棋和核对局面哈希，发现不同步就停止对局。
        int32 From, To, Seq;
        FString Hash;
        if (!ReadInt(M, TEXT("from"), From, 0, 89) || !ReadInt(M, TEXT("to"), To, 0, 89) ||
            !ReadInt(M, TEXT("seq"), Seq, 1, 1000000) || Seq != Sequence + 1 ||
            !M->TryGetStringField(TEXT("hash"), Hash))
        {
            Disconnect(TEXT("落子顺序不一致，请重新建房"));
            return;
        }
        const FChessMove2P Move({From / 9, From % 9}, {To / 9, To % 9});
        auto Legal = AuthoritativeBoard.GenerateLegalMoves(Turn);
        if (!Legal.ContainsByPredicate([&Move](const FChessMove2P& C)
                                       { return C.from == Move.from && C.to == Move.to; }))
        {
            Disconnect(TEXT("收到非法走法"));
            return;
        }
        AuthoritativeBoard.MakeMove(Move);
        Turn = FChessPosition::Opponent(Turn);
        if (Hash != FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(AuthoritativeBoard.Hash(Turn))))
        {
            Disconnect(TEXT("双方棋盘不同步，请重新建房"));
            return;
        }
        Sequence = Seq;
        PendingMoves.Add(Move);
        bMoveRequested = false;
        return;
    }
    if (Type == TEXT("resign") && State == EXQPRoomState::Playing)
    {
        if (bProRoom && !bHost) return;
        bPeerResigned = true;
        if (AXQPGameStateBase* GS = GetWorld()->GetGameState<AXQPGameStateBase>())
            GS->NotifyGameOver(GetLocalColor());
        return;
    }
    Disconnect(TEXT("收到与当前房间状态不符的消息"));
}

void UXQPLANSubsystem::Resign()
{
    if (State != EXQPRoomState::Playing)
        return;
    bLocalResigned = true;
    Send(Packet(TEXT("resign")));
    FlushSend();
    if (bProRoom && !bHost) return;
    if (AXQPGameStateBase* GS = GetWorld()->GetGameState<AXQPGameStateBase>())
        GS->NotifyGameOver(FChessPosition::Opponent(GetLocalColor()));
}
void UXQPLANSubsystem::Tick(float DeltaTime)
{
    // 网络始终运行；棋子动画完成后才提交下一手，避免逻辑与Actor位置交错。
    const double Now = FPlatformTime::Seconds();
    TickDiscovery(Now);
    TickConnection(Now);
    if (PendingStartSnapshot.IsValid() && bLocalBoardReady)
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr; GS && GS->BattleStart->IsActive())
        {
            const auto Snapshot = PendingStartSnapshot; PendingStartSnapshot.Reset();
            if (!GS->BattleStart->ReceiveSnapshot(Snapshot)) Disconnect(TEXT("开局确认数据无效"));
        }
    if (bProRoom && PendingProSnapshot.IsValid() && bLocalBoardReady)
    {
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr; GS && GS->ProBattle->IsActive())
        {
            const auto Snapshot = PendingProSnapshot;
            PendingProSnapshot.Reset();
            if (!GS->ProBattle->ReceiveSnapshot(Snapshot)) Disconnect(TEXT("实时对局数据无效"));
        }
    }
    if (State == EXQPRoomState::Playing && !PendingMoves.IsEmpty())
        if (AXQPGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
            GS && GS->CanApplyNetworkMove())
        {
            const FChessMove2P Move = PendingMoves[0];
            PendingMoves.RemoveAt(0);
            GS->ApplyNetworkMove(Move);
        }
}

void UXQPLANSubsystem::SelectProMode(bool bSelected)
{
    if (bProRoom == bSelected) return;
    LeaveRoom();
    bProRoom = bSelected;
    Rooms.Reset();
}

bool UXQPLANSubsystem::SubmitProMove(const FChessMove2P& Move)
{
    if (!bProRoom || !IsBattleReady() || !Move.IsValid()) return false;
    auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!GS || !GS->board2P) return false;
    AChesses* Chess = GS->board2P->GetChess(Move.from).Get();
    if (!GS->ProBattle->CanMove(Chess, Move, GetLocalColor())) return false;
    if (bHost) return GS->ProBattle->TryMove(Chess, Move, GetLocalColor());
    auto Message = Packet(TEXT("pro_request"));
    Message->SetNumberField(TEXT("request"), ++ProRequestSerial);
    Message->SetNumberField(TEXT("from"), Move.from.X * 9 + Move.from.Y);
    Message->SetNumberField(TEXT("to"), Move.to.X * 9 + Move.to.Y);
    Send(Message);
    return true;
}

void UXQPLANSubsystem::BroadcastProState(const TSharedRef<FJsonObject>& Message)
{
    if (bHost && bProRoom && bHandshake && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished)) Send(Message);
}

void UXQPLANSubsystem::ConfirmBattleStart()
{
    if (!IsBattleReady()) return;
    if (bHost)
    {
        if (auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>()) GS->BattleStart->ConfirmSide(GetLocalColor());
    }
    else Send(Packet(TEXT("battle_confirm")));
}

void UXQPLANSubsystem::BroadcastBattleStart(const TSharedRef<FJsonObject>& Message)
{
    if (bHost && bHandshake && State == EXQPRoomState::Playing) Send(Message);
}
