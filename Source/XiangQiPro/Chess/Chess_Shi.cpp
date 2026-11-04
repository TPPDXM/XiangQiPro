// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Shi.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Shi::AChess_Shi()
{
	MyType = EChessType::SHI;
	MyRealType = EChessType::SHI;
    // 地面动作与音效统一走真实移动/到达入口，复用自然完成、对象池和联机重复通知过滤。
    MoveEffects->GroundStyle = EChessGroundStyle::Advisor;
    MoveEffects->MaxSpeed = 35.f;
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Advisor/NS_AdvisorMoveTrail.NS_AdvisorMoveTrail"));
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Advisor/NS_AdvisorCapture.NS_AdvisorCapture"));
#endif
    // 移动端使用独立 CPU 版本；低档装饰层由质量参数关闭，主体符号始终保留。
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Advisor/NS_AdvisorMoveTrail_Mobile.NS_AdvisorMoveTrail_Mobile"));
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Advisor/NS_AdvisorCapture_Mobile.NS_AdvisorCapture_Mobile"));
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Advisor/S_AdvisorMove.S_AdvisorMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Advisor/S_AdvisorLand.S_AdvisorLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Advisor/S_AdvisorCapture.S_AdvisorCapture"));

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
