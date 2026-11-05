#include "XQPOutlineStyle.h"
#include "XQPMobile.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"

namespace
{
    // 弱引用含对象序列号，不延长UI寿命，也不会把新对象误判为已适配的旧对象。
    TSet<TWeakObjectPtr<UWidget>> Applied;
    bool IsWhite(const FLinearColor& C)
    {
        return C.A > 0 && FMath::Min3(C.R,C.G,C.B) >= .65f && FMath::Max3(C.R,C.G,C.B)-FMath::Min3(C.R,C.G,C.B) <= .25f;
    }
    void Thicken(FSlateBrush& B)
    {
        if (B.OutlineSettings.Width > 0 && IsWhite(B.OutlineSettings.Color.GetSpecifiedColor())) B.OutlineSettings.Width *= 2.f;
    }
    void Thicken(FButtonStyle& S) { Thicken(S.Normal); Thicken(S.Hovered); Thicken(S.Pressed); Thicken(S.Disabled); }
}

void XQPUIStyle::ApplyMobileOutlines(UUserWidget* Widget)
{
    if (!Widget || !Widget->WidgetTree || !XQPMobile::IsPortrait(Widget)) return;
    for (auto It = Applied.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    Widget->WidgetTree->ForEachWidgetAndDescendants([](UWidget* Child)
    {
        if (!Child || Applied.Contains(Child)) return;
        Applied.Add(Child);
        if (auto* Text = Cast<UTextBlock>(Child))
        {
            auto Font = Text->GetFont();
            if (Font.OutlineSettings.OutlineSize > 0 && IsWhite(Font.OutlineSettings.OutlineColor))
            { Font.OutlineSettings.OutlineSize *= 2; Text->SetFont(Font); }
        }
        if (auto* Button = Cast<UButton>(Child)) { auto Style = Button->GetStyle(); Thicken(Style); Button->SetStyle(Style); }
        if (auto* Image = Cast<UImage>(Child)) { auto Brush = Image->GetBrush(); Thicken(Brush); Image->SetBrush(Brush); }
        if (auto* Border = Cast<UBorder>(Child)) { auto Brush = Border->Background; Thicken(Brush); Border->SetBrush(Brush); }
        if (auto* Check = Cast<UCheckBox>(Child))
        {
            auto Style = Check->GetWidgetStyle();
            Thicken(Style.UncheckedImage); Thicken(Style.UncheckedHoveredImage); Thicken(Style.UncheckedPressedImage);
            Thicken(Style.CheckedImage); Thicken(Style.CheckedHoveredImage); Thicken(Style.CheckedPressedImage);
            Thicken(Style.UndeterminedImage); Thicken(Style.UndeterminedHoveredImage); Thicken(Style.UndeterminedPressedImage);
            Check->SetWidgetStyle(Style);
        }
        if (auto* Combo = Cast<UComboBoxString>(Child))
        {
            auto Style = Combo->GetWidgetStyle();
            Thicken(Style.ComboButtonStyle.ButtonStyle); Thicken(Style.ComboButtonStyle.MenuBorderBrush);
            Combo->SetWidgetStyle(Style);
        }
    });
}
