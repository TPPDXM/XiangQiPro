// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Xiang.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Xiang::AChess_Xiang()
{
	MyType = EChessType::XIANG;
	MyRealType = EChessType::XIANG;
    // 地面动作与音效统一走真实移动/到达入口，复用自然完成、对象池和联机重复通知过滤。
    MoveEffects->GroundStyle = EChessGroundStyle::Elephant;
    MoveEffects->MaxSpeed = 35.f;
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Elephant/NS_ElephantMoveTrail.NS_ElephantMoveTrail"));
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Elephant/NS_ElephantCapture.NS_ElephantCapture"));
#endif
    // 移动端使用独立 CPU 版本；低档装饰层由质量参数关闭，主体符号始终保留。
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Elephant/NS_ElephantMoveTrail_Mobile.NS_ElephantMoveTrail_Mobile"));
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Elephant/NS_ElephantCapture_Mobile.NS_ElephantCapture_Mobile"));
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Elephant/S_ElephantMove.S_ElephantMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Elephant/S_ElephantLand.S_ElephantLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Elephant/S_ElephantCapture.S_ElephantCapture"));

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
