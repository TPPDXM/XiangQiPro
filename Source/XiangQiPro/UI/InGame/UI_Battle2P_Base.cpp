// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Battle2P_Base.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/UI/XQP_HUD.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/Interface/IF_GameState.h"

#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameMode/BattleStartComponent.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/SaveGame/PlayerProfileSubsystem.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/GameMode/XQPPlayerController.h"
#include "XiangQiPro/Util/EndingLibrary.h"
#include "XiangQiPro/UI/XQPUIStyle.h"

void UUI_Battle2P_Base::NativeConstruct()
{
    Super::NativeConstruct();
    UpdateCanTick();
    GameState = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    RefreshPlayerInfoLayout();
    const bool Solo = GetGameInstance<UXQPGameInstance>()->GetGameMode() == EXQPGameMode::SoloRide;
    SetPlayerScoreVisible(true);
    SetOpponentScoreVisible(!Solo);
    if (auto* Button = XQPButtonBinding::GetButton(MenuButton))
        Button->OnClicked.AddUniqueDynamic(this, &UUI_Battle2P_Base::OpenMenu);
    if (auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>())
    {
        Room->OnRoomUpdated.RemoveAll(this);
        Room->OnRoomUpdated.AddUObject(this, &UUI_Battle2P_Base::HandleRoomUpdated);
    }
}

void UUI_Battle2P_Base::NativeDestruct()
{
    if (GetGameInstance())
        GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->OnRoomUpdated.RemoveAll(this);
    Super::NativeDestruct();
}

void UUI_Battle2P_Base::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!GameState.IsValid()) return;
    RefreshPlayerInfoLayout();
    const auto* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
    const bool LAN = GameState->IsLANGame();
    const bool Solo = GameState->GetBattleType() == EBattleType::SoloRide;
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (Text_Name_P1)
        Text_Name_P1->SetText(FText::FromString(LAN ? Room->GetRedName() : Profile->GetPlayerName()));
    if (Text_Name_P2)
    {
        Text_Name_P2->SetVisibility(Solo ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        Text_Name_P2->SetText(Solo ? FText::GetEmpty() : FText::FromString(LAN ? Room->GetBlackName() : TEXT("电子棋友")));
    }
    if (Solo)
    {
        if (auto* Panel = GetWidgetFromName(TEXT("BlackInfoPanel"))) Panel->SetVisibility(ESlateVisibility::Collapsed);
        if (Text_AIThinking) Text_AIThinking->SetVisibility(ESlateVisibility::Collapsed);
        if (Image_RoundMark_P2) Image_RoundMark_P2->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ModeStatus)
    {
        const bool Challenge = GetGameInstance<UXQPGameInstance>()->GetGameMode() == EXQPGameMode::AI2P && Profile->bChallengeSelected;
        ModeStatus->SetVisibility(LAN || Challenge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        ModeStatus->SetText(FText::FromString(LAN
            ? FString::Printf(TEXT("你执%s · %s"), Room->GetLocalColor() == EChessColor::REDCHESS ? TEXT("红方") : TEXT("黑方"),
                GameState->IsGameOver() ? TEXT("本局结束") :
                (GameState->GetTurnColor() == EChessColor::REDCHESS ? TEXT("红方回合") : TEXT("黑方回合")))
            : FString::Printf(TEXT("最高连胜 %d 局"), Profile->GetBestStreak())));
    }
    if (LAN)
    {
        if (Text_AIThinking) Text_AIThinking->SetVisibility(ESlateVisibility::Collapsed);
        if (Image_RoundMark_P1)
            Image_RoundMark_P1->SetVisibility(!GameState->IsGameOver() && GameState->GetTurnColor() == EChessColor::REDCHESS
                ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (Image_RoundMark_P2)
            Image_RoundMark_P2->SetVisibility(!GameState->IsGameOver() && GameState->GetTurnColor() == EChessColor::BLACKCHESS
                ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UUI_Battle2P_Base::RefreshPlayerInfoLayout()
{
    const auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UXQPLANSubsystem>() : nullptr;
    const bool LocalBlack = GameState.IsValid() && GameState->IsLANGame() && Room &&
        Room->GetLocalColor() == EChessColor::BLACKCHESS;
    if (LocalBlack == bInfoPanelsSwapped) return;

    const auto SwapLayout = [](UWidget* Red, UWidget* Black)
    {
        auto* RedSlot = Red ? Cast<UCanvasPanelSlot>(Red->Slot) : nullptr;
        auto* BlackSlot = Black ? Cast<UCanvasPanelSlot>(Black->Slot) : nullptr;
        if (!RedSlot || !BlackSlot) return false;
        const FAnchorData RedLayout = RedSlot->GetLayout();
        RedSlot->SetLayout(BlackSlot->GetLayout());
        BlackSlot->SetLayout(RedLayout);
        return true;
    };
    // Designer中的红方位置是本地位置：移动端下方、PC端左侧。
    // 保留阵营对应的姓名与得分，只交换完整名牌的布局。
    if (SwapLayout(GetWidgetFromName(TEXT("RedInfoPanel")), GetWidgetFromName(TEXT("BlackInfoPanel"))))
    {
        // PC的回合标记独立于名牌；移动端标记已包含在名牌内部。
        SwapLayout(Image_RoundMark_P1, Image_RoundMark_P2);
        bInfoPanelsSwapped = LocalBlack;
    }
}

void UUI_Battle2P_Base::OpenMenu()
{
    if (auto* Controller = Cast<AXQPPlayerController>(GetOwningPlayer())) Controller->OnEscapePressed();
}

void UUI_Battle2P_Base::ShowBattleStart()
{
    if (auto* Ready = GetWidgetFromName(TEXT("BattleReadyOverlay")))
        Ready->SetVisibility(GameState.IsValid() && !GameState->BattleStart->HasStarted() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

UUI_CommonDialog* UUI_Battle2P_Base::OpenResultDialog()
{
    if (!CommonDialogClass) return nullptr;
    if (!ResultDialog.IsValid()) ResultDialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetOwningPlayer(), CommonDialogClass);
    auto* Dialog = ResultDialog.Get();
    if (!Dialog) return nullptr;
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateUObject(this, &UUI_Battle2P_Base::PlayAgain);
    Dialog->OnSecondaryAction = FSimpleDelegate::CreateUObject(this, &UUI_Battle2P_Base::ReturnToMainMenu);
    auto* Manager = GetGameInstance()->GetSubsystem<UUIManager>();
    // 对方离开或对局结束时，替换尚未确认的认输提示。
    if (!Dialog->IsInViewport())
    {
        // 结算直接显示棋盘，避免暂停菜单的遮罩与弹窗背景叠加。
        Manager->CloseOverlays();
        Manager->AddUI(Dialog, false);
    }
    else Manager->CloseDialogs(Dialog);
    return Dialog;
}

void UUI_Battle2P_Base::ShowMatchResult(EChessColor Winner)
{
    if (GameState.IsValid() && GameState->GetBattleType() == EBattleType::SoloRide)
    {
        ShowSoloRideResult(GameState->GetScore1());
        return;
    }
    auto* Dialog = OpenResultDialog();
    if (!Dialog) return;
    const bool LAN = GameState.IsValid() && GameState->IsLANGame();
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    const bool Won = Winner == (LAN ? Room->GetLocalColor() : EChessColor::REDCHESS);
    if (GetGameInstance<UXQPGameInstance>()->GetGameMode() == EXQPGameMode::Ending)
    {
        const FString Title = GameState.IsValid() ? UEndingLibrary::GetEndingGameTitle(GameState->GetCurrentEndingIndex()) : FString();
        FString Message = Won
            ? (Title.IsEmpty() ? TEXT("本关挑战成功！") : FString::Printf(TEXT("已通过「%s」"), *Title))
            : TEXT("本关挑战失败，可以重新挑战。");
        if (Won && GameState.IsValid())
        {
            Message += FString::Printf(TEXT("\n本次 %d 步 · 最少步数纪录 %d 步"), GameState->GetEndingMoveCount(), GameState->GetEndingBestSteps());
            if (GameState->DidBreakEndingRecord()) Message += TEXT("\n已刷新最少步数纪录！");
        }
        Dialog->Configure(FText::FromString(Won ? TEXT("残局通关") : TEXT("挑战失败")), FText::FromString(Message),
            FText::FromString(TEXT("重新挑战")), FText::FromString(TEXT("返回主菜单")), FText::FromString(TEXT("查看棋盘")));
        Dialog->SetMatchOutcome(Won);
        RefreshRematchAction();
        return;
    }
    FString Message = TEXT("本局对弈已结束。");
    if (LAN && Room->DidPeerResign()) Message = TEXT("对方已认输");
    else if (LAN && Room->DidLocalResign()) Message = TEXT("你已认输");
    Dialog->Configure(FText::FromString(Won ? TEXT("对弈胜利") : TEXT("对弈失败")), FText::FromString(Message),
        FText::FromString(TEXT("再战")), FText::FromString(TEXT("返回主菜单")), FText::FromString(TEXT("查看棋盘")));
    Dialog->SetMatchOutcome(Won);
    Dialog->SetMessageFaction(Winner);
    RefreshRematchAction();
}

void UUI_Battle2P_Base::ShowSoloRideResult(int32 Score)
{
    if (auto* Dialog = OpenResultDialog())
        Dialog->Configure(FText::FromString(TEXT("单骑挑战结束")),
            FText::FromString(FString::Printf(TEXT("本次得分：%d 分"), Score)),
            FText::FromString(TEXT("重新挑战")), FText::FromString(TEXT("返回主菜单")), FText::FromString(TEXT("查看棋盘")));
}

void UUI_Battle2P_Base::ShowCurrentResult()
{
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (GameState.IsValid() && GameState->IsLANGame() && Room->GetState() == EXQPRoomState::Disconnected)
        HandleRoomUpdated();
    else if (GameState.IsValid() && GameState->IsGameOver()) ShowMatchResult(GameState->GetWinnerColor());
}

void UUI_Battle2P_Base::HandleRoomUpdated()
{
    if (!GameState.IsValid() || !GameState->IsLANGame()) return;
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (Room->GetState() == EXQPRoomState::Disconnected)
    {
        if (auto* Dialog = OpenResultDialog())
            Dialog->Configure(FText::FromString(Room->GetStatus() == TEXT("对方已离开房间") ? TEXT("对方已离开") : TEXT("联机已中断")), FText::FromString(Room->GetStatus()),
                FText::GetEmpty(), FText::FromString(TEXT("返回主菜单")), FText::FromString(TEXT("查看棋盘")));
    }
    else RefreshRematchAction();
}

void UUI_Battle2P_Base::RefreshRematchAction()
{
    if (!ResultDialog.IsValid() || !ResultDialog->IsInViewport() || !GameState.IsValid()) return;
    auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
    if (GameState->IsLANGame())
    {
        if (Room->GetState() != EXQPRoomState::Finished) return;
        const bool Waiting = Room->IsHost() ? !Room->IsPeerReady() : Room->IsLocalReady();
        const TCHAR* Caption = Room->IsHost() ? (Waiting ? TEXT("等待对方再战") : TEXT("确认再战"))
                                            : (Waiting ? TEXT("等待对方确认") : TEXT("再战"));
        ResultDialog->SetPrimaryAction(FText::FromString(Caption), !Waiting);
    }
    else
    {
        if (GetGameInstance<UXQPGameInstance>()->GetGameMode() == EXQPGameMode::Ending)
        {
            const bool Won = GameState->GetWinnerColor() == EChessColor::REDCHESS;
            const bool HasNext = GameState->GetCurrentEndingIndex() + 1 < UEndingLibrary::GetEndingGameNum();
            ResultDialog->SetPrimaryAction(FText::FromString(Won ? (HasNext ? TEXT("下一关") : TEXT("再来一局")) : TEXT("重新挑战")), true);
            return;
        }
        const auto* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
        const bool Challenge = GetGameInstance<UXQPGameInstance>()->GetGameMode() == EXQPGameMode::AI2P && Profile->bChallengeSelected;
        ResultDialog->SetPrimaryAction(FText::FromString(Challenge ? (Profile->HasActiveRun() ? TEXT("下一局") : TEXT("重新挑战")) : TEXT("再战")), true);
    }
}

void UUI_Battle2P_Base::PlayAgain()
{
    if (!GameState.IsValid() || !GameState->IsGameOver()) return;
    if (GameState->IsLANGame())
    {
        auto* Room = GetGameInstance()->GetSubsystem<UXQPLANSubsystem>();
        if (Room->GetState() != EXQPRoomState::Finished || !Room->HasPeer()) return;
        if (Room->IsHost()) Room->StartMatch();
        else Room->SetReady(true);
        RefreshRematchAction();
        return;
    }
    // 先恢复菜单栈，再重开；AI和连战均由原有重开广播初始化下一局。
    GetGameInstance()->GetSubsystem<UUIManager>()->CloseOverlays();
    ResultDialog.Reset();
    ExecGamePlayAgain();
}

void UUI_Battle2P_Base::ReturnToMainMenu()
{
    if (GameState.IsValid() && GameState->IsLANGame()) GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Default);
    UGameplayStatics::SetGamePaused(this, false);
    UXQPGameInstance::OpenGameLevel(this, TEXT("/Game/Map/Main"));
}

// 刷新双方得分控件。
void UUI_Battle2P_Base::UpdateScore(int32 score1, int32 score2)
{
	if (IsValid(Text_Score_P1)) Text_Score_P1->SetText(FText::FromString(FString::FromInt(score1)));
	if (IsValid(Text_Score_P2)) Text_Score_P2->SetText(FText::FromString(FString::FromInt(score2)));
}

void UUI_Battle2P_Base::SetPlayerScoreVisible(bool bVisible)
{
    bPlayerScoreVisible = bVisible;
    if (PlayerScorePanel) PlayerScorePanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Text_Score_P1) Text_Score_P1->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UUI_Battle2P_Base::SetOpponentScoreVisible(bool bVisible)
{
    bOpponentScoreVisible = bVisible;
    if (OpponentScorePanel) OpponentScorePanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Text_Score_P2) Text_Score_P2->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// 更新AI思考提示和回合标记。
void UUI_Battle2P_Base::SetAITurn(bool bAITurn)
{
	if (bAITurn)
	{
		if (IsValid(Text_AIThinking)) Text_AIThinking->SetVisibility(ESlateVisibility::Visible); // 轮到AI则提示AI正在思考
		if (IsValid(Image_RoundMark_P1)) Image_RoundMark_P1->SetVisibility(ESlateVisibility::Collapsed); // 更新回合标记
		if (IsValid(Image_RoundMark_P2)) Image_RoundMark_P2->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		if (IsValid(Text_AIThinking)) Text_AIThinking->SetVisibility(ESlateVisibility::Collapsed); // 隐藏
		if (IsValid(Image_RoundMark_P1)) Image_RoundMark_P1->SetVisibility(ESlateVisibility::Visible); // 更新回合标记
		if (IsValid(Image_RoundMark_P2)) Image_RoundMark_P2->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// 广播重开事件并清空棋谱显示。
void UUI_Battle2P_Base::ExecGamePlayAgain()
{
    if (GameState.IsValid() && GameState->IsLANGame()) return;
    EXEC_PLAYAGAIN();
    SetAITurn(false); // 更新AI回合结束
    if (IsValid(Text_OperatingRecord)) Text_OperatingRecord->SetText(FText());
}

// 将当前走法追加到执棋方的棋谱。
void UUI_Battle2P_Base::AddOperatingRecord(EPlayerTag player, TWeakObjectPtr<AChesses> targetChess, FChessMove2P move)
{
	if (!IsValid(Text_OperatingRecord) || !IsValid(Text_Name_P1) || !IsValid(Text_Name_P2) || !targetChess.IsValid()) return;
	// 获取之前的操作
	FString text = Text_OperatingRecord->GetText().ToString();

	FString playerName;
	// 获取玩家名称
	switch (player)
	{
	case EPlayerTag::P1:
		playerName = Text_Name_P1->GetText().ToString();
		break;
	case EPlayerTag::AI:
	case EPlayerTag::P2:
		playerName = Text_Name_P2->GetText().ToString();
		break;
	default:
		playerName = Text_Name_P1->GetText().ToString();
		break;
	}

    text.Append(playerName).Append(TEXT(": ")).Append(GetEnhancedMoveNotation(targetChess, move)/* 获取操作 */).Append(TEXT("\n"));
    if (IsValid(Text_OperatingRecord)) Text_OperatingRecord->SetText(FText::FromString(text));
}

// 将棋子移动转换为基本中文棋谱。
FString UUI_Battle2P_Base::GetMoveNotation(TWeakObjectPtr<AChesses> targetChess, FChessMove2P move)
{
    if (!targetChess.IsValid() || !move.IsValid()) return FString();
    // 获取棋子名称
    FString chessName = targetChess->GetChessName();

    // 列号映射（从右到左：1-9）
    TMap<int32, FString> rowMap;
    rowMap.Add(0, TEXT("1"));
    rowMap.Add(1, TEXT("2"));
    rowMap.Add(2, TEXT("3"));
    rowMap.Add(3, TEXT("4"));
    rowMap.Add(4, TEXT("5"));
    rowMap.Add(5, TEXT("6"));
    rowMap.Add(6, TEXT("7"));
    rowMap.Add(7, TEXT("8"));
    rowMap.Add(8, TEXT("9"));
    rowMap.Add(9, TEXT("10"));

    // 行号映射（从下到上：一-十，红方视角）
    TMap<int32, FString> columnMapRed;
    columnMapRed.Add(8, UTF8_TO_TCHAR("一"));
    columnMapRed.Add(7, UTF8_TO_TCHAR("二"));
    columnMapRed.Add(6, UTF8_TO_TCHAR("三"));
    columnMapRed.Add(5, UTF8_TO_TCHAR("四"));
    columnMapRed.Add(4, UTF8_TO_TCHAR("五"));
    columnMapRed.Add(3, UTF8_TO_TCHAR("六"));
    columnMapRed.Add(2, UTF8_TO_TCHAR("七"));
    columnMapRed.Add(1, UTF8_TO_TCHAR("八"));
    columnMapRed.Add(0, UTF8_TO_TCHAR("九"));

    // 行号映射（从上到下：1-10，黑方视角）
    TMap<int32, FString> columnMapBlack;
    columnMapBlack.Add(8, TEXT("9"));
    columnMapBlack.Add(7, TEXT("8"));
    columnMapBlack.Add(6, TEXT("7"));
    columnMapBlack.Add(5, TEXT("6"));
    columnMapBlack.Add(4, TEXT("5"));
    columnMapBlack.Add(3, TEXT("4"));
    columnMapBlack.Add(2, TEXT("3"));
    columnMapBlack.Add(1, TEXT("2"));
    columnMapBlack.Add(0, TEXT("1"));

    // 选择行号映射
    TMap<int32, FString> columnMap = (targetChess->GetColor() == EChessColor::REDCHESS) ? columnMapRed : columnMapBlack;

    // 获取起始位置和目标位置
    FString fromColumn = columnMap.Contains(move.from.Y) ? columnMap[move.from.Y] : TEXT("?");
    FString fromRow = rowMap.Contains(move.from.X) ? rowMap[move.from.X] : TEXT("?");
    FString toColumn = columnMap.Contains(move.to.Y) ? columnMap[move.to.Y] : TEXT("?");
    FString toRow = rowMap.Contains(move.to.X) ? rowMap[move.to.X] : TEXT("?");

    // 确定移动方向
    FString direction;
    int32 deltaX = move.to.X - move.from.X;
    int32 deltaY = move.to.Y - move.from.Y;

    // 对于红方，向上移动是"进"，向下是"退"
    // 对于黑方，向下移动是"进"，向上是"退"（因为黑方在棋盘上方）
    if (targetChess->GetColor() == EChessColor::REDCHESS)
    {
        if (deltaX > 0)
        {
            direction = UTF8_TO_TCHAR("进");
        }
        else if (deltaX < 0)
        {
            direction = UTF8_TO_TCHAR("退");
        }
        else
        {
            direction = UTF8_TO_TCHAR("平");
        }
    }
    else // 黑方
    {
        if (deltaX < 0)
        {
            direction = UTF8_TO_TCHAR("进");
        }
        else if (deltaX > 0)
        {
            direction = UTF8_TO_TCHAR("退");
        }
        else
        {
            direction = UTF8_TO_TCHAR("平");
        }
    }

    // 构建棋谱记录
    FString notation;

    EChessType chessType = targetChess->GetType();
    // 特殊处理：当同一列有多个相同棋子时，需要区分是前一个还是后一个
    // 这里简化处理，只使用基本格式
    if (direction == UTF8_TO_TCHAR("平"))
    {
        // 平移动作：棋子名 + 起始列 + 平 + 目标列
        notation = FString::Printf(TEXT("%s%s%s%s"), *chessName, *fromColumn, *direction, *toColumn);
    }
    else
    {
        // 进/退动作
        if (chessType == EChessType::MA || chessType == EChessType::XIANG || chessType == EChessType::SHI)
        {
            // 马、象、士：棋子名 + 起始列 + 方向 + 目标列
            notation = FString::Printf(TEXT("%s%s%s%s"), *chessName, *fromColumn, *direction, *toColumn);
        }
        else
        {
            // 其他棋子：棋子名 + 起始列 + 方向 + 移动格数
            int32 moveSteps = FMath::Abs(deltaX) + FMath::Abs(deltaY); // 实际移动距离
            notation = FString::Printf(TEXT("%s%s%s%d"), *chessName, *fromRow, *direction, moveSteps);
        }
    }
    return notation;
}

// 增强版：考虑同列多个相同棋子的情况
FString UUI_Battle2P_Base::GetEnhancedMoveNotation(TWeakObjectPtr<AChesses> targetChess, FChessMove2P move)
{
    if (!GameState.IsValid() || !GameState->GetChessBoard2P().IsValid()) return FString(TEXT("无效走法"));

    TWeakObjectPtr<AChesses> movingChess = targetChess;
    if (!movingChess.IsValid()) return FString(TEXT("无效棋子"));

    EChessType chessType = movingChess->GetType();
    EChessColor chessColor = movingChess->GetColor();

    // 获取基本棋谱
    FString basicNotation = GetMoveNotation(targetChess, move);

    // 检查同一列是否有多个相同类型的棋子
    int32 sameTypeCount = 0;
    int32 currentIndex = 0;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GameState->GetChessBoard2P()->GetChess(i, j);
            if (chess.IsValid() &&
                chess->GetType() == chessType &&
                chess->GetColor() == chessColor &&
                j == move.from.Y) // 同一列
            {
                sameTypeCount++;
                if (i == move.from.X && j == move.from.Y)
                {
                    // 记录当前棋子的位置
                    currentIndex = sameTypeCount;
                }
            }
        }
    }

    // 如果同一列有多个相同棋子，需要添加前后标识
    if (sameTypeCount > 1)
    {
        FString positionMarker;
        if (chessColor == EChessColor::REDCHESS)
        {
            // 红方：前面的棋子用"前"，后面的用"后"
            if (currentIndex == 1)
            {
                positionMarker = TEXT("前");
            }
            else
            {
                positionMarker = TEXT("后");
            }
        }
        else
        {
            // 黑方：前面的棋子用"前"，后面的用"后"（从黑方视角）
            if (currentIndex == 1)
            {
                positionMarker = TEXT("前");
            }
            else
            {
                positionMarker = TEXT("后");
            }
        }

        // 修改棋谱：添加前后标识
        FString enhancedNotation = basicNotation;

        // 如果是车、马、炮、兵，且在同一列有多个，使用前后标识
        if (chessType == EChessType::JV || chessType == EChessType::MA ||
            chessType == EChessType::PAO || chessType == EChessType::BING)
        {
            enhancedNotation = positionMarker + basicNotation;
        }

        return enhancedNotation;
    }

    return basicNotation;
}
