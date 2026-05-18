// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Bing.h"

AChess_Bing::AChess_Bing()
{
	MyType = EChessType::BING; 
	MyRealType = EChessType::BING;
}

void AChess_Bing::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_BING));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_BING));
	}

	if (color == EChessColor::BLACKCHESS)
	{
		MyName = UTF8_TO_TCHAR("卒");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("兵");
	}
}

void AChess_Bing::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
