#include "XQPLANSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameMode/FengHuoBattleComponent.h"

namespace
{
    constexpr int32 MaxSpectators = 16;
    constexpr int32 MaxGuestPacket = 8192;
    void CloseGuestSocket(FSocket*& Socket)
    {
        if (!Socket) return;
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
    TSharedRef<FJsonObject> GuestPacket(const TCHAR* Type)
    {
        auto M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("type"), Type);
        return M;
    }
}

int32 UXQPLANSubsystem::GetSpectatorCount() const
{
    int32 Count = 0;
    for (const auto& Guest : Guests) if (Guest.bWatching) ++Count;
    return Count;
}

void UXQPLANSubsystem::CloseGuests()
{
    for (auto& Guest : Guests) CloseGuestSocket(Guest.Socket);
    Guests.Reset();
}

void UXQPLANSubsystem::BroadcastSpectators(const TSharedRef<FJsonObject>& Message)
{
    if (!bHost) return;
    const bool bRealtime = Message->GetStringField(TEXT("type")) == TEXT("pro_state");
    for (auto& Guest : Guests)
        if (Guest.bWatching && Guest.Socket)
        {
            if (bRealtime)
            {
                // 地图加载时不堆积实时帧；低带宽观众只保留最新快照，不能拖慢棋手。
                if (Guest.bBoardReady) Guest.LatestRealtimeSnapshot = Message;
            }
            else if (!QueueMessage(Message, Guest.Send)) CloseGuestSocket(Guest.Socket);
        }
}

void UXQPLANSubsystem::SendSpectatorWelcome(FXQPLANGuest& Guest)
{
    auto M = GuestPacket(TEXT("spectator_welcome"));
    M->SetStringField(TEXT("game_type"), XQPGameTypes::FindLANMode(GameType)->ProtocolId);
    M->SetStringField(TEXT("red_name"), GetRedName());
    M->SetStringField(TEXT("black_name"), GetBlackName());
    M->SetNumberField(TEXT("phase"), static_cast<uint8>(State));
    M->SetNumberField(TEXT("color"), static_cast<uint8>(HostColor));
    M->SetNumberField(TEXT("turn"), static_cast<uint8>(Turn));
    M->SetNumberField(TEXT("seq"), Sequence);
    TArray<TSharedPtr<FJsonValue>> Squares;
    const FChessPosition Board = IsFengHuoRoom() ? FChessPosition::Standard() : AuthoritativeBoard;
    for (const auto& P : Board.Squares)
        Squares.Add(MakeShared<FJsonValueNumber>(static_cast<uint8>(P.Type) | (static_cast<uint8>(P.Color) << 4)));
    M->SetArrayField(TEXT("board"), Squares);
    if (const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr)
        M->SetNumberField(TEXT("winner"), static_cast<uint8>(GS->GetWinnerColor()));
    else M->SetNumberField(TEXT("winner"), 0);
    QueueMessage(M, Guest.Send);
    if (const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
        GS && (State == EXQPRoomState::Playing || State == EXQPRoomState::Finished))
    {
        QueueMessage(GS->BattleStart->MakeSnapshot(), Guest.Send);
        if (IsFengHuoRoom()) QueueMessage(GS->FengHuoBattle->MakeSnapshot(), Guest.Send);
    }
    FlushSocket(Guest.Socket, Guest.Send);
}

void UXQPLANSubsystem::TickGuests(double Now)
{
    for (int32 Index = Guests.Num() - 1; Index >= 0; --Index)
    {
        auto& Guest = Guests[Index];
        bool bRemove = !Guest.Socket || Now - Guest.LastReceive > (Guest.bWatching ? 30.0 : 10.0);
        for (int32 Count = 0; !bRemove && Count < 16; ++Count)
        {
            uint32 Pending;
            if (!Guest.Socket->HasPendingData(Pending)) break;
            uint8 Buffer[2048];
            int32 Read = 0;
            if (!Guest.Socket->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0) { bRemove = true; break; }
            Guest.Receive.Append(Buffer, Read);
            Guest.LastReceive = Now;
            if (Guest.Receive.Num() > MaxGuestPacket * 2) { bRemove = true; break; }
            int32 Newline;
            while (!bRemove && Guest.Receive.Find(static_cast<uint8>('\n'), Newline))
            {
                if (Newline > MaxGuestPacket) { bRemove = true; break; }
                FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Guest.Receive.GetData()), Newline);
                TSharedPtr<FJsonObject> M;
                const bool bParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Text.Length(), Text.Get())), M);
                Guest.Receive.RemoveAt(0, Newline + 1, EAllowShrinking::No);
                FString Type;
                if (!bParsed || !M || !M->TryGetStringField(TEXT("type"), Type)) { bRemove = true; break; }
                if (Guest.bWatching)
                {
                    // 观战者只可维持连接、确认画面加载或离开。落子/准备/认输均不能进入权威对局入口。
                    if (Type == TEXT("board_ready") && !Guest.bBoardReady)
                    {
                        Guest.bBoardReady = true;
                        if (const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr)
                        {
                            QueueMessage(GS->BattleStart->MakeSnapshot(), Guest.Send);
                            if (IsFengHuoRoom()) Guest.LatestRealtimeSnapshot = GS->FengHuoBattle->MakeSnapshot();
                        }
                    }
                    else if (Type != TEXT("ping") && Type != TEXT("board_ready")) bRemove = true;
                    continue;
                }
                FString Role, Id, Mode, Name;
                double Version;
                if (Type != TEXT("hello") || !M->TryGetNumberField(TEXT("version"), Version) || Version != ProtocolVersion ||
                    !M->TryGetStringField(TEXT("name"), Name) ||
                    !M->TryGetStringField(TEXT("role"), Role) || (Role != TEXT("player") && Role != TEXT("spectator")) ||
                    !M->TryGetStringField(TEXT("room_id"), Id) || Id != RoomId ||
                    !M->TryGetStringField(TEXT("game_type"), Mode) || Mode != XQPGameTypes::FindLANMode(GameType)->ProtocolId)
                {
                    auto Failure = GuestPacket(TEXT("mode_mismatch"));
                    Failure->SetStringField(TEXT("message"), TEXT("联机码已失效或双方玩法、版本不匹配，请重新搜索房间。"));
                    QueueMessage(Failure, Guest.Send); FlushSocket(Guest.Socket, Guest.Send);
                    bRemove = true;
                    break;
                }
                if (Role == TEXT("player") && !Peer && State == EXQPRoomState::Lobby)
                {
                    Peer = Guest.Socket;
                    Guest.Socket = nullptr;
                    ReceiveBuffer = MoveTemp(Guest.Receive);
                    LastReceive = LastPing = Now;
                    HandleMessage(M);
                    bRemove = true;
                    break;
                }
                if (GetSpectatorCount() >= MaxSpectators)
                {
                    auto Failure = GuestPacket(TEXT("mode_mismatch"));
                    Failure->SetStringField(TEXT("message"), TEXT("观战席位已满，请稍后再试。"));
                    QueueMessage(Failure, Guest.Send); FlushSocket(Guest.Socket, Guest.Send);
                    bRemove = true;
                    break;
                }
                Guest.bWatching = true;
                Guest.LastPing = Now;
                SendSpectatorWelcome(Guest);
                OnRoomUpdated.Broadcast();
            }
        }
        if (!bRemove && Guest.bWatching && Now - Guest.LastPing >= 2)
        {
            bRemove = !QueueMessage(GuestPacket(TEXT("ping")), Guest.Send);
            Guest.LastPing = Now;
        }
        if (bRemove)
        {
            CloseGuestSocket(Guest.Socket);
            Guests.RemoveAt(Index);
        }
        else
        {
            FlushSocket(Guest.Socket, Guest.Send);
            if (Guest.Send.IsEmpty() && Guest.LatestRealtimeSnapshot.IsValid())
            {
                QueueMessage(Guest.LatestRealtimeSnapshot.ToSharedRef(), Guest.Send);
                Guest.LatestRealtimeSnapshot.Reset();
                FlushSocket(Guest.Socket, Guest.Send);
            }
        }
    }
}

bool UXQPLANSubsystem::ReceiveSpectatorWelcome(const TSharedPtr<FJsonObject>& M)
{
    FString Mode, Red, Black;
    double Phase, Color, Side, Seq, Winner;
    const TArray<TSharedPtr<FJsonValue>>* Values;
    if (!M->TryGetStringField(TEXT("game_type"), Mode) || Mode != XQPGameTypes::FindLANMode(GameType)->ProtocolId ||
        !M->TryGetStringField(TEXT("red_name"), Red) || !M->TryGetStringField(TEXT("black_name"), Black) ||
        !M->TryGetNumberField(TEXT("phase"), Phase) || !M->TryGetNumberField(TEXT("color"), Color) ||
        !M->TryGetNumberField(TEXT("turn"), Side) || !M->TryGetNumberField(TEXT("seq"), Seq) ||
        !M->TryGetNumberField(TEXT("winner"), Winner) || !M->TryGetArrayField(TEXT("board"), Values) || Values->Num() != 90 ||
        (Phase != int32(EXQPRoomState::Lobby) && Phase != int32(EXQPRoomState::Playing) && Phase != int32(EXQPRoomState::Finished)) ||
        (Color != 0 && Color != 1) || (Side != 0 && Side != 1) || (Winner != 0 && Winner != 1) ||
        !FMath::IsFinite(Seq) || Seq < 0 || Seq > 1000000 || FMath::FloorToDouble(Seq) != Seq) return false;
    FChessPosition Board;
    for (int32 I = 0; I < 90; ++I)
    {
        double Value;
        if (!(*Values)[I]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0 || Value > 23 ||
            FMath::FloorToDouble(Value) != Value || (int32(Value) & 15) > 7) return false;
        Board.Squares[I] = {static_cast<EChessType>(int32(Value) & 15), static_cast<EChessColor>(int32(Value) >> 4)};
    }
    bSpectator = bHandshake = true;
    SpectatorRedName = Red.Left(32);
    SpectatorBlackName = Black.Left(32);
    HostColor = static_cast<EChessColor>(int32(Color));
    Turn = static_cast<EChessColor>(int32(Side));
    Sequence = int32(Seq);
    AuthoritativeBoard = Board;
    PendingMoves.Reset();
    if (Phase == int32(EXQPRoomState::Lobby))
    {
        State = EXQPRoomState::Lobby;
        Status = TEXT("观战席已加入，等待双方开局");
    }
    else
    {
        OpenBattleMap();
        if (Phase == int32(EXQPRoomState::Finished)) PendingSpectatorWinner = static_cast<EChessColor>(int32(Winner));
        Status = TEXT("正在进入观战…");
    }
    OnRoomUpdated.Broadcast();
    return true;
}

bool UXQPLANSubsystem::PrepareSpectatorBoard(FChessPosition& Board, EChessColor& Side)
{
    if (!bSpectator || !bHandshake) return false;
    Board = IsFengHuoRoom() ? FChessPosition::Standard() : AuthoritativeBoard;
    Side = Turn;
    // 加载地图期间收到的走法已经包含在当前权威快照里，不能再重复播放。
    PendingMoves.Reset();
    return true;
}
