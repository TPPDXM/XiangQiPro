#include "UI_ProModeSetup.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "UI_LANLobby.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UUI_ProModeSetup::NativeConstruct()
{
    Super::NativeConstruct();
    // 控件及全部样式保存在PC/移动端UMG资产，C++只负责绑定和设置数据。
    DifficultyChoice = DifficultySelector ? DifficultySelector->ComboBox : nullptr;
    if (DifficultyChoice)
    {
        DifficultyChoice->OnSelectionChanged.RemoveDynamic(this, &UUI_ProModeSetup::DifficultyChanged);
        DifficultyChoice->ClearOptions();
        for (const TCHAR* Option : {TEXT("简单"), TEXT("普通"), TEXT("困难"), TEXT("大师")}) DifficultyChoice->AddOption(Option);
        DifficultyChoice->OnSelectionChanged.AddUniqueDynamic(this, &UUI_ProModeSetup::DifficultyChanged);
    }
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
    if (auto* Subtitle = Cast<UTextBlock>(GetWidgetFromName(TEXT("ModeSubtitle"))))
        Subtitle->SetText(FText::FromString(Pro ? TEXT("三分钟即时对弈 · 运筹粮草，攻守争先") :
            (ModeId == 3 ? TEXT("一骑闯关 · 步步为营") : TEXT("明暗交锋 · 识破对手"))));
    const TCHAR* Title = Pro ? TEXT("烽火弈") : (ModeId == 3 ? TEXT("千里走单骑") : TEXT("兵不厌诈"));
    const TCHAR* Introduction = Pro
        ? TEXT("即时交锋\n双方随时出兵，无需等待回合或应将。棋子抵达时才吃子，途中可以抢先逃离。将帅只走九宫内一格，取消照面飞将。\n\n粮草调度\n初始5点，上限10点，随时间恢复；最后30秒恢复翻倍。\n移动消耗：兵1 · 士1 · 象2 · 将2 · 马3 · 炮4 · 车5\n\n胜负规则\n吃将立即获胜；三分钟结束后比较吃子得分，同分平局。\n吃子得分：兵1 · 士2 · 象3 · 马5 · 炮6 · 车9")
        : (ModeId == 3
            ? TEXT("仅以一匹红马迎战不断出现的敌棋。沿用马的走法，选择落点，连续吃子，突破重围。\n\n留意蹩马腿与敌方威胁，利用机动性寻找安全路线。红马被吃后本局结束。")
            : TEXT("将帅身份公开，其余棋子的身份与阵营隐藏。棋子首次移动后揭示真实身份，随后按真实身份参与对弈。\n\n观察揭示结果，随时调整计划。保留棋子的走法限制，吃掉对方将帅或让对手无棋可走即可获胜。"));
    Configure(FText::FromString(Title), FText::FromString(Introduction),
        FText::FromString(Pro ? TEXT("AI 对战") : TEXT("开始游戏")),
        Pro ? FText::FromString(TEXT("联机对战")) : FText::GetEmpty(), FText::FromString(TEXT("返回")));
    if (Pro) ShowDifficulty();
    else if (DifficultyPanel) DifficultyPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UUI_ProModeSetup::ShowDifficulty()
{
    if (!DifficultyPanel || !DifficultyChoice) return;
    DifficultyPanel->SetVisibility(ESlateVisibility::Visible);
    if (auto* GI = GetGameInstance<UXQPGameInstance>())
        DifficultyChoice->SetSelectedIndex(FMath::Clamp(static_cast<int32>(GI->ProSettings.AIDifficulty), 0, 3));
    else
        DifficultyChoice->SetSelectedIndex(static_cast<int32>(EProAIDifficulty::Hard));
    DifficultyChanged(DifficultyChoice->GetSelectedOption(), ESelectInfo::Direct);
}

void UUI_ProModeSetup::DifficultyChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!DifficultyChoice) return;
    const int32 Index = DifficultyChoice->FindOptionIndex(SelectedItem);
    if (Index < 0 || Index > 3) return;
    if (auto* GI = GetGameInstance<UXQPGameInstance>()) GI->ProSettings.AIDifficulty = static_cast<EProAIDifficulty>(Index);
    static const TCHAR* Hints[] = {
        TEXT("从容练习，熟悉出兵与粮草节奏。"), TEXT("主动争子，兼顾进攻与防守。"),
        TEXT("快速应战，推演反击与连续进攻。"), TEXT("极速应战，更广泛地推演攻防与抢先吃将。")};
    if (DifficultyHint) DifficultyHint->SetText(FText::FromString(Hints[Index]));
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
        if (!Lobby) { bLobbyVisible = false; ConfigureMode(); return; }
        Lobby->bProModeLobby = true;
        if (DifficultyPanel) DifficultyPanel->SetVisibility(ESlateVisibility::Collapsed);
        LobbyHost->SetContent(Lobby);
        LobbyHost->SetVisibility(ESlateVisibility::Visible);
        if (auto* Subtitle = Cast<UTextBlock>(GetWidgetFromName(TEXT("ModeSubtitle"))))
            Subtitle->SetText(FText::FromString(TEXT("创建或加入房间 · 双方准备后开局")));
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
        bLobbyVisible = false;
        LobbyHost->ClearChildren();
        LobbyHost->SetVisibility(ESlateVisibility::Collapsed);
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
        ConfigureMode();
    }
    ReplayEntrance();
}

void UUI_ProModeSetup::NativeDestruct()
{
    auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UXQPLANSubsystem>() : nullptr;
    if (Room && bLobbyVisible && Room->GetState() != EXQPRoomState::Playing && Room->GetState() != EXQPRoomState::Finished) Room->LeaveRoom();
    Super::NativeDestruct();
}
