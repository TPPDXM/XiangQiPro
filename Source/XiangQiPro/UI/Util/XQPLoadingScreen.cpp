#include "XQPLoadingScreen.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class SLoadingMoves : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SLoadingMoves) {} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        StartTime = FPlatformTime::Seconds();
        SetCanTick(false);
        ForceVolatile(true);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(80, 80); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        const double Time = FPlatformTime::Seconds() - StartTime;
        const FVector2D Center = Geometry.GetLocalSize() * .5f;
        const FLinearColor Gold(.52f, .38f, .19f, .35f);
        for (int32 Index = -1; Index <= 1; ++Index)
        {
            const float Offset = Index * 24.f;
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
                {Center + FVector2D(-24, Offset), Center + FVector2D(24, Offset)}, ESlateDrawEffect::None, Gold, true, .8f);
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
                {Center + FVector2D(Offset, -24), Center + FVector2D(Offset, 24)}, ESlateDrawEffect::None, Gold, true, .8f);
        }
        const FVector2D Corners[] = {{-24,-24}, {24,-24}, {24,24}, {-24,24}};
        const double Step = Time / .95;
        const int32 Index = FMath::FloorToInt(Step) % 4;
        const float T = FMath::SmoothStep(0.0, 1.0, FMath::Frac(Step));
        const FVector2D Position = Center + FMath::Lerp(Corners[Index], Corners[(Index + 1) % 4], T);
        static const FSlateRoundedBoxBrush Stone(FLinearColor::White, 8.f);
        const float Pulse = .8f + .2f * FMath::Sin(Time * 2.8);
        FSlateDrawElement::MakeBox(Elements, Layer + 1,
            Geometry.ToPaintGeometry(FVector2D(12,12), FSlateLayoutTransform(Center - FVector2D(6,6))),
            &Stone, ESlateDrawEffect::None, FLinearColor(.70f,.58f,.38f,Pulse));
        FSlateDrawElement::MakeBox(Elements, Layer + 2,
            Geometry.ToPaintGeometry(FVector2D(14,14), FSlateLayoutTransform(Position - FVector2D(7,7))),
            &Stone, ESlateDrawEffect::None, FLinearColor(.48f,.095f,.065f,1.f));
        return Layer + 2;
    }
private:
    double StartTime = 0;
};
}

TSharedRef<SWidget> MakeXQPLoadingScreen()
{
    return SNew(SOverlay)
        + SOverlay::Slot()
        [SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(.014f,.011f,.012f,1.f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SBox).WidthOverride(80).HeightOverride(80)[SNew(SLoadingMoves)]]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,18,0,0)
            [SNew(STextBlock).Text(FText::FromString(TEXT("正在布棋")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(FLinearColor(.76f,.67f,.52f,1.f))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,10,0,0)
            [SNew(STextBlock).Text(FText::FromString(TEXT("片刻，即入弈境")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(FLinearColor(.37f,.33f,.28f,1.f))]
        ];
}
