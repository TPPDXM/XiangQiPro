// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Pao.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

AChess_Pao::AChess_Pao()
{
	MyType = EChessType::PAO;
	MyRealType = EChessType::PAO;
    // 喷火起音和命中爆响分开触发，避免远距离吃子时爆响提前于落点。
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Cannon/S_CannonMove.S_CannonMove"));
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Cannon/S_CannonLand.S_CannonLand"));
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(TEXT("/Game/Audio/Chess/Cannon/S_CannonCapture.S_CannonCapture"));

    // 强引用使炮特效随原生棋子类进入 Cook；其他棋子仍继承基类的空配置。
    // 移动端不加载 GPU 发射器运行，改用同风格、低粒子预算的独立 CPU 系统。
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    // 真正的移动端进程不加载 PC GPU 资源，减少棋子类默认对象的常驻资源和着色器开销。
    // 编辑器仍保留两组引用，PC/竖屏预览可在同一进程中按已有界面规则切换。
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Cannon/NS_CannonCapture.NS_CannonCapture"));
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Cannon/NS_CannonMoveTrail.NS_CannonMoveTrail"));
#endif
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Cannon/NS_CannonCapture_Mobile.NS_CannonCapture_Mobile"));
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Cannon/NS_CannonMoveTrail_Mobile.NS_CannonMoveTrail_Mobile"));
    MoveEffects->LandingEffect = OM::GetConstructorObject<UNiagaraSystem>(TEXT("/Game/Niagara/Cannon/NS_CannonLandingFlash.NS_CannonLandingFlash"));
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
