#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "BattleStartComponent.generated.h"
class FJsonObject;

UCLASS()
class XIANGQIPRO_API UBattleStartComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBattleStartComponent();
    void BeginSetup();
    void ConfirmLocal();
    void ConfirmSide(EChessColor Side);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    bool HasStarted() const { return bRunning; }
    bool IsConfirmed(EChessColor Side) const { return Side == EChessColor::REDCHESS ? bRedReady : bBlackReady; }
    float GetCountdown() const { return Countdown; }
    bool IsActive() const { return bActive; }
    FSimpleMulticastDelegate OnSetupChanged;
    void Stop() { bActive = bRunning = false; }
    TSharedRef<FJsonObject> MakeSnapshot() const;
    bool ReceiveSnapshot(const TSharedPtr<FJsonObject>& Message);
private:
    bool bActive = false;
    bool bRunning = false;
    bool bRedReady = false;
    bool bBlackReady = false;
    float Countdown = -1.f;
    float SetupTime = 0.f;
    float SendTime = 0.f;
    /** 每局只允许 3→2→1 单向推进；联机快照回调或校时不能重复敲同一个数字。 */
    int32 LastAudibleCountdown = 4;
    void UpdateCountdownAudio();
    bool IsAuthority() const;
    void Broadcast();
    void StartBattle();
};
