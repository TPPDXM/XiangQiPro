#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/UI/XQPMobile.h"

namespace XQPUIStyle
{
inline FLinearColor Red() { return FLinearColor(.58f, .02f, .015f); }
inline FLinearColor Black() { return FLinearColor(.018f, .022f, .028f); }
inline FLinearColor Paper() { return FLinearColor(.88f, .83f, .72f, .96f); }
// 与菜单金色边框、箭头保持一致，观战结果不根据任一方胜负变色。
inline FLinearColor Gold() { return FLinearColor(.83f, .69f, .35f); }

// 名牌保留完整姓名，布局只绘制单行并以省略号收尾。
// 字体描边、斜体边缘和阴影可能超出字形的排版矩形，因此预留绘制边距，
// 裁切边界保留在包含安全边距的控件外沿，省略宽度也会扣除这些边距。
// 不能取消本层裁切：Slate 的省略绘制还会参考裁切区域，继承整屏区域会让长名溢出卡片。
inline void ConstrainPlayerName(UTextBlock* Text)
{
    if (!Text) return;
    Text->SetAutoWrapText(false);
    Text->SetWrapTextAt(0.f);
    Text->SetMinDesiredWidth(0.f);
    Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    const float InkPadding = FMath::Max(2.f, float(Text->GetFont().OutlineSettings.OutlineSize) + 1.f);
    const FVector2D Shadow = Text->GetShadowOffset();
    Text->SetMargin(FMargin(
        InkPadding + FMath::Max(0.f, -Shadow.X),
        InkPadding + FMath::Max(0.f, -Shadow.Y),
        InkPadding + FMath::Max(0.f, Shadow.X),
        InkPadding + FMath::Max(0.f, Shadow.Y)));
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
    // 动态阵营文字在每次刷新时都会重建字体，必须在此同步移动端的浅色描边规则。
    if (!bRed && !bLightBackground && XQPMobile::IsPortrait(Text)) Font.OutlineSettings.OutlineSize *= 2;
    Text->SetFont(Font);
}
}
