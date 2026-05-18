// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Ma.h"

AChess_Ma::AChess_Ma()
{
	MyType = EChessType::MA;
	MyRealType = EChessType::MA;
}

void AChess_Ma::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_MA));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_MA));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("傌");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("馬");
	}
}

void AChess_Ma::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
