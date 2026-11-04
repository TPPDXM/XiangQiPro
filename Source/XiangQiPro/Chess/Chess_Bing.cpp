// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Bing.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Bing::AChess_Bing()
{
	MyType = EChessType::BING; 
	MyRealType = EChessType::BING;
    // 地面动作与音效统一走真实移动/到达入口，复用自然完成、对象池和联机重复通知过滤。
    MoveEffects->GroundStyle = EChessGroundStyle::Pawn;
    MoveEffects->MaxSpeed = 35.f;
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Pawn/NS_PawnMoveTrail.NS_PawnMoveTrail"));
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Pawn/NS_PawnCapture.NS_PawnCapture"));
#endif
    // 移动端使用独立 CPU 版本；低档装饰层由质量参数关闭，主体符号始终保留。
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Pawn/NS_PawnMoveTrail_Mobile.NS_PawnMoveTrail_Mobile"));
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Pawn/NS_PawnCapture_Mobile.NS_PawnCapture_Mobile"));
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Pawn/S_PawnMove.S_PawnMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Pawn/S_PawnLand.S_PawnLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Pawn/S_PawnCapture.S_PawnCapture"));

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
