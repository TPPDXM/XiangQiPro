// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Battle2P_Base.h"
#include "XiangQiPro/UI/XQP_HUD.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/Interface/IF_GameState.h"

#include "XiangQiPro/GameMode/XQPGameStateBase.h"

// 构造界面并初始化控件与事件绑定。
void UUI_Battle2P_Base::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateCanTick();
	GameState = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
}

// 更新界面每帧状态。
void UUI_Battle2P_Base::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

// 刷新双方得分控件。
void UUI_Battle2P_Base::UpdateScore(int32 score1, int32 score2)
{
	if (IsValid(Text_Score_P1)) Text_Score_P1->SetText(FText::FromString(FString::FromInt(score1)));
	if (IsValid(Text_Score_P2)) Text_Score_P2->SetText(FText::FromString(FString::FromInt(score2)));
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
