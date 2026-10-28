#include "BattleStartComponent.h"
#include "XQPGameStateBase.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Dom/JsonObject.h"

UBattleStartComponent::UBattleStartComponent() { PrimaryComponentTick.bCanEverTick = true; }

bool UBattleStartComponent::IsAuthority() const
{
    const auto* State = Cast<AXQPGameStateBase>(GetOwner());
    return State && (!State->IsLANGame() || GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->IsHost());
}

void UBattleStartComponent::BeginSetup()
{
    bActive = true;
    bRunning = bRedReady = bBlackReady = false;
    Countdown = -1.f;
    SetupTime = SendTime = 0.f;
    const auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (State && !State->IsProBattle() && !State->IsLANGame())
    {
        bRedReady = bBlackReady = true;
        StartBattle();
        return;
    }
    OnSetupChanged.Broadcast();
    Broadcast();
}

void UBattleStartComponent::ConfirmLocal()
{
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !bActive || bRunning || State->IsGameOver()) return;
    if (State->IsLANGame()) GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->ConfirmBattleStart();
    else ConfirmSide(EChessColor::REDCHESS);
}

void UBattleStartComponent::ConfirmSide(EChessColor Side)
{
    if (!bActive || bRunning || !IsAuthority() || static_cast<uint8>(Side) > 1) return;
    (Side == EChessColor::REDCHESS ? bRedReady : bBlackReady) = true;
    if (bRedReady && bBlackReady && Countdown < 0.f)
    {
        if (Cast<AXQPGameStateBase>(GetOwner())->IsProBattle()) Countdown = 3.f;
        else { StartBattle(); return; }
    }
    OnSetupChanged.Broadcast();
    Broadcast();
}

void UBattleStartComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!bActive || bRunning) return;
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || State->IsGameOver()) return;
    if (!IsAuthority())
    {
        if (Countdown > 0.f) Countdown = FMath::Max(0.001f, Countdown - DeltaTime);
        return;
    }
    if (State->IsLANGame() && !GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->IsBattleReady()) return;
    const bool WasCounting = Countdown >= 0.f;
    SetupTime += DeltaTime;
    if (!State->IsLANGame() && !bBlackReady && SetupTime >= 0.6f) ConfirmSide(EChessColor::BLACKCHESS);
    if (WasCounting)
    {
        Countdown = FMath::Max(0.f, Countdown - DeltaTime);
        if (Countdown <= 0.f)
        {
            StartBattle();
        }
    }
    SendTime += DeltaTime;
    if (SendTime >= 0.1f) { SendTime = 0.f; Broadcast(); }
}

void UBattleStartComponent::StartBattle()
{
    Countdown = 0.f;
    bRunning = true;
    OnSetupChanged.Broadcast();
    Broadcast();
    Cast<AXQPGameStateBase>(GetOwner())->OnBattleStarted();
}

TSharedRef<FJsonObject> UBattleStartComponent::MakeSnapshot() const
{
    auto M = MakeShared<FJsonObject>();
    M->SetStringField(TEXT("type"), TEXT("battle_start_state"));
    M->SetBoolField(TEXT("red"), bRedReady); M->SetBoolField(TEXT("black"), bBlackReady);
    M->SetBoolField(TEXT("running"), bRunning); M->SetNumberField(TEXT("countdown"), Countdown);
    return M;
}

void UBattleStartComponent::Broadcast()
{
    if (IsAuthority() && Cast<AXQPGameStateBase>(GetOwner())->IsLANGame())
        GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->BroadcastBattleStart(MakeSnapshot());
}

bool UBattleStartComponent::ReceiveSnapshot(const TSharedPtr<FJsonObject>& M)
{
    if (!M || !bActive || IsAuthority()) return false;
    bool Red, Black, Running; double Time;
    if (!M->TryGetBoolField(TEXT("red"), Red) || !M->TryGetBoolField(TEXT("black"), Black) ||
        !M->TryGetBoolField(TEXT("running"), Running) || !M->TryGetNumberField(TEXT("countdown"), Time) ||
        !FMath::IsFinite(Time) || Time < -1.f || Time > 3.f || (Running && (!Red || !Black || Time > 0.f))) return false;
    const bool WasRunning = bRunning;
    bRedReady = Red; bBlackReady = Black; bRunning = Running; Countdown = Time;
    OnSetupChanged.Broadcast();
    if (Running && !WasRunning) Cast<AXQPGameStateBase>(GetOwner())->OnBattleStarted();
    return true;
}
