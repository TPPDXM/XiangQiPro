// Copyright 2026 Ultimate Player All Rights Reserved.


#include "Chess_Jiang.h"
#include "ChessMoveEffectsComponent.h"
#include "Sound/SoundBase.h"

// 初始化默认属性和组件。
AChess_Jiang::AChess_Jiang() : Super()
{
	MyType = EChessType::JIANG;
	MyRealType = EChessType::JIANG;
    // 地面动作与音效统一走真实移动/到达入口，复用自然完成、对象池和联机重复通知过滤。
    MoveEffects->GroundStyle = EChessGroundStyle::Royal;
    MoveEffects->MaxSpeed = 35.f;
#if !PLATFORM_ANDROID && !PLATFORM_IOS
    MoveEffects->MoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_MOVE);
    MoveEffects->CaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_CAPTURE);
    MoveEffects->LandingEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_LANDING);
#endif
    // 移动端使用独立 CPU 版本；低档装饰层由质量参数关闭，主体符号始终保留。
    MoveEffects->MobileMoveTrailEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_MOVE_MOBILE);
    MoveEffects->MobileCaptureEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_CAPTURE_MOBILE);
    // 落印独立于移动祥云和吃子冲击，普通走子与吃子下降完成后均只播放一次。
    MoveEffects->MobileLandingEffect = OM::GetConstructorObject<UNiagaraSystem>(PATH_NS_ROYAL_LANDING_MOBILE);
    MoveEffects->MoveSound = OM::GetConstructorObject<USoundBase>(PATH_AUDIO_ROYAL_MOVE);
    MoveEffects->LandingSound = OM::GetConstructorObject<USoundBase>(PATH_AUDIO_ROYAL_LAND);
    MoveEffects->CaptureSound = OM::GetConstructorObject<USoundBase>(PATH_AUDIO_ROYAL_CAPTURE);

}

// 初始化将帅名称和阵营材质。
void AChess_Jiang::Init(EChessColor color, Position pos, TWeakObjectPtr<UChessBoard2P> board2P)
{
	Super::Init(color, pos, board2P);
	if (color == EChessColor::REDCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_RAD_JIANG));
	}
	else if (color == EChessColor::BLACKCHESS)
	{
		ChessMask->SetDecalMaterial(OM::GetObject<UMaterialInterface>(PATH_MI_CHESSMASK_BLACK_JIANG));
	}

	if (color == EChessColor::REDCHESS)
	{
		MyName = UTF8_TO_TCHAR("帥");
	}
	else
	{
		MyName = UTF8_TO_TCHAR("將");
	}
}

// 播放棋子被吃效果，并处理派生棋子的结束逻辑。
void AChess_Jiang::Defeated()
{
	Super::Defeated();
	if (GameState.IsValid() && !GameState->IsFengHuoBattle()) GameState->NotifyGameOver(MyColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);
}

// 生成选中棋子的合法走法并显示提示。
void AChess_Jiang::GenerateMove2P(TWeakObjectPtr<UChessBoard2P> board2P, TWeakObjectPtr<AChesses> target)
{
	Super::GenerateMove2P(board2P, target);
}
