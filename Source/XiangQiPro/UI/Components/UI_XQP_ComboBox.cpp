// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_XQP_ComboBox.h"
#include "Styling/CoreStyle.h"

void UUI_XQP_ComboBox::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (!ComboBox) return;
    FComboBoxStyle Style = ComboBox->GetWidgetStyle();
    Style.ComboButtonStyle.DownArrowImage = *FCoreStyle::Get().GetBrush("ComboButton.Arrow");
    Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(FLinearColor(.83f, .69f, .35f));
    ComboBox->SetWidgetStyle(Style);
}

