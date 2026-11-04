// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Ma.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Ma::AChess_Ma()
{
	MyType = EChessType::MA;
	MyRealType = EChessType::MA;
    // 马采用世界空间的短促腾跃：靛蓝分股疾风、流云蹄印与成对蹬尘。
    // 与士的青玉护印/短剑光区分颜色、轮廓和发射节奏，步点仍按真实移动进度触发。
    MoveEffects->bKnightEffects = true;
    // 靛蓝保留蓝紫色相；材质将风带分成疏密变化的双股，减少与士的实心青玉拖尾重合。
    // 金属蹄铁使用独立受光材质，不从这份自发光风色派生金属颜色。
    MoveEffects->TrailColor = FLinearColor(0.10f, 0.12f, 0.48f, 1.f);
    // 灰褐土尘和棋盘亮木色拉开明度差；加深色相不增加粒子数量，也不影响各档位的数量预算。
    MoveEffects->DustColor = FLinearColor(0.28f, 0.205f, 0.12f, 1.f);
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->KnightMotionTrail = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightMotionTrail.NS_KnightMotionTrail"));
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightMoveTrail.NS_KnightMoveTrail"));
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightCapture.NS_KnightCapture"));
#endif
    MoveEffects->MobileKnightMotionTrail = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightMotionTrail_Mobile.NS_KnightMotionTrail_Mobile"));
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightMoveTrail_Mobile.NS_KnightMoveTrail_Mobile"));
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Knight/NS_KnightCapture_Mobile.NS_KnightCapture_Mobile"));
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Knight/S_KnightMove.S_KnightMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Knight/S_KnightLand.S_KnightLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Knight/S_KnightCapture.S_KnightCapture"));
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
