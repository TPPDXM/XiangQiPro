// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_InGamePause.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/InGame/UI_Battle2P_Base.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UUI_InGamePause::NativeConstruct()
{
    Super::NativeConstruct();
    RestartButton = Cast<UUserWidget>(GetWidgetFromName(TEXT("Btn_Restart")));
    MainMenuButton = Cast<UUserWidget>(GetWidgetFromName(TEXT("Btn_QuitMap")));
    ResignButton = Cast<UUserWidget>(GetWidgetFromName(TEXT("Btn_Resign")));
    // 保留原菜单的设置、继续按钮与动画，重开和离房改由统一逻辑处理。
    if (auto* Button = XQPButtonBinding::GetButton(RestartButton))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddUniqueDynamic(this, &UUI_InGamePause::RestartOrShowResult);
    }
    if (auto* Button = XQPButtonBinding::GetButton(MainMenuButton))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddUniqueDynamic(this, &UUI_InGamePause::LeaveToMainMenu);
    }
    if (auto* Button = XQPButtonBinding::GetButton(ResignButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_InGamePause::AskToResign);
    RefreshActions();
    UpdateCanTick();
}

void UUI_InGamePause::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);
    RefreshActions();
}

void UUI_InGamePause::RefreshActions()
{
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    const bool LAN = State && State->IsLANGame();
    const auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    XQPButtonBinding::SetVisibility(ResignButton, LAN && Room->GetState() == EXQPRoomState::Playing
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetVisibility(RestartButton, !LAN || State->IsGameOver() || Room->GetState() == EXQPRoomState::Disconnected
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetCaption(RestartButton, FText::FromString(State && (State->IsGameOver() || (LAN && Room->GetState() == EXQPRoomState::Disconnected))
        ? TEXT("查看对局结果") : TEXT("重新开始")));
    if (auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_146"))))
        Title->SetText(FText::FromString(LAN ? TEXT("对局菜单") : TEXT("暂停中…")));
}

void UUI_InGamePause::RestartOrShowResult()
{
    auto* Manager = GetGameInstance()->GetSubsystem<UUIManager>();
    auto* Battle = Cast<UUI_Battle2P_Base>(Manager->GetBasicUI());
    auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!Battle || !State) return;
    if (State->IsLANGame() || State->IsGameOver()) Battle->ShowCurrentResult();
    else
    {
        Battle->ExecGamePlayAgain();
        Manager->CloseOverlays();
    }
}

void UUI_InGamePause::LeaveToMainMenu()
{
    if (const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>(); State && State->IsLANGame())
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Default);
    UGameplayStatics::SetGamePaused(this, false);
    UXQPGameInstance::OpenGameLevel(this, TEXT("/Game/Map/Main"));
}

void UUI_InGamePause::AskToResign()
{
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (!CommonDialogClass || Room->GetState() != EXQPRoomState::Playing) return;
    auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
    if (!Dialog) return;
    ResignDialog = Dialog;
    Dialog->Configure(FText::FromString(TEXT("确认认输")), FText::FromString(TEXT("认输后本局结束，对方获胜。")),
        FText::FromString(TEXT("确认认输")), FText::FromString(TEXT("取消")));
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(this, &UUI_InGamePause::ConfirmResign);
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog);
}

void UUI_InGamePause::ConfirmResign()
{
    // 先收起确认框，随后胜负事件会打开同一种通用弹窗。
    if (ResignDialog.IsValid()) ResignDialog->Dismiss();
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->Resign();
}

