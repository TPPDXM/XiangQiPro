#include "XQPDPISettings.h"
#include "Engine/UserInterfaceSettings.h"

float UXQPDPISettings::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
    if (Size.X > 0 && Size.Y > Size.X) return FMath::Max(.1f, Size.X / 420.f);
    return FMath::Max(.1f, GetDefault<UUserInterfaceSettings>()->UIScaleCurve.GetRichCurveConst()->Eval(FMath::Min(Size.X, Size.Y)));
}
