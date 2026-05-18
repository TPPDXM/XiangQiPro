// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Pao.h"

AChess_Pao::AChess_Pao()
{
	MyType = EChessType::PAO;
	MyRealType = EChessType::PAO;
}

void AChess_Pao::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_PAO));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_PAO));
	}

	MyName = UTF8_TO_TCHAR("炮");
}

void AChess_Pao::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
