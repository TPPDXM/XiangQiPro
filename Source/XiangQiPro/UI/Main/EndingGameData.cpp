// Copyright 2026 Ultimate Player All Rights Reserved.


#include "EndingGameData.h"
#include "UI_Introduce_Ending.h"
#include "XiangQiPro/Util/EndingLibrary.h"

// 保存关卡数据和选择来源的弱引用。
void UEndingGameData::Init(int32 InIndex, UUI_Introduce_Ending* InSelectionOwner, FOnEndingGameListItemClicked CallBackFunc)
{
	Index = InIndex;
	SelectionOwner = InSelectionOwner;
	OnItemClickedDelegate = CallBackFunc;

	Title = UEndingLibrary::GetEndingGameTitle(Index);
}
