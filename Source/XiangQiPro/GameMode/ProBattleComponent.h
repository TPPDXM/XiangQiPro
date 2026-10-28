#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProBattleTypes.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "ProBattleComponent.generated.h"

class AChesses;
class FJsonObject;

UCLASS(ClassGroup=(XiangQi), meta=(BlueprintSpawnableComponent))
class XIANGQIPRO_API UProBattleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UProBattleComponent();
    void Start(const FProBattleSettings& InSettings, bool bInNetwork, bool bInAI);
    void Reset();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    bool TryMove(AChesses* Chess, const FChessMove2P& Move, EChessColor Requester);
    bool CanMove(AChesses* Chess, const FChessMove2P& Move, EChessColor Requester) const;
    TArray<FChessMove2P> GenerateMoves(AChesses* Chess) const;
    bool IsActive() const { return bActive; }
    bool IsRunning() const { return bRunning && !bFinished; }
    bool IsNetwork() const { return bNetwork; }
    bool IsAuthority() const;
    bool IsDraw() const { return bDraw; }
    bool IsTimedOut() const { return bTimedOut; }
    bool IsDoubleSupply() const { return bRunning && Elapsed >= FMath::Max(0.f, Settings.Duration - 30.f); }
    float GetSupply(EChessColor Color) const { return Supply[ProBattle::Side(Color)]; }
    const FProBattleStats& GetStats(EChessColor Color) const { return Stats[ProBattle::Side(Color)]; }
    const FProBattleSettings& GetSettings() const { return Settings; }
    float GetRemaining() const { return FMath::Max(0.f, Settings.Duration - Elapsed); }
    float GetElapsed() const { return Elapsed; }
    EChessColor GetLocalColor() const;
    EChessType SelectedType = EChessType::EMPTY;
    void Finish(EChessColor Winner, bool bInDraw = false, bool bInTimedOut = false);
    void Suspend();
    TSharedRef<FJsonObject> MakeSnapshot() const;
    bool ReceiveSnapshot(const TSharedPtr<FJsonObject>& Message);
private:
    FProBattleSettings Settings;
    FProBattleStats Stats[2];
    float Supply[2] = {};
    TArray<FProBattleUnit> Units;
    TArray<TWeakObjectPtr<AChesses>> Actors;
    bool bActive = false;
    bool bRunning = false;
    bool bNetwork = false;
    bool bAI = false;
    bool bFinished = false;
    bool bDraw = false;
    bool bTimedOut = false;
    EChessColor Winner = EChessColor::REDCHESS;
    float Elapsed = 0.f;
    float AITime = 0.f;
    float NetworkTime = 0.f;
    float AIActAfter = 0.f;
    FChessMove2P LastAIMove;
    int32 SnapshotSequence = 0;
    int32 LastSnapshot = -1;
    FChessPosition PositionSnapshot() const;
    void ResolveArrival(int32 Id);
    void SyncBoard();
    void RefreshSelectedMoves();
    void SendSnapshot();
    void RecoverSupply(float From, float To);
};
