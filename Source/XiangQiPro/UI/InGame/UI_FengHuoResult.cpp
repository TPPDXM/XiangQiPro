#include "UI_FengHuoResult.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/GameMode/FengHuoBattleComponent.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/UI/XQPUIStyle.h"

void UUI_FengHuoResult::NativeConstruct()
{
    Super::NativeConstruct();
    XQPUIStyle::ConstrainPlayerName(RedPlayerName);
    XQPUIStyle::ConstrainPlayerName(BlackPlayerName);
    ReplayEntrance(true);
}

void UUI_FengHuoResult::ShowBattle(const UFengHuoBattleComponent* Battle, const FText& RedName, const FText& BlackName, EChessColor Winner)
{
    if (!Battle) return;
    const bool Draw = Battle->IsDraw();
    const bool Won = Winner == Battle->GetLocalColor();
    const int32 Seconds = FMath::RoundToInt(Battle->GetElapsed());
    const auto* Room = Battle->IsNetwork() && GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UXQPLANSubsystem>() : nullptr;
    const bool Watching = Room && Room->IsSpectator();
    const TCHAR* Reason = Watching && (Room->DidPeerResign() || Room->DidLocalResign()) ? TEXT("棋手认输")
        : Room && Room->DidPeerResign() ? TEXT("对方已认输")
        : Room && Room->DidLocalResign() ? TEXT("你已认输")
        : Battle->IsTimedOut() ? TEXT("限时结束，按吃子得分结算") : TEXT("对局结束");
    Configure(FText::FromString(Watching ? TEXT("对局结束") : Draw ? TEXT("烽火弈 · 平局") : Won ? TEXT("烽火弈 · 胜利") : TEXT("烽火弈 · 败北")),
        FText::FromString(FString::Printf(TEXT("%s\n对局时长 %02d:%02d"), Reason, Seconds / 60, Seconds % 60)),
        Watching ? FText::GetEmpty() : FText::FromString(TEXT("再战")),
        FText::FromString(Watching ? TEXT("退出观战") : TEXT("返回主菜单")), FText::FromString(TEXT("查看棋盘")));
    if (Watching) SetSpectatorOutcome();
    else SetMatchOutcome(Won, Draw);
    if (RedPlayerName) RedPlayerName->SetText(RedName);
    if (BlackPlayerName) BlackPlayerName->SetText(BlackName);
    const auto Summary = [](const FFengHuoStats& Stats)
    {
        return FText::FromString(FString::Printf(TEXT("%d\n%d\n%d\n%d\n%.2f"), Stats.CaptureScore, Stats.Captures,
            Stats.Moves, Stats.SupplySpent, Stats.SupplySpent ? float(Stats.CaptureScore) / Stats.SupplySpent : 0.f));
    };
    if (RedSummary) RedSummary->SetText(Summary(Battle->GetStats(EChessColor::REDCHESS)));
    if (BlackSummary) BlackSummary->SetText(Summary(Battle->GetStats(EChessColor::BLACKCHESS)));
    ReplayEntrance(true);
}
