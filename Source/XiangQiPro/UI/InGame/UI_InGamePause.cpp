// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_InGamePause.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/InGame/UI_Battle2P_Base.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/SaveGame/ReenactmentLibrary.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UUI_InGamePause::NativeConstruct()
{
    Super::NativeConstruct();
    bLeavingToMainMenu = false;
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
    // 悔棋按钮预置在桌面/移动端 UMG 资产中，顺序、尺寸和边距由设计器管理。
    // 与其余按钮使用同一个 WB_Btn 组件，仅在此绑定行为；隐藏时连同 SizeBox 占位收起。
    UndoButton = Cast<UUserWidget>(GetWidgetFromName(TEXT("Btn_Undo")));
    if (auto* Button = XQPButtonBinding::GetButton(UndoButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_InGamePause::UndoMove);
    // 控件直接预置在两端暂停UMG中，置于返回主菜单上方；其他模式收起整个占位。
    EditArrangementButton = Cast<UUserWidget>(GetWidgetFromName(TEXT("Btn_EditGame")));
    if (auto* Button = XQPButtonBinding::GetButton(EditArrangementButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_InGamePause::EditArrangement);
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
    // LeaveRoom/SetGameMode 会立刻改变按钮资格，但旧菜单还会显示约 0.25 秒。
    // 退出期间停止状态刷新，保留可见性、文案与启用状态，交给上层转场统一淡出。
    if (bLeavingToMainMenu) return;
    const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    const bool LAN = State && State->IsLANGame();
    const auto* Instance = GetGameInstance<UXQPGameInstance>();
    const auto* Arrangements = Instance ? Instance->GetSubsystem<UReenactmentLibrary>() : nullptr;
    const bool bArrangementBattle = Instance && Instance->GetGameMode() == EXQPGameMode::Reenactment &&
        Arrangements && !Arrangements->IsEditing();
    XQPButtonBinding::SetVisibility(EditArrangementButton,
        bArrangementBattle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    const bool bClassic = State && State->SupportsUndo() && !State->IsSpectator();
    XQPButtonBinding::SetVisibility(UndoButton, bClassic ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (UndoButton)
    {
        const auto* UndoRoom = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
        UndoButton->SetIsEnabled(bClassic && (LAN ? UndoRoom->CanRequestUndo() : State->CanUndoLocalMove()));
        const int32 Remaining = State ? State->GetRemainingUndoCount() : 0;
        XQPButtonBinding::SetCaption(UndoButton, FText::FromString(Remaining == INDEX_NONE
            ? TEXT("悔棋") : FString::Printf(TEXT("悔棋（%d）"), Remaining)));
    }
    const auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    XQPButtonBinding::SetVisibility(ResignButton, LAN && !Room->IsSpectator() && Room->GetState() == EXQPRoomState::Playing
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetVisibility(RestartButton, !LAN || State->IsGameOver() || Room->GetState() == EXQPRoomState::Disconnected
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    XQPButtonBinding::SetCaption(RestartButton, FText::FromString(State && (State->IsGameOver() || (LAN && Room->GetState() == EXQPRoomState::Disconnected))
        ? TEXT("查看对局结果") : State && State->bEnableMachineLearning ? TEXT("重开本局") : TEXT("重新开始")));
    if (auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_146"))))
        Title->SetText(FText::FromString(LAN ? TEXT("选项") : TEXT("暂停中…")));
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
    if (bLeavingToMainMenu) return;
    // 必须先锁定呈现状态，再清理联机与玩法；也避免重复点击产生第二次退出请求。
    bLeavingToMainMenu = true;
    if (const auto* State = GetWorld()->GetGameState<AXQPGameStateBase>(); State && State->IsLANGame())
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Default);
    UGameplayStatics::SetGamePaused(this, false);
    UXQPGameInstance::OpenGameLevel(this, PATH_MAP_MAIN);
}

void UUI_InGamePause::AskToResign()
{
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (!CommonDialogClass || Room->IsSpectator() || Room->GetState() != EXQPRoomState::Playing) return;
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


void UUI_InGamePause::UndoMove()
{
    auto* State = GetWorld()->GetGameState<AXQPGameStateBase>();
    if (!State || !State->SupportsUndo() || State->IsSpectator()) return;
    if (State->IsLANGame()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->RequestUndo();
    else if (State->UndoLocalClassicMove()) GetGameInstance()->GetSubsystem<UUIManager>()->CloseOverlays();
}

void UUI_InGamePause::EditArrangement()
{
    auto* Instance = GetGameInstance<UXQPGameInstance>();
    if (bLeavingToMainMenu || !Instance || Instance->GetGameMode() != EXQPGameMode::Reenactment) return;
    auto* Library = Instance->GetSubsystem<UReenactmentLibrary>();
    if (!Library || Library->IsEditing()) return;
    // 编辑的是已保存的开局布阵，而不是对弈中已经移动、吃子的临时棋盘。
    // SelectGame保持棋局身份、作者及AI选项；重载关卡后由正式编辑初始化重建棋盘。
    FString Error;
    const FString Id = Library->GetDraft().Id;
    if (!Library->SelectGame(Id,true,Error))
    {
        if (CommonDialogClass)
            if (auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(),CommonDialogClass))
            {
                Dialog->Configure(FText::FromString(TEXT("无法编辑棋局")),FText::FromString(Error),
                    FText::GetEmpty(),FText::FromString(TEXT("返回")));
                GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog);
            }
        return;
    }
    // 复用退出转场的刷新锁，防止IsEditing变化让按钮在淡出期间突然消失。
    bLeavingToMainMenu = true;
    UXQPGameInstance::OpenGameLevel(this,PATH_MAP_BATTLE2P);
}
