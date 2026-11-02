#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

// Shared by UI and cameras so the board always fits the same usable screen area.
namespace XQPMobile
{
    XIANGQIPRO_API bool IsPortrait(const UObject* Context);
    XIANGQIPRO_API FVector2D ViewportSize(const UObject* Context);
    XIANGQIPRO_API FMargin SafeInsets(const UObject* Context);
    XIANGQIPRO_API FBox2D BoardRect(const UObject* Context);
    XIANGQIPRO_API UClass* ResolveWidgetClass(const UObject* Context, UClass* WidgetClass);
    // UI保持原生像素，棋盘渲染比例、纹理采样随画质变化，低档保留720p下限。
    XIANGQIPRO_API void ApplyNativeResolution(const UObject* Context);

    template <typename WidgetType, typename OwnerType>
    WidgetType* CreateWidget(OwnerType* Owner, TSubclassOf<UUserWidget> WidgetClass)
    {
        return Owner && WidgetClass ? ::CreateWidget<WidgetType>(Owner, ResolveWidgetClass(Owner, WidgetClass.Get())) : nullptr;
    }
}
