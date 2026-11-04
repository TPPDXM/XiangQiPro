// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Jv.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Jv::AChess_Jv()
{
	MyType = EChessType::JV;
	MyRealType = EChessType::JV;
    // 原创短音色：木轮带金属颤音，普通落子和吃子使用不同力度的收尾。
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Rook/S_RookMove.S_RookMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Rook/S_RookLand.S_RookLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Rook/S_RookCapture.S_RookCapture"));

    // 原生类持有正式资源引用，使离线、联机和菜单演示中的车共用同一套走子特效。
    // 真正的 Android/iOS 不加载桌面版本；竖屏编辑器预览仍可按现有画质规则切换。
    MoveEffects->bChariotEffects = true;
    MoveEffects->TrailColor = FLinearColor(0.7f, 0.25f, 0.045f, 1.f);
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Rook/NS_RookCapture.NS_RookCapture"));
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Rook/NS_RookMoveTrail.NS_RookMoveTrail"));
#endif
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Rook/NS_RookCapture_Mobile.NS_RookCapture_Mobile"));
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Rook/NS_RookMoveTrail_Mobile.NS_RookMoveTrail_Mobile"));
    MoveEffects->LandingEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Rook/NS_RookLandingShock.NS_RookLandingShock"));
}

void AChess_Jv::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_JV));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_JV));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("俥");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("車");
	}
}

void AChess_Jv::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
