#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "XiangQiPro/GameMode/XQPGameTypes.h"
#include "XQPLANSubsystem.generated.h"

class FSocket;
class FJsonObject;

UENUM(BlueprintType)
enum class EXQPRoomState : uint8
{
    Idle,
    Searching,
    Connecting,
    Lobby,
    Playing,
    Finished,
    Disconnected,
    Creating
};

USTRUCT(BlueprintType)
struct FXQPLANRoom
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString HostName;
    UPROPERTY(BlueprintReadOnly) FString Address;
    UPROPERTY(BlueprintReadOnly) EXQPGameType GameType = EXQPGameType::Classic;
    UPROPERTY(BlueprintReadOnly) FString Code;
    UPROPERTY(BlueprintReadOnly) bool bFull = false;
    UPROPERTY(BlueprintReadOnly) bool bInProgress = false;
    UPROPERTY(BlueprintReadOnly) int32 Spectators = 0;
    FString RoomId;
};

// 观战连接独立拥有收发队列，不参与对局双方的就绪、步号或认输权限。
struct FXQPLANGuest
{
    FSocket* Socket = nullptr;
    TArray<uint8> Receive;
    TArray<uint8> Send;
    bool bWatching = false;
    bool bBoardReady = false;
    TSharedPtr<FJsonObject> LatestRealtimeSnapshot;
    double LastReceive = 0;
    double LastPing = 0;
};

// TCP保证落子有序；UDP仅负责局域网发现。棋盘规则与胜负由房主校验。
UCLASS()
class XIANGQIPRO_API UXQPLANSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override
    {
        return !IsTemplate();
    }
    virtual bool IsTickableWhenPaused() const override
    {
        return true;
    }
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UXQPLANSubsystem, STATGROUP_Tickables);
    }
    UFUNCTION(BlueprintCallable) bool CreateRoom(const FString& Name);
    UFUNCTION(BlueprintCallable) void FindRooms();
    UFUNCTION(BlueprintCallable) bool JoinRoom(const FString& Code);
    // 直接使用发现快照中的地址、端口及房间身份握手，不再广播搜索联机码。
    bool JoinDiscoveredRoom(const FXQPLANRoom& Room);
    UFUNCTION(BlueprintCallable) void LeaveRoom();
    UFUNCTION(BlueprintCallable) void SetHostColor(EChessColor Color);
    UFUNCTION(BlueprintCallable) void SetReady(bool bReady);
    UFUNCTION(BlueprintCallable) bool StartMatch();
    UFUNCTION(BlueprintPure) EXQPRoomState GetState() const
    {
        return State;
    }
    // TCP 握手与未缓存联机码的定向查找均属于一次加入事务。
    // 普通房间浏览虽然也使用 Searching 状态，但不应阻止用户提前加入。
    bool IsJoiningRoom() const { return State == EXQPRoomState::Connecting || (State == EXQPRoomState::Searching && !PendingJoinCode.IsEmpty()); }
    UFUNCTION(BlueprintPure) FString GetStatus() const
    {
        return Status;
    }
    UFUNCTION(BlueprintPure) bool IsHost() const
    {
        return bHost;
    }
    UFUNCTION(BlueprintPure) bool IsSpectator() const { return bSpectator; }
    UFUNCTION(BlueprintPure) int32 GetSpectatorCount() const;
    UFUNCTION(BlueprintPure) bool HasPeer() const
    {
        return bHandshake;
    }
    UFUNCTION(BlueprintPure) bool IsPeerReady() const
    {
        return bPeerReady;
    }
    UFUNCTION(BlueprintPure) bool IsLocalReady() const
    {
        return bLocalReady;
    }
    UFUNCTION(BlueprintPure) EChessColor GetHostColor() const
    {
        return HostColor;
    }
    UFUNCTION(BlueprintPure) EChessColor GetLocalColor() const
    {
        if (bSpectator) return EChessColor::REDCHESS;
        return bHost ? HostColor : FChessPosition::Opponent(HostColor);
    }
    UFUNCTION(BlueprintPure) FString GetRedName() const;
    UFUNCTION(BlueprintPure) FString GetBlackName() const;
    UFUNCTION(BlueprintPure) FString GetLocalAddress() const;
    UFUNCTION(BlueprintPure) FString GetRoomCode() const { return State == EXQPRoomState::Creating ? FString() : RoomCode; }
    UFUNCTION(BlueprintPure) TArray<FXQPLANRoom> GetRooms() const
    {
        return Rooms;
    }
    bool CanLocalMove() const;
    bool SubmitMove(const FChessMove2P& Move);
    void NotifyBoardReady();
    void NotifyMatchFinished();
    bool PrepareSpectatorBoard(FChessPosition& Board, EChessColor& Side);
    void Resign();
    // 悔棋协商仅使用 Peer（对手）连接；观战连接不具备申请或答复权限。
    bool CanRequestUndo() const;
    bool RequestUndo();
    int32 GetRemainingUndoCount(EChessColor Color) const
    {
        const uint8 Side = static_cast<uint8>(Color);
        return Side < 2 ? UndoRemaining[Side] : 0;
    }
    bool IsUndoPending() const { return bUndoPending || bUndoAwaitingHost || PendingUndoSnapshot.IsValid(); }
    void ReplyToUndo(bool bAccept);
    UFUNCTION(BlueprintCallable) bool SelectGameType(EXQPGameType Type);
    UFUNCTION(BlueprintPure) EXQPGameType GetGameType() const { return GameType; }
    // 兼容已有蓝图调用；新玩法通过 SelectGameType 注册明确类型。
    UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use SelectGameType"))
    void SelectFengHuoMode(bool bSelected);
    bool IsFengHuoRoom() const { return GameType == EXQPGameType::FengHuo; }
    bool IsBattleReady() const { return !bSpectator && State == EXQPRoomState::Playing && bHandshake && bLocalBoardReady && bPeerBoardReady; }
    bool SubmitFengHuoMove(const FChessMove2P& Move);
    void BroadcastFengHuoState(const TSharedRef<FJsonObject>& Message);
    void ConfirmBattleStart();
    void BroadcastBattleStart(const TSharedRef<FJsonObject>& Message);
    DECLARE_MULTICAST_DELEGATE_OneParam(FModeMismatch, const FString&);
    FModeMismatch OnModeMismatch;
    bool DidPeerResign() const { return bPeerResigned; }
    bool DidLocalResign() const { return bLocalResigned; }
    // 结果弹窗在菜单或设置上方也能及时收到断线和再战准备变化。
    FSimpleMulticastDelegate OnRoomUpdated;

  private:
    // v4 同步双方悔棋机会，旧协议不理解次数限制，握手时明确拒绝混用。
    static constexpr int32 ProtocolVersion = 4;
    EXQPRoomState State = EXQPRoomState::Idle;
    FString Status;
    FString RoomName;
    FString RoomId;
    FString RoomCode;
    FString PendingJoinCode;
    FString JoinDiscoveryError;
    int32 JoinDiscoveryRetries = 0;
    double CreationDeadline = 0;
    double LastCodeClaim = 0;
    FString LocalName;
    FString PeerName;
    bool bHost = false;
    bool bSpectator = false;
    FString SpectatorRedName;
    FString SpectatorBlackName;
    TOptional<EChessColor> PendingSpectatorWinner;
    TArray<FXQPLANGuest> Guests;
    EXQPGameType GameType = EXQPGameType::Classic;
    int32 BoundGamePort = 0;
    int32 FengHuoRequestSerial = 0;
    int32 LastFengHuoPeerRequest = 0;
    TSharedPtr<FJsonObject> PendingProSnapshot;
    TSharedPtr<FJsonObject> PendingStartSnapshot;
    bool bHandshake = false;
    bool bPeerReady = false;
    bool bLocalReady = false;
    bool bLocalBoardReady = false;
    bool bPeerBoardReady = false;
    bool bMoveRequested = false;
    bool bPeerResigned = false;
    bool bLocalResigned = false;
    EChessColor HostColor = EChessColor::REDCHESS;
    EChessColor Turn = EChessColor::REDCHESS;
    FChessPosition AuthoritativeBoard;
    int32 Sequence = 0;
    double LastReceive = 0;
    double LastPing = 0;
    double SearchDeadline = 0;
    double LastDiscovery = 0;
    FSocket* Listener = nullptr;
    FSocket* Peer = nullptr;
    FSocket* Discovery = nullptr;
    // 当前搜索/建房周期的广播与热点网关目标，全部使用主机字节序。
    TArray<uint32> DiscoveryTargets;
    TArray<uint8> ReceiveBuffer;
    TArray<uint8> SendBuffer;
    TArray<FChessMove2P> PendingMoves;
    TArray<FXQPLANRoom> Rooms;
    bool bUndoPending = false;
    bool bUndoAwaitingHost = false;
    bool bUndoIncoming = false;
    int32 UndoSerial = 0;
    int32 UndoRemaining[2] = {5, 5};
    int32 UndoId = 0;
    int32 UndoSequence = 0;
    int32 UndoClientSerial = 0;
    EChessColor UndoRequester = EChessColor::REDCHESS;
    double UndoDeadline = 0.;
    TSharedPtr<FJsonObject> PendingUndoSnapshot;
    int32 PendingUndoPlayedPlies = 0;
    TWeakObjectPtr<class UUI_CommonDialog> UndoDialog;
    void ResetUndo();
    void ShowUndoDialog();
    bool StartUndo(EChessColor Requester, int32 ExpectedSequence);
    void ResolveUndo(bool bAccept);
    bool HandleUndoMessage(const TSharedPtr<FJsonObject>& Message, const FString& Type);
    void TickUndo(double Now);
    void CloseSockets();
    void Disconnect(const FString& Reason);
    void TickDiscovery(double Now);
    void BroadcastDiscovery(const FString& Payload);
    void TickConnection(double Now);
    bool ConnectToEndpoint(const FXQPLANRoom& Room);
    void Send(const TSharedRef<FJsonObject>& Message);
    void FlushSend();
    void HandleMessage(const TSharedPtr<FJsonObject>& Message);
    bool CommitMove(const FChessMove2P& Move, EChessColor Requester, int32 ExpectedSequence);
    void OpenBattleMap();
    void SendLobby();
    static bool QueueMessage(const TSharedRef<FJsonObject>& Message, TArray<uint8>& Buffer);
    static void FlushSocket(FSocket* Socket, TArray<uint8>& Buffer);
    void TickGuests(double Now);
    void CloseGuests();
    void SendSpectatorWelcome(FXQPLANGuest& Guest);
    void BroadcastSpectators(const TSharedRef<FJsonObject>& Message);
    bool ReceiveSpectatorWelcome(const TSharedPtr<FJsonObject>& Message);
};
