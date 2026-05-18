// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Xiang.h"

AChess_Xiang::AChess_Xiang()
{
	MyType = EChessType::XIANG;
	MyRealType = EChessType::XIANG;
}

void AChess_Xiang::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_XIANG));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_XIANG));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("相");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("象");
	}
}

void AChess_Xiang::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
