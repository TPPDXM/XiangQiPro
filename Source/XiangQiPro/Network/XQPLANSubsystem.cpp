#include "XQPLANSubsystem.h"
#include "XQPLANDiscovery.h"
#include "XiangQiPro/Util/AssetPaths.h"
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
#include "XiangQiPro/GameMode/FengHuoBattleComponent.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"

namespace
{
constexpr int32 DiscoveryPort = 42137;
constexpr int32 GamePort = 42138;
// 每个本机房主独占一个发现端口，避免 UDP reuse 在不同平台只把广播交给一个进程。
constexpr int32 DiscoveryPortCount = 32;
constexpr int32 ProtocolVersion = 2;
constexpr int32 MaxPacket = 8192;
TSharedRef<FJsonObject> Packet(const TCHAR* Type)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("type"), Type);
    return Result;
}
FString NewRoomCode()
{
    constexpr TCHAR Digits[] = TEXT("23456789");
    constexpr TCHAR Letters[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ");
    constexpr TCHAR Alphabet[] = TEXT("23456789ABCDEFGHJKLMNPQRSTUVWXYZ");
    const FGuid Seed = FGuid::NewGuid();
    FRandomStream Random(static_cast<int32>(Seed.A ^ Seed.B ^ Seed.C ^ Seed.D));
    FString Code;
    Code.AppendChar(Digits[Random.RandRange(0, UE_ARRAY_COUNT(Digits) - 2)]);
    Code.AppendChar(Letters[Random.RandRange(0, UE_ARRAY_COUNT(Letters) - 2)]);
    for (int32 Index = 2; Index < 6; ++Index) Code.AppendChar(Alphabet[Random.RandRange(0, UE_ARRAY_COUNT(Alphabet) - 2)]);
    for (int32 Index = 5; Index > 0; --Index) Swap(Code[Index], Code[Random.RandRange(0, Index)]);
    return Code;
}
bool ValidRoomCode(const FString& Code)
{
    if (Code.Len() != 6) return false;
    bool bDigit = false, bLetter = false;
    for (TCHAR C : Code)
    {
        if (C >= TEXT('0') && C <= TEXT('9')) bDigit = true;
        else if (C >= TEXT('A') && C <= TEXT('Z')) bLetter = true;
        else return false;
    }
    return bDigit && bLetter;
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
    if (State == EXQPRoomState::Playing && IsFengHuoRoom())
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr) GS->FengHuoBattle->Suspend();
    CloseGuests();
    DestroySocket(Peer);
    DestroySocket(Listener);
    BoundGamePort = 0;
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
    if (bHost)
    {
        BroadcastSpectators(Packet(TEXT("leave")));
        for (auto& Guest : Guests) FlushSocket(Guest.Socket, Guest.Send);
    }
    CloseSockets();
    bSpectator = false;
    PendingSpectatorWinner.Reset();
    State = EXQPRoomState::Idle;
    RoomCode.Reset();
    RoomId.Reset();
    PendingJoinCode.Reset();
    JoinDiscoveryError.Reset();
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
    // 优先使用兼容的默认端口，冲突时让系统分配空闲端口。
    LeaveRoom();
    bHost = true;
    HostColor = EChessColor::REDCHESS;
    RoomName = CleanName(Name);
    RoomId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    RoomCode = NewRoomCode();
    CreationDeadline = FPlatformTime::Seconds() + 1.5;
    LastCodeClaim = 0;
    LocalName = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()->GetPlayerName();
    PeerName.Reset();
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Listener = Sockets->CreateSocket(NAME_Stream, TEXT("XQP LAN Host"), false);
    auto Address = Sockets->CreateInternetAddr();
    Address->SetAnyAddress();
    Address->SetPort(GamePort);
    if (!Listener || !Listener->SetNonBlocking(true))
    {
        Disconnect(TEXT("无法创建房间监听连接"));
        return false;
    }
    if (!Listener->Bind(*Address))
    {
        Address->SetPort(0);
        if (!Listener->Bind(*Address))
        {
            Disconnect(TEXT("建房失败，无法分配空闲端口"));
            return false;
        }
    }
    if (!Listener->Listen(2))
    {
        Disconnect(TEXT("无法监听房间连接"));
        return false;
    }
    Listener->GetAddress(*Address);
    BoundGamePort = Address->GetPort();
    Discovery = Sockets->CreateSocket(NAME_DGram, TEXT("XQP Discovery Host"), false);
    if (Discovery)
    {
        bool bBound = false;
        if (Discovery->SetNonBlocking(true) && Discovery->SetBroadcast(true))
            for (int32 Index = 0; Index < DiscoveryPortCount && !bBound; ++Index)
            {
                Address->SetPort(DiscoveryPort + Index);
                bBound = Discovery->Bind(*Address);
            }
        if (!bBound) DestroySocket(Discovery);
    }
    if (!Discovery)
    {
        Disconnect(TEXT("无法分配房间发现端口，请关闭本机多余房间后重试"));
        return false;
    }
    DiscoveryTargets = XQPLANDiscovery::GetTargets();
    State = EXQPRoomState::Creating;
    Status = TEXT("正在创建房间…");
    return true;
}

void UXQPLANSubsystem::FindRooms()
{
    // 搜索端绑定临时端口，允许同一台机器上同时运行房主与客户端。
    if (Peer || Listener)
        return;
    DestroySocket(Discovery);
    PendingJoinCode.Reset();
    JoinDiscoveryError.Reset();
    Rooms.Reset();
    bHost = false;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Discovery = Sockets->CreateSocket(NAME_DGram, TEXT("XQP Discover"), false);
    auto Address = Sockets->CreateInternetAddr();
    Address->SetAnyAddress();
    Address->SetPort(0);
    if (!Discovery)
    {
        Status = TEXT("无法搜索房间，请检查网络连接后重试");
        return;
    }
    if (!Discovery->SetNonBlocking(true) || !Discovery->SetBroadcast(true) || !Discovery->Bind(*Address))
    {
        DestroySocket(Discovery);
        Status = TEXT("无法搜索房间，请检查网络连接后重试");
        return;
    }
    DiscoveryTargets = XQPLANDiscovery::GetTargets();
    SearchDeadline = FPlatformTime::Seconds() + 3;
    LastDiscovery = 0;
    State = EXQPRoomState::Searching;
    Status = TEXT("正在搜索局域网房间…");
}

bool UXQPLANSubsystem::JoinRoom(const FString& Input)
{
    FString Code = Input.TrimStartAndEnd().ToUpper();
    Code.ReplaceInline(TEXT(" "), TEXT(""));
    Code.ReplaceInline(TEXT("-"), TEXT(""));
    if (!ValidRoomCode(Code))
    {
        Status = TEXT("请输入房主提供的6位数字字母联机码");
        return false;
    }
    // 手输的联机码若唯一命中已有发现结果，同样复用缓存地址；未命中或重码才走发现流程。
    const auto Matches = Rooms.FilterByPredicate([&Code](const FXQPLANRoom& Room) { return Room.Code == Code; });
    if (Matches.Num() == 1) return JoinDiscoveredRoom(Matches[0]);
    LeaveRoom();
    FindRooms();
    if (State != EXQPRoomState::Searching) return false;
    PendingJoinCode = Code;
    JoinDiscoveryRetries = 0;
    Status = TEXT("正在查找联机码对应的房间…");
    return true;
}

bool UXQPLANSubsystem::JoinDiscoveredRoom(const FXQPLANRoom& DiscoveredRoom)
{
    // 拷贝完整快照，避免退出旧连接后仍持有房间列表或 UI 选择器内部的引用。
    const FXQPLANRoom Room = DiscoveredRoom;
    if (Room.GameType != GameType)
    {
        Status = TEXT("该房间玩法与当前入口不匹配，请从对应玩法的联机入口加入。");
        OnModeMismatch.Broadcast(Status);
        return false;
    }
    FGuid ParsedId;
    if (!ValidRoomCode(Room.Code) || !FGuid::ParseExact(Room.RoomId, EGuidFormats::Digits, ParsedId) || !ParsedId.IsValid())
    {
        Status = TEXT("房间信息已失效，请重新搜索房间");
        return false;
    }
    LeaveRoom();
    // 即使房间码随后发生变化，也由 room_id 校验房间身份，避免误入端口被复用的新房间。
    // 缓存地址失效时使用现有连接超时提示，由用户刷新，不自动退回联机码搜索。
    if (ConnectToEndpoint(Room)) return true;
    Disconnect(TEXT("无法连接已发现的房间，请重新搜索后重试"));
    return false;
}

bool UXQPLANSubsystem::ConnectToEndpoint(const FXQPLANRoom& Room)
{
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    auto Address = Sockets->CreateInternetAddr();
    FString Host, PortText;
    if (!Room.Address.Split(TEXT(":"), &Host, &PortText)) return false;
    bool bValid = false;
    Address->SetIp(*Host, bValid);
    const int32 Port = FCString::Atoi(*PortText);
    if (!bValid || !PortText.IsNumeric() || Port < 1 || Port > 65535) return false;
    Address->SetPort(Port);
    bSpectator = Room.bFull || Room.bInProgress;
    RoomCode = Room.Code;
    RoomId = Room.RoomId;
    bHost = false;
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
    auto Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);
    Address->SetPort(BoundGamePort);
    return Address->ToString(BoundGamePort > 0);
}
FString UXQPLANSubsystem::GetRedName() const
{
    if (bSpectator) return SpectatorRedName;
    return (HostColor == EChessColor::REDCHESS) == bHost ? LocalName : PeerName;
}
FString UXQPLANSubsystem::GetBlackName() const
{
    if (bSpectator) return SpectatorBlackName;
    return (HostColor == EChessColor::BLACKCHESS) == bHost ? LocalName : PeerName;
}

bool UXQPLANSubsystem::QueueMessage(const TSharedRef<FJsonObject>& Message, TArray<uint8>& Buffer)
{
    FString Json;
    FJsonSerializer::Serialize(Message, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    Json.AppendChar('\n');
    FTCHARToUTF8 Bytes(*Json);
    if (Bytes.Length() > MaxPacket || Buffer.Num() + Bytes.Length() > MaxPacket * 4) return false;
    Buffer.Append(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
    return true;
}

void UXQPLANSubsystem::Send(const TSharedRef<FJsonObject>& Message)
{
    if (!QueueMessage(Message, SendBuffer)) Disconnect(TEXT("连接拥塞，房间已断开"));
}

void UXQPLANSubsystem::FlushSocket(FSocket* Socket, TArray<uint8>& Buffer)
{
    if (!Socket || Buffer.IsEmpty()) return;
    int32 Sent = 0;
    if (Socket->Send(Buffer.GetData(), Buffer.Num(), Sent) && Sent > 0) Buffer.RemoveAt(0, Sent, EAllowShrinking::No);
}

void UXQPLANSubsystem::FlushSend()
{
    FlushSocket(Peer, SendBuffer);
}

void UXQPLANSubsystem::BroadcastDiscovery(const FString& Payload)
{
    if (!Discovery) return;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    // 使用每轮刷新得到的目标，避免每 0.6 秒重新枚举系统网卡。
    auto Address = Sockets->CreateInternetAddr();
    FTCHARToUTF8 Bytes(*Payload);
    for (uint32 Target : DiscoveryTargets)
    {
        Address->SetIp(Target);
        for (int32 Index = 0; Index < DiscoveryPortCount; ++Index)
        {
            Address->SetPort(DiscoveryPort + Index);
            int32 Sent = 0;
            Discovery->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Sent, *Address);
        }
    }
}

void UXQPLANSubsystem::TickDiscovery(double Now)
{
    // 广播只携带发现协议，真正加入和身份交换使用TCP。
    if (!Discovery)
        return;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!bHost && State == EXQPRoomState::Searching && Now - LastDiscovery > 0.6)
    {
        BroadcastDiscovery(TEXT("XQP_DISCOVER_V2"));
        LastDiscovery = Now;
    }
    if (bHost && !Peer && (State == EXQPRoomState::Creating || State == EXQPRoomState::Lobby) && Now - LastCodeClaim > .5)
    {
        auto Claim = Packet(TEXT("XQP_CODE_CLAIM_V2"));
        Claim->SetStringField(TEXT("room_id"), RoomId);
        Claim->SetStringField(TEXT("code"), RoomCode);
        FString Text;
        FJsonSerializer::Serialize(Claim, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        BroadcastDiscovery(Text);
        LastCodeClaim = Now;
    }
    for (int32 Count = 0; Count < 128; ++Count)
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
        if (bHost && !Peer && Payload != TEXT("XQP_DISCOVER_V2"))
        {
            TSharedPtr<FJsonObject> Claim;
            FString Type, Id, Code;
            FGuid ParsedId;
            if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Claim) && Claim &&
                Claim->TryGetStringField(TEXT("type"), Type) && Type == TEXT("XQP_CODE_CLAIM_V2") &&
                Claim->TryGetStringField(TEXT("room_id"), Id) && FGuid::ParseExact(Id, EGuidFormats::Digits, ParsedId) &&
                Claim->TryGetStringField(TEXT("code"), Code) && Code == RoomCode && RoomId.Compare(Id) > 0)
            {
                // 确定性的房间ID仲裁：只有一方换码，同时创建也不会互相反复换码。
                RoomCode = NewRoomCode();
                State = EXQPRoomState::Creating;
                CreationDeadline = Now + 1.5;
                LastCodeClaim = 0;
            }
            continue;
        }
        if (bHost && (State == EXQPRoomState::Lobby || State == EXQPRoomState::Playing || State == EXQPRoomState::Finished) && Payload == TEXT("XQP_DISCOVER_V2"))
        {
            auto Reply = Packet(TEXT("XQP_ROOM_V2"));
            Reply->SetStringField(TEXT("name"), RoomName);
            Reply->SetStringField(TEXT("host"), LocalName);
            Reply->SetStringField(TEXT("game_type"), XQPGameTypes::FindLANMode(GameType)->ProtocolId);
            Reply->SetNumberField(TEXT("port"), BoundGamePort);
            Reply->SetStringField(TEXT("room_id"), RoomId);
            Reply->SetStringField(TEXT("code"), RoomCode);
            Reply->SetBoolField(TEXT("full"), Peer != nullptr);
            Reply->SetBoolField(TEXT("playing"), State == EXQPRoomState::Playing || State == EXQPRoomState::Finished);
            Reply->SetNumberField(TEXT("spectators"), GetSpectatorCount());
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
                !Reply->TryGetStringField(TEXT("type"), Type) || Type != TEXT("XQP_ROOM_V2") ||
                !Reply->TryGetStringField(TEXT("name"), Name) || !Reply->TryGetStringField(TEXT("host"), Host))
                continue;
            FString TypeId;
            int32 Port;
            if (!Reply->TryGetStringField(TEXT("game_type"), TypeId) || !ReadInt(Reply, TEXT("port"), Port, 1, 65535)) continue;
            const auto* Mode = XQPGameTypes::FindLANMode(TypeId);
            FString Code, Id;
            FGuid ParsedId;
            if (!Mode || !Reply->TryGetStringField(TEXT("code"), Code) || !ValidRoomCode(Code) ||
                !Reply->TryGetStringField(TEXT("room_id"), Id) || !FGuid::ParseExact(Id, EGuidFormats::Digits, ParsedId) ||
                !ParsedId.IsValid()) continue;
            if (Mode->Type != GameType)
            {
                if (Code == PendingJoinCode)
                    JoinDiscoveryError = FString::Printf(TEXT("该联机码属于%s，请从对应玩法的联机入口加入。"), Mode->DisplayName);
                continue;
            }
            bool Full, Playing;
            int32 SpectatorCount;
            if (!Reply->TryGetBoolField(TEXT("full"), Full) || !Reply->TryGetBoolField(TEXT("playing"), Playing) ||
                !ReadInt(Reply, TEXT("spectators"), SpectatorCount, 0, 16)) continue;
            Sender->SetPort(Port);
            const FString IP = Sender->ToString(true);
            if (auto* Existing = Rooms.FindByPredicate([&Id](const FXQPLANRoom& R) { return R.RoomId == Id; }))
            {
                // 同一房间可能同时回应广播和回环发现，按房间ID合并。
                if (!IP.StartsWith(TEXT("127."))) Existing->Address = IP;
                Existing->Code = Code;
                Existing->bFull = Full;
                Existing->bInProgress = Playing;
                Existing->Spectators = SpectatorCount;
            }
            else if (Rooms.Num() < 256)
            {
                FXQPLANRoom Room;
                Room.Name = CleanName(Name);
                Room.HostName = CleanName(Host);
                Room.Address = IP;
                Room.GameType = Mode->Type;
                Room.Code = Code;
                Room.bFull = Full;
                Room.bInProgress = Playing;
                Room.Spectators = SpectatorCount;
                Room.RoomId = Id;
                Rooms.Add(Room);
            }
        }
    }
    if (bHost && State == EXQPRoomState::Creating && Now >= CreationDeadline)
    {
        State = EXQPRoomState::Lobby;
        Status = TEXT("房间已建立，等待棋友加入");
    }
    if (!bHost && State == EXQPRoomState::Searching && Now >= SearchDeadline)
    {
        if (!PendingJoinCode.IsEmpty())
        {
            const FString Code = PendingJoinCode;
            const auto Matches = Rooms.FilterByPredicate([&Code](const FXQPLANRoom& Room) { return Room.Code == Code; });
            if (!JoinDiscoveryError.IsEmpty())
            {
                Status = JoinDiscoveryError;
                OnModeMismatch.Broadcast(Status);
            }
            else if (Matches.Num() == 1)
            {
                DestroySocket(Discovery);
                PendingJoinCode.Reset();
                ConnectToEndpoint(Matches[0]);
                return;
            }
            else if (Matches.Num() > 1 && JoinDiscoveryRetries++ < 3)
            {
                // 将碰撞房间的声明转发给所有房主，让它们自动仲裁；加入端继续查找原码。
                for (const auto& Room : Matches)
                {
                    auto Claim = Packet(TEXT("XQP_CODE_CLAIM_V2"));
                    Claim->SetStringField(TEXT("room_id"), Room.RoomId);
                    Claim->SetStringField(TEXT("code"), Room.Code);
                    FString Text;
                    FJsonSerializer::Serialize(Claim, TJsonWriterFactory<>::Create(&Text));
                    BroadcastDiscovery(Text);
                }
                Rooms.Reset();
                SearchDeadline = Now + 3;
                LastDiscovery = 0;
                return;
            }
            else if (Matches.Num() > 1) Status = TEXT("网络状态异常，暂时无法连接房间，请稍后重试");
            else Status = TEXT("未找到该联机码，请确认双方在同一局域网、房间仍在等待加入且网络允许房间发现");
        }
        else Status = Rooms.IsEmpty() ? TEXT("未发现房间，请确认双方已连接同一局域网") : FString::Printf(TEXT("找到%d个房间"), Rooms.Num());
        DestroySocket(Discovery);
        PendingJoinCode.Reset();
        State = EXQPRoomState::Idle;
    }
}

void UXQPLANSubsystem::TickConnection(double Now)
{
    // 新连接先独立握手，主机只把一个参赛席位交给权威落子通道，其余进入只读观战席。
    if (Listener && State != EXQPRoomState::Creating)
    {
        bool Pending = false;
        if (Listener->HasPendingConnection(Pending) && Pending)
        {
            FSocket* Accepted = Listener->Accept(TEXT("XQP Guest"));
            if (Accepted)
            {
                if (Guests.Num() >= 24) DestroySocket(Accepted);
                else
                {
                    Accepted->SetNonBlocking(true);
                    Accepted->SetNoDelay(true);
                    auto& Guest = Guests.AddDefaulted_GetRef();
                    Guest.Socket = Accepted;
                    Guest.LastReceive = Guest.LastPing = Now;
                }
            }
        }
    }
    TickGuests(Now);
    if (!Peer)
        return;
    if (State == EXQPRoomState::Connecting)
    {
        if (Peer->GetConnectionState() == SCS_Connected)
        {
            auto Hello = Packet(TEXT("hello"));
            Hello->SetNumberField(TEXT("version"), ProtocolVersion);
            Hello->SetStringField(TEXT("name"), LocalName);
            Hello->SetStringField(TEXT("role"), bSpectator ? TEXT("spectator") : TEXT("player"));
            Hello->SetStringField(TEXT("room_id"), RoomId);
            Hello->SetStringField(TEXT("game_type"), XQPGameTypes::FindLANMode(GameType)->ProtocolId);
            Send(Hello);
            State = EXQPRoomState::Lobby;
            Status = TEXT("正在进入房间…");
        }
        else if (Now - LastReceive > 8)
        {
            Disconnect(TEXT("连接超时，请确认房间仍在开放且网络连接正常"));
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
    Message->SetStringField(TEXT("game_type"), XQPGameTypes::FindLANMode(GameType)->ProtocolId);
    Message->SetNumberField(TEXT("color"), static_cast<uint8>(HostColor));
    Send(Message);
    for (auto& Guest : Guests) if (Guest.bWatching) SendSpectatorWelcome(Guest);
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
    if (bSpectator || bHost || !bHandshake || (State != EXQPRoomState::Lobby && State != EXQPRoomState::Finished))
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
    AuthoritativeBoard = FChessPosition::Standard();
    Turn = EChessColor::REDCHESS;
    bLocalBoardReady = bPeerBoardReady = bMoveRequested = false;
    PendingMoves.Reset();
    auto Start = Packet(TEXT("start"));
    Start->SetStringField(TEXT("game_type"), XQPGameTypes::FindLANMode(GameType)->ProtocolId);
    Start->SetNumberField(TEXT("color"), static_cast<uint8>(HostColor));
    Send(Start);
    for (auto& Guest : Guests)
    {
        Guest.bBoardReady = false;
        Guest.LatestRealtimeSnapshot.Reset();
    }
    BroadcastSpectators(Start);
    for (auto& Guest : Guests) FlushSocket(Guest.Socket, Guest.Send);
    FlushSend();
    OpenBattleMap();
    return true;
}
void UXQPLANSubsystem::OpenBattleMap()
{
    PendingSpectatorWinner.Reset();
    FengHuoRequestSerial = LastFengHuoPeerRequest = 0;
    PendingProSnapshot.Reset();
    PendingStartSnapshot.Reset();
    bPeerResigned = bLocalResigned = false;
    State = EXQPRoomState::Playing;
    Status = TEXT("等待双方棋盘就绪…");
    bLocalReady = bPeerReady = false;
    const auto* Mode = XQPGameTypes::FindLANMode(GameType);
    if (!Mode) { Disconnect(TEXT("该玩法尚未支持联机")); return; }
    if (UXQPGameInstance* GI = Cast<UXQPGameInstance>(GetGameInstance())) GI->SetGameMode(Mode->Mode);
    UXQPGameInstance::OpenGameLevel(GetWorld(), Mode->MapPath);
}
void UXQPLANSubsystem::NotifyBoardReady()
{
    bLocalBoardReady = true;
    Send(Packet(TEXT("board_ready")));
    FlushSend();
}
void UXQPLANSubsystem::NotifyMatchFinished()
{
    if (bHost)
    {
        auto Result = Packet(TEXT("spectator_result"));
        const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
        Result->SetNumberField(TEXT("winner"), GS ? static_cast<uint8>(GS->GetWinnerColor()) : 0);
        BroadcastSpectators(Result);
    }
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
    if (bSpectator || GameType != EXQPGameType::Classic || !GS || !GS->BattleStart->HasStarted()) return false;
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
    BroadcastSpectators(Message);
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
    if (Type == TEXT("spectator_welcome") && !bHost)
    {
        if (!ReceiveSpectatorWelcome(M)) Disconnect(TEXT("观战房间快照无效"));
        return;
    }
    if (Type == TEXT("spectator_result") && !bHost && bSpectator && bHandshake)
    {
        int32 Winner;
        if (!ReadInt(M, TEXT("winner"), Winner, 0, 1)) { Disconnect(TEXT("观战结果数据无效")); return; }
        if (!IsFengHuoRoom()) PendingSpectatorWinner = static_cast<EChessColor>(Winner);
        return;
    }
    if (Type == TEXT("hello") && bHost && !bHandshake)
    {
        int32 Version;
        FString Name;
        if (!ReadInt(M, TEXT("version"), Version, ProtocolVersion, ProtocolVersion) || !M->TryGetStringField(TEXT("name"), Name))
        {
            Disconnect(TEXT("双方游戏版本不一致"));
            return;
        }
        FString RequestedId;
        if (!M->TryGetStringField(TEXT("room_id"), RequestedId) || RequestedId != RoomId)
        {
            DestroySocket(Peer);
            ReceiveBuffer.Reset();
            SendBuffer.Reset();
            Status = TEXT("联机码已失效，房间继续等待加入");
            return;
        }
        PeerName = CleanName(Name);
        FString TypeId;
        const auto* Mode = XQPGameTypes::FindLANMode(GameType);
        if (!M->TryGetStringField(TEXT("game_type"), TypeId) || !Mode || TypeId != Mode->ProtocolId)
        {
            auto Failure = Packet(TEXT("mode_mismatch"));
            Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("该房间为%s，请从对应玩法的联机入口加入。"), Mode->DisplayName));
            Send(Failure); FlushSend();
            DestroySocket(Peer); ReceiveBuffer.Reset(); SendBuffer.Reset();
            PeerName.Reset();
            Status = TEXT("对方玩法不匹配，房间继续等待加入");
            OnModeMismatch.Broadcast(TEXT("加入者选择的玩法与房间不同，已拒绝连接。\n房间保持开放。"));
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
        FString TypeId;
        const auto* Mode = XQPGameTypes::FindLANMode(GameType);
        if (!M->TryGetStringField(TEXT("game_type"), TypeId) || !Mode || TypeId != Mode->ProtocolId) { OnModeMismatch.Broadcast(TEXT("房间玩法不同，请从对应玩法的联机入口加入。")); Disconnect(TEXT("房间玩法不同，请选择对应玩法后加入")); return; }
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
        if ((!bSpectator && !bLocalReady) || !ReadInt(M, TEXT("color"), Color, 0, 1))
        {
            Disconnect(TEXT("开局状态不一致"));
            return;
        }
        HostColor = static_cast<EChessColor>(Color);
        FString TypeId;
        const auto* Mode = XQPGameTypes::FindLANMode(GameType);
        if (!M->TryGetStringField(TEXT("game_type"), TypeId) || !Mode || TypeId != Mode->ProtocolId) { OnModeMismatch.Broadcast(TEXT("开局玩法不一致，请重新选择对应房间。")); Disconnect(TEXT("开局玩法不一致")); return; }
        Sequence = 0;
        AuthoritativeBoard = FChessPosition::Standard();
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
    if (Type == TEXT("pro_request") && bHost && IsFengHuoRoom() && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        int32 From, To, Request;
        bool Applied = false;
        if (ReadInt(M, TEXT("from"), From, 0, 89) && ReadInt(M, TEXT("to"), To, 0, 89) &&
            ReadInt(M, TEXT("request"), Request, 1, 1000000) && Request > LastFengHuoPeerRequest)
        {
            LastFengHuoPeerRequest = Request;
            if (IsBattleReady())
                if (auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>(); GS && GS->board2P)
                    Applied = GS->FengHuoBattle->TryMove(GS->board2P->GetChess({From / 9, From % 9}).Get(),
                        {{From / 9, From % 9}, {To / 9, To % 9}}, FChessPosition::Opponent(HostColor));
        }
        if (!Applied) Send(Packet(TEXT("pro_rejected")));
        return;
    }
    if (Type == TEXT("pro_rejected") && !bHost && IsFengHuoRoom()) { Status = TEXT("落子未生效：请检查粮草及棋子状态"); return; }
    if (Type == TEXT("pro_state") && !bHost && IsFengHuoRoom() && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        PendingProSnapshot = M;
        return;
    }
    if (Type == TEXT("request") && bHost && State == EXQPRoomState::Playing)
    {
        if (GameType != EXQPGameType::Classic) { Disconnect(TEXT("房间落子协议不一致")); return; }
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
        if (GameType != EXQPGameType::Classic) { Disconnect(TEXT("房间落子协议不一致")); return; }
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
        if (IsFengHuoRoom() && !bHost) return;
        bPeerResigned = true;
        if (AXQPGameStateBase* GS = GetWorld()->GetGameState<AXQPGameStateBase>())
            GS->NotifyGameOver(GetLocalColor());
        return;
    }
    Disconnect(TEXT("收到与当前房间状态不符的消息"));
}

void UXQPLANSubsystem::Resign()
{
    if (bSpectator || State != EXQPRoomState::Playing)
        return;
    bLocalResigned = true;
    Send(Packet(TEXT("resign")));
    FlushSend();
    if (IsFengHuoRoom() && !bHost) return;
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
    if (IsFengHuoRoom() && PendingProSnapshot.IsValid() && bLocalBoardReady)
    {
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr; GS && GS->FengHuoBattle->IsActive())
        {
            const auto Snapshot = PendingProSnapshot;
            PendingProSnapshot.Reset();
            if (!GS->FengHuoBattle->ReceiveSnapshot(Snapshot)) Disconnect(TEXT("实时对局数据无效"));
        }
    }
    if (bSpectator && PendingSpectatorWinner.IsSet() && bLocalBoardReady && PendingMoves.IsEmpty())
        if (auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
            GS && (GS->IsGameOver() || GS->CanApplyNetworkMove()))
        {
            const auto Winner = PendingSpectatorWinner.GetValue();
            PendingSpectatorWinner.Reset();
            if (!GS->IsGameOver()) GS->NotifyGameOver(Winner);
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

bool UXQPLANSubsystem::SelectGameType(EXQPGameType Type)
{
    if (!XQPGameTypes::FindLANMode(Type)) return false;
    if (GameType == Type) return true;
    LeaveRoom();
    GameType = Type;
    Rooms.Reset();
    return true;
}

void UXQPLANSubsystem::SelectFengHuoMode(bool bSelected)
{
    if (bSelected) SelectGameType(EXQPGameType::FengHuo);
    else SelectGameType(EXQPGameType::Classic);
}

bool UXQPLANSubsystem::SubmitFengHuoMove(const FChessMove2P& Move)
{
    if (bSpectator || !IsFengHuoRoom() || !IsBattleReady() || !Move.IsValid()) return false;
    auto* GS = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!GS || !GS->board2P) return false;
    AChesses* Chess = GS->board2P->GetChess(Move.from).Get();
    if (!GS->FengHuoBattle->CanMove(Chess, Move, GetLocalColor())) return false;
    if (bHost) return GS->FengHuoBattle->TryMove(Chess, Move, GetLocalColor());
    auto Message = Packet(TEXT("pro_request"));
    Message->SetNumberField(TEXT("request"), ++FengHuoRequestSerial);
    Message->SetNumberField(TEXT("from"), Move.from.X * 9 + Move.from.Y);
    Message->SetNumberField(TEXT("to"), Move.to.X * 9 + Move.to.Y);
    Send(Message);
    return true;
}

void UXQPLANSubsystem::BroadcastFengHuoState(const TSharedRef<FJsonObject>& Message)
{
    if (bHost && IsFengHuoRoom() && bHandshake && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        Send(Message);
        BroadcastSpectators(Message);
    }
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
    if (bHost && bHandshake && State == EXQPRoomState::Playing)
    {
        Send(Message);
        BroadcastSpectators(Message);
    }
}
