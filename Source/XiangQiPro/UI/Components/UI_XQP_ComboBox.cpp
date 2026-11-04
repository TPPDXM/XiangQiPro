// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_XQP_ComboBox.h"
#include "XiangQiPro/Audio/XQPFeedbackAudioSubsystem.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "Widgets/Input/SButton.h"
#include "Layout/Children.h"

UUI_XQP_ComboBox::UUI_XQP_ComboBox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Arrow(PATH_TEXTURE_COMBO_ARROW);
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


void UUI_XQP_ComboBox::NativeConstruct()
{
    Super::NativeConstruct();
    UXQPFeedbackAudioSubsystem::ConfigureFor(this);
    if (!ComboBox || !XQPMobile::IsPortrait(this)) return;
    // SComboButton 的鼠标策略本来就是 MouseDown，但默认触摸策略为 DownAndUp。
    // 菜单在第二次按下时被 Slate 关闭，抬手跨帧后 bDismissedThisTick 已清除，因而又打开。
    // 仅把触摸策略与原鼠标策略对齐；选项点击、列表滚动和 PC 行为仍由原控件处理。
    const TFunction<bool(const TSharedRef<SWidget>&)> ConfigureButton = [&](const TSharedRef<SWidget>& Widget)
    {
        if (Widget->GetType() == FName(TEXT("SButton")))
        {
            StaticCastSharedRef<SButton>(Widget)->SetTouchMethod(EButtonTouchMethod::Down);
            return true;
        }
        FChildren* Children = Widget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (ConfigureButton(Children->GetChildAt(Index))) return true;
        return false;
    };
    ConfigureButton(ComboBox->TakeWidget());
}
