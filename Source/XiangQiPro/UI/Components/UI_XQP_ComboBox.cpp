// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_XQP_ComboBox.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

UUI_XQP_ComboBox::UUI_XQP_ComboBox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Arrow(TEXT("/Game/UMG/Texture/T_ComboArrow"));
    ArrowTexture = Arrow.Object;
}

void UUI_XQP_ComboBox::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (!ComboBox) return;
    FComboBoxStyle Style = ComboBox->GetWidgetStyle();
    Style.ComboButtonStyle.DownArrowImage.SetResourceObject(ArrowTexture);
    Style.ComboButtonStyle.DownArrowImage.DrawAs = ESlateBrushDrawType::Image;
    Style.ComboButtonStyle.DownArrowImage.ImageType = ESlateBrushImageType::FullColor;
    Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(FLinearColor(.83f, .69f, .35f));
    ComboBox->SetWidgetStyle(Style);
}

