#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "XiangQiPro/AI/ChessPosition.h"
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
    Disconnected
};

USTRUCT(BlueprintType)
struct FXQPLANRoom
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString HostName;
    UPROPERTY(BlueprintReadOnly) FString Address;
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
    UFUNCTION(BlueprintCallable) bool JoinRoom(const FString& Address);
    UFUNCTION(BlueprintCallable) void LeaveRoom();
    UFUNCTION(BlueprintCallable) void SetHostColor(EChessColor Color);
    UFUNCTION(BlueprintCallable) void SetReady(bool bReady);
    UFUNCTION(BlueprintCallable) bool StartMatch();
    UFUNCTION(BlueprintPure) EXQPRoomState GetState() const
    {
        return State;
    }
    UFUNCTION(BlueprintPure) FString GetStatus() const
    {
        return Status;
    }
    UFUNCTION(BlueprintPure) bool IsHost() const
    {
        return bHost;
    }
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
        return bHost ? HostColor : FChessPosition::Opponent(HostColor);
    }
    UFUNCTION(BlueprintPure) FString GetRedName() const;
    UFUNCTION(BlueprintPure) FString GetBlackName() const;
    UFUNCTION(BlueprintPure) FString GetLocalAddress() const;
    UFUNCTION(BlueprintPure) TArray<FXQPLANRoom> GetRooms() const
    {
        return Rooms;
    }
    bool CanLocalMove() const;
    bool SubmitMove(const FChessMove2P& Move);
    void NotifyBoardReady();
    void NotifyMatchFinished();
    void Resign();
    UFUNCTION(BlueprintCallable) void SelectProMode(bool bSelected);
    bool IsProRoom() const { return bProRoom; }
    bool IsBattleReady() const { return State == EXQPRoomState::Playing && bHandshake && bLocalBoardReady && bPeerBoardReady; }
    bool SubmitProMove(const FChessMove2P& Move);
    void BroadcastProState(const TSharedRef<FJsonObject>& Message);
    void ConfirmBattleStart();
    void BroadcastBattleStart(const TSharedRef<FJsonObject>& Message);
    DECLARE_MULTICAST_DELEGATE_OneParam(FModeMismatch, const FString&);
    FModeMismatch OnModeMismatch;
    bool DidPeerResign() const { return bPeerResigned; }
    bool DidLocalResign() const { return bLocalResigned; }
    // 结果弹窗在菜单或设置上方也能及时收到断线和再战准备变化。
    FSimpleMulticastDelegate OnRoomUpdated;

  private:
    EXQPRoomState State = EXQPRoomState::Idle;
    FString Status;
    FString RoomName;
    FString LocalName;
    FString PeerName;
    bool bHost = false;
    bool bProRoom = false;
    int32 ProRequestSerial = 0;
    int32 LastProPeerRequest = 0;
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
    TArray<uint8> ReceiveBuffer;
    TArray<uint8> SendBuffer;
    TArray<FChessMove2P> PendingMoves;
    TArray<FXQPLANRoom> Rooms;
    void CloseSockets();
    void Disconnect(const FString& Reason);
    void TickDiscovery(double Now);
    void TickConnection(double Now);
    void Send(const TSharedRef<FJsonObject>& Message);
    void FlushSend();
    void HandleMessage(const TSharedPtr<FJsonObject>& Message);
    bool CommitMove(const FChessMove2P& Move, EChessColor Requester, int32 ExpectedSequence);
    void OpenBattleMap();
    void SendLobby();
};
