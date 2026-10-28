// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Jiang.h"

// 初始化默认属性和组件。
AChess_Jiang::AChess_Jiang() : Super()
{
	MyType = EChessType::JIANG;
	MyRealType = EChessType::JIANG;
}

// 初始化将帅名称和阵营材质。
void AChess_Jiang::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_JIANG));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_JIANG));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("帥");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("將");
	}
}

// 播放棋子被吃效果，并处理派生棋子的结束逻辑。
void AChess_Jiang::Defeated()
{
	Super::Defeated();
	if (GameState.IsValid() && !GameState->IsProBattle()) GameState->NotifyGameOver(MyColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);
}

// 生成选中棋子的合法走法并显示提示。
void AChess_Jiang::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
