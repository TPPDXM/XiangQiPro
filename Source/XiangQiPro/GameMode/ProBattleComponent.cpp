#include "ProBattleComponent.h"
#include "XQPGameStateBase.h"
#include "BattleStartComponent.h"
#include "XiangQiPro/AI/ProBattleAI.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"

UProBattleComponent::UProBattleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UProBattleComponent::IsAuthority() const
{
    return !bNetwork || GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->IsHost();
}

EChessColor UProBattleComponent::GetLocalColor() const
{
    return bNetwork ? GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->GetLocalColor() : EChessColor::REDCHESS;
}

void UProBattleComponent::Reset()
{
    SetComponentTickEnabled(false);
    for (const auto& Actor : Actors) if (Actor.IsValid()) Actor->Destroy();
    Actors.Reset();
    Units.Reset();
    bActive = bRunning = bFinished = bDraw = bTimedOut = false;
    Elapsed = AITime = NetworkTime = 0.f;
    AIActAfter = 0.f;
    LastAIMove = {};
    RecentAIMoves.Reset();
    AISearch.Reset();
    ++PositionRevision;
    SnapshotSequence = 0;
    LastSnapshot = -1;
    SelectedType = EChessType::EMPTY;
    Stats[0] = Stats[1] = {};
}

void UProBattleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Reset();
    Super::EndPlay(Reason);
}

void UProBattleComponent::Start(const FProBattleSettings& InSettings, bool bInNetwork, bool bInAI)
{
    Reset();
    Settings = InSettings;
    Settings.Duration = FMath::Clamp(Settings.Duration, 30.f, 900.f);
    Settings.MaxSupply = FMath::Clamp(Settings.MaxSupply, 5, 20);
    Settings.InitialSupply = FMath::Clamp(Settings.InitialSupply, 0.f, float(Settings.MaxSupply));
    Settings.SupplyPerSecond = FMath::Clamp(Settings.SupplyPerSecond, 0.1f, 5.f);
    Settings.AIDifficulty = static_cast<EProAIDifficulty>(FMath::Clamp(static_cast<int32>(Settings.AIDifficulty), 0, 3));
    AIActAfter = FProBattleAI::Profile(Settings.AIDifficulty).ReactionSeconds;
    bNetwork = bInNetwork;
    bAI = bInAI;
    Supply[0] = Supply[1] = Settings.InitialSupply;
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !State->board2P) return;
    for (int32 X = 0; X < 10; ++X)
        for (int32 Y = 0; Y < 9; ++Y)
            if (auto Chess = State->board2P->GetChess(X, Y); Chess.IsValid())
            {
                FProBattleUnit Unit;
                Unit.Type = Chess->GetType();
                Unit.Color = Chess->GetColor();
                Unit.Square = Unit.From = Unit.To = {X, Y};
                Units.Add(Unit);
                Actors.Add(Chess);
                Chess->EnableProMode();
            }
    bActive = true;
    SetComponentTickEnabled(true);
}

FChessPosition UProBattleComponent::PositionSnapshot() const
{
    FChessPosition Board;
    Board.bRequireKing = false;
    Board.bFlyingKingCapture = false;
    for (const auto& Unit : Units)
        if (Unit.bAlive && !Unit.bMoving) Board.Set(Unit.Square, {Unit.Type, Unit.Color});
    return Board;
}

TArray<FChessMove2P> UProBattleComponent::GenerateMoves(AChesses* Chess) const
{
    TArray<FChessMove2P> Moves;
    const int32 Id = Actors.IndexOfByKey(Chess);
    if (!Units.IsValidIndex(Id) || !Units[Id].bAlive || Units[Id].bMoving || bFinished) return Moves;
    PositionSnapshot().GeneratePieceMoves(Units[Id].Square, Moves);
    Moves.RemoveAll([&](const FChessMove2P& Move)
    {
        return Units.ContainsByPredicate([&](const FProBattleUnit& U)
            { return U.bAlive && U.bMoving && U.Color == Units[Id].Color && U.To == Move.to; });
    });
    return Moves;
}

bool UProBattleComponent::CanMove(AChesses* Chess, const FChessMove2P& Move, EChessColor Requester) const
{
    if (!IsRunning() || !Cast<AXQPGameStateBase>(GetOwner())->BattleStart->HasStarted() || !Move.IsValid()) return false;
    const int32 Id = Actors.IndexOfByKey(Chess);
    if (!Units.IsValidIndex(Id)) return false;
    const auto& Unit = Units[Id];
    if (!Unit.bAlive || Unit.bMoving || Unit.Color != Requester || Unit.Square != Move.from ||
        Supply[ProBattle::Side(Requester)] + KINDA_SMALL_NUMBER < ProBattle::Rule(Unit.Type).Cost) return false;
    return GenerateMoves(Chess).ContainsByPredicate([&](const FChessMove2P& M) { return M.to == Move.to; });
}

bool UProBattleComponent::TryMove(AChesses* Chess, const FChessMove2P& Move, EChessColor Requester)
{
    if (!IsAuthority() || !CanMove(Chess, Move, Requester)) return false;
    const int32 Id = Actors.IndexOfByKey(Chess);
    auto& Unit = Units[Id];
    const int32 Side = ProBattle::Side(Requester);
    const int32 Cost = ProBattle::Rule(Unit.Type).Cost;
    Supply[Side] = FMath::Max(0.f, Supply[Side] - Cost);
    Stats[Side].SupplySpent += Cost;
    ++Stats[Side].Moves;
    if (bAI && Requester == EChessColor::BLACKCHESS)
    {
        AIActAfter = Elapsed + FProBattleAI::Profile(Settings.AIDifficulty).ReactionSeconds;
        LastAIMove = Move;
        RecentAIMoves.Add(Move);
        if (RecentAIMoves.Num() > 8) RecentAIMoves.RemoveAt(0);
    }
    Unit.From = Unit.Square;
    Unit.To = Move.to;
    Unit.DepartAt = Elapsed;
    Unit.ArriveAt = Elapsed + ProBattle::TravelSeconds(Unit.Type, Move.from, Move.to);
    Unit.bMoving = true;
    Unit.Support = INDEX_NONE;
    SyncBoard();
    Chess->BeginProMove(Move, Unit.ArriveAt - Elapsed);
    SendSnapshot();
    return true;
}

void UProBattleComponent::SyncBoard()
{
    ++PositionRevision;
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !State->board2P) return;
    for (auto& Row : State->board2P->AllChess) for (auto& Cell : Row) Cell.Reset();
    for (int32 Id = 0; Id < Units.Num(); ++Id)
    {
        const auto& Unit = Units[Id];
        if (Unit.bAlive && !Unit.bMoving)
            State->board2P->AllChess[Unit.Square.X][Unit.Square.Y] = Actors[Id];
    }
    State->board2P->MarkPositionChanged();
    RefreshSelectedMoves();
}

void UProBattleComponent::RefreshSelectedMoves()
{
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State) return;
    for (int32 Id = 0; Id < Actors.Num(); ++Id)
    {
        AChesses* Chess = Actors[Id].Get();
        if (!Chess || !Chess->IsSelectedForMove()) continue;
        // 棋局变化不会取消静止棋子的选择；即使暂时无路可走，也保留选择以便后续恢复。
        if (!bFinished && !State->IsGameOver() && Units[Id].bAlive && !Units[Id].bMoving &&
            Units[Id].Color == GetLocalColor())
        {
            SelectedType = Chess->GetType();
            State->ShowSettingPoint2P(GenerateMoves(Chess), Chess);
            return;
        }
        Chess->ClearMoveSelection();
    }
    SelectedType = EChessType::EMPTY;
    State->DismissSettingPoint2P();
}

void UProBattleComponent::ResolveArrival(int32 Id)
{
    auto& Unit = Units[Id];
    Unit.bMoving = false;
    Unit.Square = Unit.To;
    int32 Victim = INDEX_NONE;
    for (int32 Other = 0; Other < Units.Num(); ++Other)
        if (Other != Id && Units[Other].bAlive && !Units[Other].bMoving && Units[Other].Square == Unit.Square)
        { Victim = Other; break; }
    bool bKingCaptured = false;
    if (Units.IsValidIndex(Victim))
    {
        auto& Target = Units[Victim];
        // 同阵营目的地已在提交时预留，不允许后到的友军覆盖棋盘。
        if (Target.Color == Unit.Color) { Unit.Square = Unit.From; Unit.To = Unit.From; return; }
        Target.bAlive = false;
        Unit.Support = Victim;
        auto& SideStats = Stats[ProBattle::Side(Unit.Color)];
        SideStats.CaptureScore += ProBattle::Rule(Target.Type).Score;
        ++SideStats.Captures;
        bKingCaptured = Target.Type == EChessType::JIANG;
    }
    SyncBoard();
    if (Actors[Id].IsValid()) Actors[Id]->ArriveProMove(Unit.Square, Actors.IsValidIndex(Victim) ? Actors[Victim].Get() : nullptr);
    if (Actors.IsValidIndex(Victim) && Actors[Victim].IsValid()) Actors[Victim]->Defeated();
    if (bKingCaptured) Finish(Unit.Color);
}

void UProBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!bActive || bFinished || !IsAuthority()) return;
    if (!Cast<AXQPGameStateBase>(GetOwner())->BattleStart->HasStarted()) return;
    auto* Room = GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (bNetwork && !Room->IsBattleReady()) return;
    if (!bRunning) { bRunning = true; SendSnapshot(); }
    const float End = FMath::Min(Settings.Duration, Elapsed + DeltaTime);
    // 按实际到达时刻排序，同一帧中的多次吃子也遵循一致的先后关系。
    while (!bFinished)
    {
        int32 Next = INDEX_NONE;
        float Arrival = End + 1.f;
        for (int32 Id = 0; Id < Units.Num(); ++Id)
            if (Units[Id].bAlive && Units[Id].bMoving && Units[Id].ArriveAt <= End && Units[Id].ArriveAt < Arrival)
            { Next = Id; Arrival = Units[Id].ArriveAt; }
        if (Next == INDEX_NONE) break;
        RecoverSupply(Elapsed, Arrival);
        Elapsed = Arrival;
        ResolveArrival(Next);
    }
    if (bFinished) return;
    RecoverSupply(Elapsed, End);
    Elapsed = End;
    if (Elapsed >= Settings.Duration)
    {
        const int32 Red = Stats[0].CaptureScore, Black = Stats[1].CaptureScore;
        Finish(Red >= Black ? EChessColor::REDCHESS : EChessColor::BLACKCHESS, Red == Black, true);
        return;
    }
    AITime += DeltaTime;
    if (bAI && AISearch.IsSet() && AISearch->IsReady())
    {
        const auto Move = AISearch->Get();
        AISearch.Reset();
        // 棋子出发/抵达后旧结果失效；提交仍通过正式规则校验。
        if (SearchRevision == PositionRevision && Elapsed - SearchStartedAt <= .75f && Move.IsValid() && Elapsed >= AIActAfter)
        {
            const int32 Id = Units.IndexOfByPredicate([&](const FProBattleUnit& U)
                { return U.bAlive && !U.bMoving && U.Square == Move.from && U.Color == EChessColor::BLACKCHESS; });
            if (Actors.IsValidIndex(Id)) TryMove(Actors[Id].Get(), Move, EChessColor::BLACKCHESS);
        }
    }
    if (bAI && !AISearch.IsSet() && Elapsed >= AIActAfter && AITime >= FProBattleAI::Profile(Settings.AIDifficulty).ThinkInterval)
    {
        AITime = 0.f;
        SearchRevision = PositionRevision;
        SearchStartedAt = Elapsed;
        const auto Board = PositionSnapshot();
        const auto SnapshotUnits = Units;
        const auto History = RecentAIMoves;
        const auto Last = LastAIMove;
        const auto Difficulty = Settings.AIDifficulty;
        const float OwnSupply = Supply[1], OpponentSupply = Supply[0], Now = Elapsed, Remaining = GetRemaining();
        const float Rate = Settings.SupplyPerSecond * (IsDoubleSupply() ? 2.f : 1.f), Cap = Settings.MaxSupply;
        const int32 Seed = FMath::Rand();
        // 后台只持有值类型，切关/重开不访问已销毁组件，也不阻塞游戏线程。
        AISearch.Emplace(Async(EAsyncExecution::ThreadPool,
            [Board, SnapshotUnits, History, Last, Difficulty, OwnSupply, OpponentSupply, Now, Remaining, Rate, Cap, Seed]()
            {
                return FProBattleAI::ChooseMove(Board, SnapshotUnits, EChessColor::BLACKCHESS,
                    OwnSupply, OpponentSupply, Now, Remaining, Last, Seed, Rate, Difficulty, Cap, &History);
            }));
    }
    NetworkTime += DeltaTime;
    if (NetworkTime >= 0.05f) { NetworkTime = 0.f; SendSnapshot(); }
}

void UProBattleComponent::RecoverSupply(float From, float To)
{
    const float Threshold = FMath::Max(0.f, Settings.Duration - 30.f);
    const float Normal = FMath::Max(0.f, FMath::Min(To, Threshold) - From);
    const float Double = FMath::Max(0.f, To - FMath::Max(From, Threshold));
    for (float& Value : Supply)
        Value = FMath::Min(float(Settings.MaxSupply), Value + (Normal + Double * 2.f) * Settings.SupplyPerSecond);
}

void UProBattleComponent::Suspend()
{
    bRunning = false;
    SetComponentTickEnabled(false);
    for (const auto& Actor : Actors) if (Actor.IsValid()) Actor->StopProMove();
}

void UProBattleComponent::Finish(EChessColor InWinner, bool bInDraw, bool bInTimedOut)
{
    if (bFinished) return;
    bFinished = true;
    Winner = InWinner;
    bDraw = bInDraw;
    bTimedOut = bInTimedOut;
    for (const auto& Actor : Actors) if (Actor.IsValid()) Actor->StopProMove();
    SendSnapshot();
    if (auto* State = Cast<AXQPGameStateBase>(GetOwner())) State->FinishProBattle(InWinner);
}

TSharedRef<FJsonObject> UProBattleComponent::MakeSnapshot() const
{
    auto Message = MakeShared<FJsonObject>();
    Message->SetStringField(TEXT("type"), TEXT("pro_state"));
    Message->SetNumberField(TEXT("seq"), SnapshotSequence);
    Message->SetNumberField(TEXT("time"), Elapsed);
    Message->SetNumberField(TEXT("duration"), Settings.Duration);
    Message->SetNumberField(TEXT("max"), Settings.MaxSupply);
    Message->SetNumberField(TEXT("regen"), Settings.SupplyPerSecond);
    Message->SetBoolField(TEXT("running"), bRunning);
    Message->SetBoolField(TEXT("finished"), bFinished);
    Message->SetBoolField(TEXT("draw"), bDraw);
    Message->SetBoolField(TEXT("timeout"), bTimedOut);
    Message->SetNumberField(TEXT("winner"), ProBattle::Side(Winner));
    TArray<TSharedPtr<FJsonValue>> SideValues;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (double Number : {double(Supply[Side]), double(Stats[Side].CaptureScore), double(Stats[Side].Captures),
            double(Stats[Side].Moves), double(Stats[Side].SupplySpent)}) Fields.Add(MakeShared<FJsonValueNumber>(Number));
        SideValues.Add(MakeShared<FJsonValueArray>(Fields));
    }
    Message->SetArrayField(TEXT("sides"), SideValues);
    TArray<TSharedPtr<FJsonValue>> UnitValues;
    for (const auto& Unit : Units)
    {
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (double Number : {double(Unit.Type), double(Unit.Color), double(Unit.Square.X * 9 + Unit.Square.Y),
            double(Unit.From.X * 9 + Unit.From.Y), double(Unit.To.X * 9 + Unit.To.Y), double(Unit.DepartAt),
            double(Unit.ArriveAt), double(Unit.bAlive), double(Unit.bMoving), double(Unit.Support)})
            Fields.Add(MakeShared<FJsonValueNumber>(Number));
        UnitValues.Add(MakeShared<FJsonValueArray>(Fields));
    }
    Message->SetArrayField(TEXT("units"), UnitValues);
    return Message;
}

void UProBattleComponent::SendSnapshot()
{
    if (!bNetwork || !IsAuthority()) return;
    ++SnapshotSequence;
    GetWorld()->GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->BroadcastProState(MakeSnapshot());
}

bool UProBattleComponent::ReceiveSnapshot(const TSharedPtr<FJsonObject>& M)
{
    if (!bActive || !bNetwork || IsAuthority() || !M) return false;
    double Seq, Time, Duration, Max, Regen, Win;
    bool Running, Finished, Draw, Timeout;
    const TArray<TSharedPtr<FJsonValue>> *SideValues, *UnitValues;
    if (!M->TryGetNumberField(TEXT("seq"), Seq) || !M->TryGetNumberField(TEXT("time"), Time) ||
        !M->TryGetNumberField(TEXT("duration"), Duration) || !M->TryGetNumberField(TEXT("max"), Max) ||
        !M->TryGetNumberField(TEXT("regen"), Regen) || !M->TryGetNumberField(TEXT("winner"), Win) ||
        !M->TryGetBoolField(TEXT("running"), Running) || !M->TryGetBoolField(TEXT("finished"), Finished) ||
        !M->TryGetBoolField(TEXT("draw"), Draw) || !M->TryGetBoolField(TEXT("timeout"), Timeout) ||
        !M->TryGetArrayField(TEXT("sides"), SideValues) || !M->TryGetArrayField(TEXT("units"), UnitValues) ||
        SideValues->Num() != 2 || UnitValues->Num() != Units.Num()) return false;
    if (!FMath::IsFinite(Time) || !FMath::IsFinite(Duration) || Duration < 30 || Duration > 900 || Time < 0 || Time > Duration ||
        Max < 5 || Max > 20 || Regen < 0.1 || Regen > 5 || Win < 0 || Win > 1 || Seq > MAX_int32 || Seq < 0) return false;
    if (Seq <= LastSnapshot) return true;
    TArray<FProBattleUnit> NewUnits = Units;
    FProBattleStats NewStats[2];
    float NewSupply[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const TArray<TSharedPtr<FJsonValue>>* Fields;
        if (!(*SideValues)[Side]->TryGetArray(Fields) || Fields->Num() != 5) return false;
        double V[5];
        for (int32 I = 0; I < 5; ++I)
            if (!(*Fields)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I]) || V[I] < 0 || V[I] > 1000000) return false;
        if (V[0] > Max) return false;
        NewSupply[Side] = V[0];
        NewStats[Side].CaptureScore = V[1]; NewStats[Side].Captures = V[2];
        NewStats[Side].Moves = V[3]; NewStats[Side].SupplySpent = V[4];
    }
    for (int32 Id = 0; Id < Units.Num(); ++Id)
    {
        const TArray<TSharedPtr<FJsonValue>>* Fields;
        if (!(*UnitValues)[Id]->TryGetArray(Fields) || Fields->Num() != 10) return false;
        double V[10];
        for (int32 I = 0; I < 10; ++I)
            if (!(*Fields)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I])) return false;
        if (V[0] != int32(Units[Id].Type) || V[1] != int32(Units[Id].Color)) return false;
        for (int32 I : {2, 3, 4}) if (V[I] < 0 || V[I] > 89 || FMath::FloorToDouble(V[I]) != V[I]) return false;
        if (V[5] < 0 || V[6] < V[5] || V[6] > Duration + 20 || (V[7] != 0 && V[7] != 1) ||
            (V[8] != 0 && V[8] != 1) || V[9] < -1 || V[9] >= Units.Num() || V[9] == Id) return false;
        auto& U = NewUnits[Id];
        U.Square = {int32(V[2]) / 9, int32(V[2]) % 9};
        U.From = {int32(V[3]) / 9, int32(V[3]) % 9};
        U.To = {int32(V[4]) / 9, int32(V[4]) % 9};
        U.DepartAt = V[5]; U.ArriveAt = V[6]; U.bAlive = V[7] != 0; U.bMoving = V[8] != 0; U.Support = V[9];
    }
    LastSnapshot = Seq;
    Elapsed = Time;
    Settings.Duration = Duration; Settings.MaxSupply = Max; Settings.SupplyPerSecond = Regen;
    bRunning = Running;
    for (int32 Side = 0; Side < 2; ++Side) { Stats[Side] = NewStats[Side]; Supply[Side] = NewSupply[Side]; }
    // 先建立叠放关系，再触发被吃方的消散，短时连续吃子也可保持高度链。
    for (int32 Id = 0; Id < Units.Num(); ++Id)
    {
        if (!Actors[Id].IsValid()) continue;
        const auto& U = NewUnits[Id];
        const auto& Old = Units[Id];
        if (U.bMoving && (!Old.bMoving || U.DepartAt != Old.DepartAt))
            Actors[Id]->BeginProMove({U.From, U.To}, FMath::Max(0.01f, U.ArriveAt - float(Time)));
        if (!U.bMoving && (Old.bMoving || Old.Square != U.Square || Old.Support != U.Support))
        {
            Actors[Id]->ArriveProMove(U.Square, Actors.IsValidIndex(U.Support) ? Actors[U.Support].Get() : nullptr);
        }
    }
    for (int32 Id = 0; Id < Units.Num(); ++Id)
        if (Units[Id].bAlive && !NewUnits[Id].bAlive && Actors[Id].IsValid()) Actors[Id]->Defeated();
    Units = MoveTemp(NewUnits);
    SyncBoard();
    if (Finished) Finish(static_cast<EChessColor>(int32(Win)), Draw, Timeout);
    return true;
}
