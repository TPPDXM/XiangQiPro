#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/Util/ChessInfo.h"

namespace XQPUIStyle
{
inline FLinearColor Red() { return FLinearColor(.58f, .02f, .015f); }
inline FLinearColor Black() { return FLinearColor(.018f, .022f, .028f); }
inline FLinearColor Paper() { return FLinearColor(.88f, .83f, .72f, .96f); }
// 与菜单金色边框、箭头保持一致，观战结果不根据任一方胜负变色。
inline FLinearColor Gold() { return FLinearColor(.83f, .69f, .35f); }

// 名牌保留完整文本作为数据，只在绘制时省略尾部，避免长名字撑开布局。
inline void ConstrainPlayerName(UTextBlock* Text)
{
    if (!Text) return;
    Text->SetAutoWrapText(false);
    Text->SetWrapTextAt(0.f);
    Text->SetMinDesiredWidth(0.f);
    Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Text->SetClipping(EWidgetClipping::ClipToBounds);
}
inline FLinearColor Victory() { return FLinearColor(1.f, .68f, .14f); }
inline FLinearColor Defeat() { return FLinearColor(.94f, .16f, .12f); }

inline void SetFactionText(UTextBlock* Text, EChessColor Color, bool bLightBackground = false)
{
    if (!Text) return;
    const bool bRed = Color == EChessColor::REDCHESS;
    const FLinearColor Ink = bRed ? (bLightBackground ? Red() : FLinearColor(.95f, .10f, .075f)) : Black();
    Text->SetColorAndOpacity(FSlateColor(Ink));
    FSlateFontInfo Font = Text->GetFont();
    Font.OutlineSettings.OutlineSize = bLightBackground ? 0 : (bRed ? 1 : 2);
    Font.OutlineSettings.OutlineColor = bRed ? FLinearColor(.025f, .005f, .005f, 1.f) : Paper();
    Text->SetFont(Font);
}
}
