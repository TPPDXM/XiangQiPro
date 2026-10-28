#include "ProBattleAI.h"

namespace
{
struct FDefenceRisk
{
    float King = 0.f;
    float Material = 0.f;
    float Total() const { return King * 2400.f + Material; }
};

// 重新计算车线、炮架、马腿及将帅照面，不能沿用走子前的攻击图。
FDefenceRisk DefenceRisk(const FChessPosition& Board, EChessColor Side, float EnemySupply,
    float Horizon, const TArray<FProBattleUnit>& Units, float Now, float RecoveryRate)
{
    FDefenceRisk Risk;
    for (int32 I = 0; I < 90; ++I)
    {
        const auto Target = Board.Squares[I];
        if (Target.Type == EChessType::EMPTY || Target.Color != Side) continue;
        const FIntPoint Square(I / 9, I % 9);
        float Threat = 0.f;
        for (int32 J = 0; J < 90; ++J)
        {
            const auto Attacker = Board.Squares[J];
            if (Attacker.Type == EChessType::EMPTY || Attacker.Color == Side) continue;
            const float Cost = ProBattle::Rule(Attacker.Type).Cost;
            const float Wait = FMath::Max(0.f, Cost - EnemySupply - Horizon * RecoveryRate) / FMath::Max(0.01f, RecoveryRate);
            if (Wait > 3.f || !Board.CanReach({J / 9, J % 9}, Square)) continue;
            const float Travel = ProBattle::TravelSeconds(Attacker.Type, {J / 9, J % 9}, Square);
            Threat += 1.f / (0.6f + Travel + Wait);
        }
        for (const auto& Unit : Units)
            if (Unit.bAlive && Unit.bMoving && Unit.Color != Side && Unit.To == Square && Unit.ArriveAt >= Now + Horizon)
                Threat += 3.f / (0.25f + Unit.ArriveAt - Now - Horizon);
        if (Target.Type == EChessType::JIANG) Risk.King += Threat;
        else Risk.Material += ProBattle::Rule(Target.Type).Score * 24.f * FMath::Min(Threat, 3.f);
    }
    return Risk;
}
}

FChessMove2P FProBattleAI::ChooseMove(const FChessPosition& Board, const TArray<FProBattleUnit>& Units,
    EChessColor Side, float Supply, float EnemySupply, float Now, float Remaining, const FChessMove2P& LastMove, int32 RandomSeed, float RecoveryRate)
{
    const EChessColor Enemy = FChessPosition::Opponent(Side);
    const auto Before = DefenceRisk(Board, Side, EnemySupply, 0.f, Units, Now, RecoveryRate);
    const FIntPoint OwnKing = Board.FindKing(Side), EnemyKing = Board.FindKing(Enemy);
    TArray<FChessMove2P> Moves;
    for (const auto& Unit : Units)
        if (Unit.bAlive && !Unit.bMoving && Unit.Color == Side && Supply + KINDA_SMALL_NUMBER >= ProBattle::Rule(Unit.Type).Cost)
            Board.GeneratePieceMoves(Unit.Square, Moves);

    float Best = 0.5f;
    FChessMove2P Result;
    FRandomStream Random(RandomSeed);
    for (const auto& Move : Moves)
    {
        const auto Piece = Board.Get(Move.from), Victim = Board.Get(Move.to);
        const auto Rule = ProBattle::Rule(Piece.Type);
        const float Travel = ProBattle::TravelSeconds(Piece.Type, Move.from, Move.to);
        if (Travel > Remaining || Units.ContainsByPredicate([&](const FProBattleUnit& U)
            { return U.bAlive && U.bMoving && U.Color == Side && U.To == Move.to; })) continue;

        FChessPosition InFlight = Board;
        InFlight.Set(Move.from, {});
        FChessPosition Future = Board;
        Future.MakeMove(Move);
        const auto TransitRisk = DefenceRisk(InFlight, Side, EnemySupply, 0.f, Units, Now, RecoveryRate);
        const auto ArrivalRisk = DefenceRisk(Future, Side, EnemySupply, Travel, Units, Now, RecoveryRate);
        float Score = (Before.Total() - ArrivalRisk.Total()) * 0.9f;
        // 离开炮架或车线遮挡期间暴露将帅，比到达后补救的风险更高。
        Score -= FMath::Max(0.f, TransitRisk.King - Before.King) * 4000.f;
        if (Piece.Type == EChessType::JIANG && ArrivalRisk.King > 0.f) Score -= ArrivalRisk.King * 3200.f;
        Score -= Rule.Cost * (Supply < 5.f ? 4.f : 1.8f) + Travel * 2.f;

        if (Victim.Type != EChessType::EMPTY)
        {
            const bool CanFlee = EnemySupply >= ProBattle::Rule(Victim.Type).Cost;
            Score += (Victim.Type == EChessType::JIANG ? 12000.f : ProBattle::Rule(Victim.Type).Score * 20.f)
                / (1.f + Travel * (CanFlee ? 1.8f : 0.4f));
        }
        // 将帅受威胁时保留逃离所需粮草，避免吃小子耗尽最后两点。
        if (Before.King > 0.f && Piece.Type != EChessType::JIANG && Supply - Rule.Cost < 2.f && ArrivalRisk.King > 0.f)
            Score -= 1200.f;
        if (Before.King > 0.f && ArrivalRisk.King <= 0.f) Score += 800.f;

        for (const auto& Unit : Units)
        {
            if (!Unit.bAlive || !Unit.bMoving || Unit.Color == Side) continue;
            if (Unit.To == Move.from) Score += Piece.Type == EChessType::JIANG ? 6000.f : Rule.Score * 90.f;
            if (Unit.To == Move.to && Unit.ArriveAt >= Now + Travel)
                Score -= Piece.Type == EChessType::JIANG ? 10000.f : Rule.Score * 120.f;
            if (Unit.To == OwnKing && Move.from != OwnKing &&
                (Victim.Type != EChessType::JIANG || Now + Travel >= Unit.ArriveAt))
                Score -= 16000.f;
        }
        const int32 Advance = Side == EChessColor::REDCHESS ? Move.to.X - Move.from.X : Move.from.X - Move.to.X;
        Score += Advance * (Piece.Type == EChessType::BING ? 1.4f : 0.15f);
        if (FChessPosition::IsInside(EnemyKing) && Future.CanReach(Move.to, EnemyKing)) Score += 8.f;
        if (Supply >= 9.f && Before.King <= 0.f) Score += 4.f;
        if (Piece.Type == EChessType::JIANG && Before.King <= 0.f) Score -= 24.f;
        if (LastMove.IsValid() && LastMove.to == Move.from)
        {
            Score -= 3.f;
            if (LastMove.from == Move.to && Victim.Type == EChessType::EMPTY) Score -= 14.f;
        }
        Score += Random.FRandRange(-1.f, 1.f);
        if (Score > Best) { Best = Score; Result = Move; }
    }
    return Result;
}
