#include "ProBattleAI.h"

namespace
{
constexpr float WinScore = 100000.f;

struct FPlan
{
    FChessMove2P Move;
    int32 Unit = INDEX_NONE;
    float Wait = 0.f;
    float Score = 0.f;
};

// 出发即离开棋盘，抵达才吃子，同刻按稳定棋子编号结算。
struct FForecast
{
    TArray<FProBattleUnit, TInlineAllocator<32>> Units;
    float Supply[2] = {};
    float Time = 0.f;
    float StartTime = 0.f;
    float End = 0.f;
    float DoubleAt = 0.f;
    float Rate = 0.f;
    float MaxSupply = 10.f;
    float Reaction = .28f;
    int32 Winner = INDEX_NONE;
    bool bVariantRules = false;

    FChessPosition Board() const
    {
        FChessPosition Result;
        Result.bRequireKing = false;
        Result.bFlyingKingCapture = false;
        Result.bVariantRules = bVariantRules;
        for (const auto& U : Units)
            if (U.bAlive && !U.bMoving) Result.Set(U.Square, {U.Type, U.Color});
        return Result;
    }

    void Recover(float To)
    {
        const float Normal = FMath::Max(0.f, FMath::Min(To, DoubleAt) - Time);
        const float Double = FMath::Max(0.f, To - FMath::Max(Time, DoubleAt));
        for (float& Value : Supply) Value = FMath::Min(MaxSupply, Value + Rate * (Normal + Double * 2.f));
        Time = To;
    }

    void Advance(float To)
    {
        To = FMath::Clamp(To, Time, End);
        while (Winner == INDEX_NONE)
        {
            int32 Next = INDEX_NONE;
            float Arrival = To + 1.f;
            for (int32 I = 0; I < Units.Num(); ++I)
                if (Units[I].bAlive && Units[I].bMoving && Units[I].ArriveAt <= To && Units[I].ArriveAt < Arrival)
                { Next = I; Arrival = Units[I].ArriveAt; }
            if (Next == INDEX_NONE) break;
            Recover(FMath::Max(Time, Arrival));
            auto& U = Units[Next];
            U.bMoving = false;
            U.Square = U.To;
            for (int32 I = 0; I < Units.Num(); ++I)
            {
                auto& Target = Units[I];
                if (I == Next || !Target.bAlive || Target.bMoving || Target.Square != U.Square) continue;
                if (Target.Color == U.Color) { U.Square = U.To = U.From; break; }
                Target.bAlive = false;
                if (Target.Type == EChessType::JIANG) Winner = ProBattle::Side(U.Color);
                break;
            }
        }
        if (Winner == INDEX_NONE) Recover(To);
    }

    void Settle()
    {
        float Latest = Time;
        for (const auto& U : Units) if (U.bAlive && U.bMoving) Latest = FMath::Max(Latest, U.ArriveAt);
        Advance(Latest);
    }

    float WaitFor(EChessColor Side, int32 Cost) const
    {
        if (Cost > MaxSupply) return BIG_NUMBER;
        const float Needed = FMath::Max(0.f, Cost - Supply[ProBattle::Side(Side)]);
        const float NormalTime = FMath::Max(0.f, DoubleAt - Time);
        if (Needed <= NormalTime * Rate) return Needed / Rate;
        return NormalTime + (Needed - NormalTime * Rate) / (Rate * 2.f);
    }

    bool Launch(const FPlan& Plan)
    {
        Advance(Time + Plan.Wait);
        if (Winner != INDEX_NONE || Time >= End || !Units.IsValidIndex(Plan.Unit)) return false;
        auto& U = Units[Plan.Unit];
        const int32 Cost = ProBattle::Rule(U.Type).Cost;
        if (!U.bAlive || U.bMoving || U.Square != Plan.Move.from ||
            Supply[ProBattle::Side(U.Color)] + KINDA_SMALL_NUMBER < Cost || !Board().CanReach(U.Square, Plan.Move.to)) return false;
        for (const auto& Other : Units)
            if (Other.bAlive && Other.bMoving && Other.Color == U.Color && Other.To == Plan.Move.to) return false;
        U.From = U.Square;
        U.To = Plan.Move.to;
        U.DepartAt = Time;
        U.ArriveAt = Time + ProBattle::TravelSeconds(U.Type, U.From, U.To);
        U.bMoving = true;
        Supply[ProBattle::Side(U.Color)] = FMath::Max(0.f, Supply[ProBattle::Side(U.Color)] - Cost);
        return true;
    }
};

float Material(EChessType Type)
{
    return Type == EChessType::JIANG ? WinScore : ProBattle::Rule(Type).Score * 100.f;
}

// 机动、过河、车线和逼近九宫给出持续的进攻目标，不能只奖励眼前吃子。
float Evaluate(const FForecast& State, EChessColor Side)
{
    if (State.Winner != INDEX_NONE)
    {
        const float Value = WinScore - FMath::Min(1000.f, (State.Time - State.StartTime) * 20.f);
        return State.Winner == ProBattle::Side(Side) ? Value : -Value;
    }
    const auto Board = State.Board();
    float Values[2] = {0.f, 0.f};
    for (const auto& U : State.Units)
    {
        if (!U.bAlive) continue;
        const int32 S = ProBattle::Side(U.Color);
        Values[S] += Material(U.Type);
        if (U.bMoving) continue;
        const int32 Rank = U.Color == EChessColor::REDCHESS ? U.Square.X : 9 - U.Square.X;
        const int32 Center = 4 - FMath::Abs(U.Square.Y - 4);
        TArray<FChessMove2P> Moves;
        Board.GeneratePieceMoves(U.Square, Moves);
        const bool Major = U.Type == EChessType::JV || U.Type == EChessType::MA || U.Type == EChessType::PAO;
        if (Major)
        {
            Values[S] += Moves.Num() * (U.Type == EChessType::JV ? 4.f : 3.f) + Center * 5.f + FMath::Min(Rank, 7) * 9.f;
            const auto King = Board.FindKing(FChessPosition::Opponent(U.Color));
            if (FChessPosition::IsInside(King))
                Values[S] += 5.f * (13 - FMath::Abs(U.Square.X - King.X) - FMath::Abs(U.Square.Y - King.Y));
        }
        else if (U.Type == EChessType::BING)
            Values[S] += Rank * 8.f + (Rank >= 5 ? 30.f + Center * 7.f : 0.f);
        else if (U.Type == EChessType::JIANG)
        {
            Values[S] -= Rank * 12.f;
            int32 CloseAttackers = 0;
            for (const auto& Enemy : State.Units)
            {
                if (!Enemy.bAlive || Enemy.bMoving || Enemy.Color == U.Color ||
                    (Enemy.Type != EChessType::JV && Enemy.Type != EChessType::PAO && Enemy.Type != EChessType::MA)) continue;
                if (FMath::Abs(Enemy.Square.X - U.Square.X) + FMath::Abs(Enemy.Square.Y - U.Square.Y) <= 5)
                    ++CloseAttackers;
            }
            // 敌军压近九宫时给逃将预留两点，避免吃小子后失去最后的机动能力。
            Values[S] -= FMath::Max(0.f, 2.f - State.Supply[S]) * CloseAttackers * 65.f;
            if (Moves.IsEmpty()) Values[S] -= 100.f;
        }

        const float Wait = State.WaitFor(U.Color, ProBattle::Rule(U.Type).Cost);
        float BestThreat = 0.f, OtherThreats = 0.f;
        for (const auto& M : Moves)
        {
            const auto Target = Board.Get(M.to);
            if (Target.Type == EChessType::EMPTY) continue;
            const float Travel = ProBattle::TravelSeconds(U.Type, M.from, M.to);
            if (State.Time + Wait + Travel >= State.End) continue;
            const bool CanEscape = State.WaitFor(Target.Color, ProBattle::Rule(Target.Type).Cost) < Wait + Travel;
            const float Threat = (Target.Type == EChessType::JIANG ? (CanEscape ? 850.f : 5000.f) : Material(Target.Type) * .32f)
                / (1.f + Wait * .28f + Travel * .6f);
            BestThreat = FMath::Max(BestThreat, Threat);
            OtherThreats += Threat;
        }
        // 双重攻击有价值，但不把同一棋子的每条吃子线都算作已经获利。
        Values[S] += BestThreat * .8f + OtherThreats * .2f;
    }
    // 粮草是节奏而非目的，适量计价避免因移动扣分而长时间不出兵。
    for (int32 S = 0; S < 2; ++S) Values[S] += State.Supply[S] * 7.f;
    return Values[ProBattle::Side(Side)] - Values[1 - ProBattle::Side(Side)];
}

TArray<FPlan> Plans(const FForecast& State, EChessColor Side, float MaxWait)
{
    TArray<FPlan> Result;
    const auto Board = State.Board();
    for (int32 I = 0; I < State.Units.Num(); ++I)
    {
        const auto& U = State.Units[I];
        if (!U.bAlive || U.bMoving || U.Color != Side) continue;
        const float Wait = State.WaitFor(Side, ProBattle::Rule(U.Type).Cost);
        if (Wait > MaxWait) continue;
        TArray<FChessMove2P> Moves;
        Board.GeneratePieceMoves(U.Square, Moves);
        for (const auto& Move : Moves)
        {
            if (State.Time + Wait + ProBattle::TravelSeconds(U.Type, Move.from, Move.to) >= State.End) continue;
            if (State.Units.ContainsByPredicate([&](const FProBattleUnit& Other)
                { return Other.bAlive && Other.bMoving && Other.Color == Side && Other.To == Move.to; })) continue;
            FPlan Plan;
            Plan.Move = Move;
            Plan.Unit = I;
            Plan.Wait = Wait;
            Result.Add(Plan);
        }
    }
    return Result;
}

float ForecastScore(const FForecast& State, const FPlan& Plan, EChessColor Side)
{
    FForecast Future = State;
    if (!Future.Launch(Plan)) return -WinScore * 2.f;
    Future.Settle();
    return Evaluate(Future, Side);
}

float RespondToAttack(const FForecast& Attack, EChessColor Side, FForecast& BestState)
{
    FForecast Settled = Attack;
    Settled.Settle();
    BestState = Settled;
    float Best = Evaluate(Settled, Side);
    // 对手新发出的攻击不是瞬间吃子；我方仍有一次按实际反应速度逃离的机会。
    // 已经在途的攻击由根节点处理，不能借此替不动分支自动逃将。
    FForecast Ready = Attack;
    Ready.Advance(Ready.Time + Ready.Reaction);
    if (Ready.Winner != INDEX_NONE) return Best;
    const auto Defences = Plans(Ready, Side, 0.f);
    for (const auto& Defence : Defences)
    {
        const auto& U = Ready.Units[Defence.Unit];
        const bool Threatened = Ready.Units.ContainsByPredicate([&](const FProBattleUnit& Enemy)
            { return Enemy.bAlive && Enemy.bMoving && Enemy.Color != Side && Enemy.To == U.Square; });
        if (!Threatened) continue;
        FForecast Escape = Ready;
        if (!Escape.Launch(Defence)) continue;
        Escape.Settle();
        const float Value = Evaluate(Escape, Side);
        if (Value > Best) { Best = Value; BestState = Escape; }
    }
    return Best;
}

// 对手可以在我方出发后立即逃离或抢攻，也可以等抵达后反吃。
// 完整比较单步应手，不能让窄束搜索漏掉吃将。
float CounterScore(const FForecast& State, const FPlan& Plan, EChessColor Side, FForecast* WorstState = nullptr)
{
    FForecast Flight = State;
    if (Plan.Move.IsValid() && !Flight.Launch(Plan)) return -WinScore;
    FForecast Landed = Flight;
    Landed.Settle();
    const float Idle = Evaluate(Landed, Side);
    const auto Enemy = FChessPosition::Opponent(Side);
    float Worst = Idle;
    if (WorstState) *WorstState = Landed;
    for (int32 Phase = 0; Phase < 2; ++Phase)
    {
        const FForecast& Base = Phase == 0 ? Flight : Landed;
        if (Base.Winner != INDEX_NONE) continue;
        const auto Replies = Plans(Base, Enemy, Phase == 0 ? 2.5f : .5f);
        for (const auto& Reply : Replies)
        {
            FForecast Response = Base;
            if (!Response.Launch(Reply)) continue;
            FForecast Outcome;
            const float Value = RespondToAttack(Response, Side, Outcome);
            if (Value < Worst)
            {
                Worst = Value;
                if (WorstState) *WorstState = Outcome;
            }
        }
    }
    if (Worst <= -WinScore * .9f) return Worst;
    // 保留施压收益，同时重点规避最强反击。
    return FMath::Lerp(Idle, Worst, .78f);
}

float RepeatPenalty(const FPlan& Plan, const FChessMove2P& Last, const TArray<FChessMove2P>* History, const FChessPosition& Board)
{
    if (Board.Get(Plan.Move.to).Type != EChessType::EMPTY) return 0.f;
    float Penalty = Last.to == Plan.Move.from && Last.from == Plan.Move.to ? 40.f : 0.f;
    if (History)
        for (const auto& Move : *History)
            if (Move.from == Plan.Move.from && Move.to == Plan.Move.to) Penalty += 20.f;
    return Penalty;
}
}

FProAIProfile FProBattleAI::Profile(EProAIDifficulty Difficulty)
{
    switch (Difficulty)
    {
    case EProAIDifficulty::Easy:   return {1.15f, .25f, 4, 0, 12.f};
    case EProAIDifficulty::Normal: return {.65f, .18f, 7, 0, 5.f};
    case EProAIDifficulty::Expert: return {.12f, .10f, 32, 3, 0.f};
    default:                     return {.28f, .12f, 20, 2, 1.f};
    }
}

FChessMove2P FProBattleAI::ChooseMove(const FChessPosition& Board, const TArray<FProBattleUnit>& Units,
    EChessColor Side, float Supply, float EnemySupply, float Now, float Remaining,
    const FChessMove2P& LastMove, int32 RandomSeed, float RecoveryRate, EProAIDifficulty Difficulty,
    float MaxSupply, const TArray<FChessMove2P>* RecentMoves)
{
    if (Remaining <= 0.f || (Side != EChessColor::REDCHESS && Side != EChessColor::BLACKCHESS)) return {};
    const auto Settings = Profile(Difficulty);
    FForecast State;
    State.Units.Append(Units);
    State.Time = Now;
    State.StartTime = Now;
    State.End = Now + Remaining;
    State.DoubleAt = State.End - 30.f;
    State.Rate = FMath::Max(.001f, RecoveryRate) * (Remaining <= 30.f ? .5f : 1.f);
    State.MaxSupply = FMath::Max(0.f, MaxSupply);
    State.Reaction = Settings.ReactionSeconds;
    State.Supply[ProBattle::Side(Side)] = FMath::Clamp(Supply, 0.f, State.MaxSupply);
    State.Supply[1 - ProBattle::Side(Side)] = FMath::Clamp(EnemySupply, 0.f, State.MaxSupply);
    State.bVariantRules = Board.bVariantRules;
    const float Baseline = Evaluate(State, Side);
    auto Candidates = Plans(State, Side, FMath::Min(20.f, Remaining));
    for (auto& Plan : Candidates)
    {
        const float Gain = ForecastScore(State, Plan, Side) - Baseline;
        Plan.Score = FMath::Abs(Gain) > WinScore * .8f ? Gain :
            Gain / (1.f + Plan.Wait * .12f) - Plan.Wait * 1.5f - RepeatPenalty(Plan, LastMove, RecentMoves, Board);
    }
    Candidates.Sort([](const FPlan& A, const FPlan& B) { return A.Score > B.Score; });

    const auto King = Board.FindKing(Side);
    const bool KingUnderAttack = FChessPosition::IsInside(King) && Units.ContainsByPredicate([&](const FProBattleUnit& U)
        { return U.bAlive && U.bMoving && U.Color != Side && U.To == King; });
    if (KingUnderAttack)
    {
        // 已提交的吃将必须先避开；远期假想反击不能使AI放弃眼前仍可生还的逃将。
        float BestDefence = -BIG_NUMBER;
        FChessMove2P Escape;
        for (const auto& Plan : Candidates)
        {
            if (Plan.Wait > KINDA_SMALL_NUMBER || ForecastScore(State, Plan, Side) < -WinScore * .8f) continue;
            const float Score = CounterScore(State, Plan, Side);
            if (Score > BestDefence) { BestDefence = Score; Escape = Plan.Move; }
        }
        if (Escape.IsValid()) return Escape;
    }

    // 不动也是一个真实分支，正在飞来的敌子不能因棋盘快照缺失而被忽略。
    float Best = CounterScore(State, FPlan(), Side) - Baseline;
    FChessMove2P Result;
    FRandomStream Random(RandomSeed);
    for (int32 I = 0; I < FMath::Min(Settings.Candidates, Candidates.Num()); ++I)
    {
        const auto& Plan = Candidates[I];
        FForecast Next;
        float Score = CounterScore(State, Plan, Side, &Next) - Baseline;
        if (Score > -WinScore * .8f && Settings.Continuations > 0 && Next.Winner == INDEX_NONE)
        {
            // 在对手最强应手及必要逃离之后继续推演，不能假设对手一直不动。
            const float StartValue = Evaluate(Next, Side);
            auto Follow = Plans(Next, Side, 20.f);
            for (auto& Step : Follow)
                Step.Score = (ForecastScore(Next, Step, Side) - StartValue) / (1.f + Step.Wait * .12f) - Step.Wait * 1.5f;
            Follow.Sort([](const FPlan& A, const FPlan& B) { return A.Score > B.Score; });
            float Improvement = 0.f;
            for (int32 J = 0; J < FMath::Min(Settings.Continuations, Follow.Num()); ++J)
            {
                const auto& Step = Follow[J];
                const float Value = (CounterScore(Next, Step, Side) - StartValue)
                    / (1.f + Step.Wait * .12f) - Step.Wait * 1.5f;
                Improvement = FMath::Max(Improvement, FMath::Min(250.f, Value));
            }
            Score += Improvement * .3f;
        }
        if (FMath::Abs(Score) < WinScore * .8f)
            Score = Score / (1.f + Plan.Wait * .12f) - Plan.Wait * 1.5f - RepeatPenalty(Plan, LastMove, RecentMoves, Board);
        Score += Random.FRandRange(-Settings.Noise, Settings.Noise);
        if (Score > Best)
        {
            Best = Score;
            // 有目的地攒粮，下一次决策重新评估，绝不提交付不起费用的走法。
            Result = Plan.Wait <= KINDA_SMALL_NUMBER ? Plan.Move : FChessMove2P();
        }
    }
    return Result;
}
