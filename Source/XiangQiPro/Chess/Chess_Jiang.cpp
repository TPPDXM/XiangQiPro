// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Jiang.h"

AChess_Jiang::AChess_Jiang() : Super()
{
	MyType = EChessType::JIANG;
	MyRealType = EChessType::JIANG;
}

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

void AChess_Jiang::Defeated()
{
	Super::Defeated();
	GameState->NotifyGameOver(MyColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS); // 通知游戏状态，游戏获胜
}

void AChess_Jiang::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
