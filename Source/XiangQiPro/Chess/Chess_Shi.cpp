// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Shi.h"

AChess_Shi::AChess_Shi()
{
	MyType = EChessType::SHI;
	MyRealType = EChessType::SHI;
}

void AChess_Shi::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_SHI));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_SHI));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("仕");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("士");
	}
}

void AChess_Shi::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
