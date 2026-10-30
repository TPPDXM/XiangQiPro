#include "UI_ProModeSetup.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "UI_LANLobby.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UUI_ProModeSetup::NativeConstruct()
{
    Super::NativeConstruct();
    bLobbyVisible = false;
    if (LobbyHost) { LobbyHost->ClearChildren(); LobbyHost->SetVisibility(ESlateVisibility::Collapsed); }
    OnPrimaryAction = FSimpleDelegate::CreateUObject(this, &UUI_ProModeSetup::StartGame);
    OnSecondaryAction = FSimpleDelegate::CreateUObject(this, &UUI_ProModeSetup::ToggleLobby);
    ConfigureMode();
}

void UUI_ProModeSetup::ConfigureMode()
{
    if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("ProIntroductionLabel"))))
        Label->SetText(FText::FromString(TEXT("玩法介绍")));
    if (auto* Description = GetWidgetFromName(TEXT("ModeDescriptionScroll")))
        Description->SetVisibility(ESlateVisibility::Visible);
    const bool Pro = ModeId == 6;
    const TCHAR* Title = Pro ? TEXT("烽火弈") : (ModeId == 3 ? TEXT("千里走单骑") : TEXT("兵不厌诈"));
    const TCHAR* Introduction = Pro
        ? TEXT("三分钟即时对弈，双方随时执棋。每次移动消耗粮草，粮草随时间恢复，最多储存10点，最后30秒恢复速度翻倍。\n\n兵1 · 士1 · 象2 · 将2 · 马3 · 炮4 · 车5\n不同棋子有不同速度；抵达时敌子仍在目标格才会被吃，途中可抢先逃离。保留象棋走法，无需等待回合或应将。\n\n吃掉对方将帅立即获胜。时间耗尽按吃子得分决定胜负，同分为平局。\n吃子分：兵1 · 士2 · 象3 · 马5 · 炮6 · 车9")
        : (ModeId == 3
            ? TEXT("仅以一匹红马迎战不断出现的敌棋。沿用马的走法，选择落点，连续吃子，突破重围。\n\n留意蹩马腿与敌方威胁，利用机动性寻找安全路线。红马被吃后本局结束。")
            : TEXT("将帅身份公开，其余棋子的身份与阵营隐藏。棋子首次移动后揭示真实身份，随后按真实身份参与对弈。\n\n观察揭示结果，随时调整计划。保留棋子的走法限制，吃掉对方将帅或让对手无棋可走即可获胜。"));
    Configure(FText::FromString(Title), FText::FromString(Introduction),
        FText::FromString(Pro ? TEXT("AI 对战") : TEXT("开始游戏")),
        Pro ? FText::FromString(TEXT("联机对战")) : FText::GetEmpty(), FText::FromString(TEXT("返回")));
}

void UUI_ProModeSetup::StartGame()
{
    if (bLobbyVisible) return;
    auto* GI = GetGameInstance<UXQPGameInstance>();
    GI->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GI->SetGameMode(ModeId == 6 ? EXQPGameMode::ProBattleAI : (ModeId == 3 ? EXQPGameMode::SoloRide : EXQPGameMode::GuessWho));
    UXQPGameInstance::OpenGameLevel(this, TEXT("/Game/Map/Battle2P"));
}

void UUI_ProModeSetup::ToggleLobby()
{
    if (ModeId != 6 || !LobbyHost) return;
    bLobbyVisible = !bLobbyVisible;
    if (bLobbyVisible && LobbyClass)
    {
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->SelectProMode(true);
        auto* Lobby = XQPMobile::CreateWidget<UUI_LANLobby>(GetOwningPlayer(), LobbyClass);
        if (!Lobby) { bLobbyVisible = false; return; }
        Lobby->bProModeLobby = true;
        LobbyHost->SetContent(Lobby);
        LobbyHost->SetVisibility(ESlateVisibility::Visible);
        if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("ProIntroductionLabel"))))
            Label->SetText(FText::FromString(TEXT("联机房间")));
        if (auto* Description = GetWidgetFromName(TEXT("ModeDescriptionScroll")))
            Description->SetVisibility(ESlateVisibility::Collapsed);
        Configure(FText::FromString(TEXT("烽火弈 · 联机对战")),
            FText::FromString(TEXT("三分钟即时对弈 · 粮草恢复 · 抵达吃子\n吃将即胜，超时比较吃子得分，同分为平局。创建房间或输入房主 IP 加入，双方准备后开局。")),
            FText::GetEmpty(), FText::FromString(TEXT("返回玩法介绍")), FText::FromString(TEXT("关闭")));
    }
    else
    {
        LobbyHost->ClearChildren();
        LobbyHost->SetVisibility(ESlateVisibility::Collapsed);
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
        ConfigureMode();
    }
    ReplayEntrance();
}

void UUI_ProModeSetup::NativeDestruct()
{
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (bLobbyVisible && Room->GetState() != EXQPRoomState::Playing && Room->GetState() != EXQPRoomState::Finished) Room->LeaveRoom();
    Super::NativeDestruct();
}
