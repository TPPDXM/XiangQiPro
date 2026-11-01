#include "UI_GameModeSetup.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Components/TextBlock.h"

// 通用窗口只绑定通用动作；子类可以替换委托以增加专用配置。
void UUI_GameModeSetup::NativeConstruct()
{
    Super::NativeConstruct();
    OnPrimaryAction = FSimpleDelegate::CreateUObject(this, &UUI_GameModeSetup::StartGame);
    ConfigureMode();
}

void UUI_GameModeSetup::ConfigureMode()
{
    const bool Solo = ModeId == 3;
    if (auto* Panel = GetWidgetFromName(TEXT("DifficultyPanel"))) Panel->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Panel = GetWidgetFromName(TEXT("LobbyHost"))) Panel->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Subtitle = Cast<UTextBlock>(GetWidgetFromName(TEXT("ModeSubtitle"))))
        Subtitle->SetText(FText::FromString(Solo ? TEXT("一骑闯关 · 步步为营") : TEXT("明暗交锋 · 识破对手")));
    Configure(FText::FromString(Solo ? TEXT("千里走单骑") : TEXT("兵不厌诈")),
        FText::FromString(Solo
            ? TEXT("仅以一匹红马迎战不断出现的敌棋。沿用马的走法，选择落点，连续吃子，突破重围。\n\n留意蹩马腿与敌方威胁，利用机动性寻找安全路线。红马被吃后本局结束。")
            : TEXT("将帅身份公开，其余棋子的身份与阵营隐藏。棋子首次移动后揭示真实身份，随后按真实身份参与对弈。\n\n观察揭示结果，随时调整计划。保留棋子的走法限制，吃掉对方将帅或让对手无棋可走即可获胜。")),
        FText::FromString(TEXT("开始游戏")), FText::GetEmpty(), FText::FromString(TEXT("返回")));
}

void UUI_GameModeSetup::StartGame()
{
    auto* GI = GetGameInstance<UXQPGameInstance>();
    if (!GI || (ModeId != 3 && ModeId != 4)) return;
    GI->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GI->SetGameMode(ModeId == 3 ? EXQPGameMode::SoloRide : EXQPGameMode::GuessWho);
    UXQPGameInstance::OpenGameLevel(this, TEXT("/Game/Map/Battle2P"));
}
