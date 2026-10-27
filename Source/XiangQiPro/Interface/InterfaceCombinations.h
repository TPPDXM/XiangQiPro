// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"

namespace XQPInterfaces
{
    // 先收集弱引用快照，回调销毁对象后不会再访问失效指针。
    inline TArray<TWeakObjectPtr<UObject>> Collect(UObject* Context, TSubclassOf<UInterface> InterfaceClass)
    {
        TArray<TWeakObjectPtr<UObject>> Objects;
        if (!IsValid(Context)) return Objects;
        TArray<AActor*> Actors;
        TArray<UUserWidget*> Widgets;
        UGameplayStatics::GetAllActorsWithInterface(Context, InterfaceClass, Actors);
        UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(Context, Widgets, InterfaceClass, false);
        for (AActor* Actor : Actors) Objects.Add(Actor);
        for (UUserWidget* Widget : Widgets) Objects.Add(Widget);
        return Objects;
    }
}

// 调用无参数接口事件，原生包装函数负责继续触发蓝图事件。
#define CALL_INTERFACE_EVENT(InterfaceClass, EventName) \
do { \
    const auto InterfaceObjects = XQPInterfaces::Collect(this, U##InterfaceClass::StaticClass()); \
    for (const auto& WeakObject : InterfaceObjects) \
    { \
        UObject* Object = WeakObject.Get(); \
        if (!IsValid(Object)) continue; \
        if (I##InterfaceClass* Interface = Cast<I##InterfaceClass>(Object)) Interface->EventName(Object); \
        else I##InterfaceClass::Execute_##EventName(Object); \
    } \
} while (false)

// 调用带参数接口事件，不对不同TArray元素类型进行强制转换。
#define CALL_INTERFACE_EVENT_PARAM(InterfaceClass, EventName, ...) \
do { \
    const auto InterfaceObjects = XQPInterfaces::Collect(this, U##InterfaceClass::StaticClass()); \
    for (const auto& WeakObject : InterfaceObjects) \
    { \
        UObject* Object = WeakObject.Get(); \
        if (!IsValid(Object)) continue; \
        if (I##InterfaceClass* Interface = Cast<I##InterfaceClass>(Object)) Interface->EventName(Object, __VA_ARGS__); \
        else I##InterfaceClass::Execute_##EventName(Object, __VA_ARGS__); \
    } \
} while (false)
