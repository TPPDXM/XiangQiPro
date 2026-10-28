#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "UObject/UnrealType.h"

// 复用WB_Btn的内部按钮与标题；材质、字体和交互效果完全由原UMG组件提供。
namespace XQPButtonBinding
{
inline UButton* GetButton(UUserWidget* Component)
{
    return Component ? Cast<UButton>(Component->GetWidgetFromName(TEXT("XQPBtn"))) : nullptr;
}

inline void SetVisibility(UUserWidget* Component, ESlateVisibility Visibility)
{
    if (!Component) return;
    Component->SetVisibility(Visibility);
    // 设计器用SizeBox保留原按钮的320×70尺寸，隐藏时一并收起占位。
    if (auto* Frame = Cast<USizeBox>(Component->GetParent())) Frame->SetVisibility(Visibility);
}

inline void SetCaption(UUserWidget* Component, const FText& Caption)
{
    if (!Component) return;
    // 同步原组件公开的BtnText，防止蓝图PreConstruct再次执行时覆盖动态标题。
    if (auto* Property = FindFProperty<FTextProperty>(Component->GetClass(), TEXT("BtnText")))
        Property->SetPropertyValue_InContainer(Component, Caption);
    if (auto* Label = Cast<UTextBlock>(Component->GetWidgetFromName(TEXT("XQPBtn_Text"))))
        if (!Label->GetText().EqualTo(Caption)) Label->SetText(Caption);
}
}
