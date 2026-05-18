// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

/**
 *  INTERFACE PARAMETER COMBINATIONS
 *  -----------------------------------------------------------------------------------------------
 *
 *	This file defines the different combinations of macros used to declare different interface types.
 *	which should be included in cpp files that need access to these macros.
 */

#include <Kismet/GameplayStatics.h>
#include <Blueprint/WidgetBlueprintLibrary.h>

 // 函数接口调用
#define CALL_INTERFACE_EVENT(InterfaceClass, EventName) \
TArray<AActor*> AllActors; \
TArray<UUserWidget*> AllWidgets; \
\
/* 获取所有继承接口的对象 */ \
UGameplayStatics::GetAllActorsWithInterface(this, U##InterfaceClass::StaticClass(), AllActors); \
UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(this, AllWidgets, U##InterfaceClass::StaticClass(), false); \
\
TArray<UObject*> AllObject(AllActors); \
AllObject.Append((const TArray<UObject*>&) AllWidgets); \
\
for (UObject* object : AllObject) \
{ \
    I##InterfaceClass* IF = Cast<I##InterfaceClass>(object); \
    if (!IF) \
    { \
        /* 直接调用蓝图中的函数 */ \
        IF->Execute_##EventName(object); \
        continue; \
    } \
    \
    IF->EventName(object); \
} \

// 支持带参数的接口调用
#define CALL_INTERFACE_EVENT_PARAM(InterfaceClass, EventName, ...) \
TArray<AActor*> AllActors; \
TArray<UUserWidget*> AllWidgets; \
\
/* 获取所有继承接口的对象 */ \
UGameplayStatics::GetAllActorsWithInterface(this, U##InterfaceClass::StaticClass(), AllActors); \
UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(this, AllWidgets, U##InterfaceClass::StaticClass(), false); \
\
TArray<UObject*> AllObject(AllActors); \
AllObject.Append((const TArray<UObject*>&) AllWidgets); \
\
for (UObject* object : AllObject) \
{ \
    I##InterfaceClass* IF = Cast<I##InterfaceClass>(object); \
    if (!IF) \
    { \
        /* 直接调用蓝图中的函数，支持参数传递 */ \
        IF->Execute_##EventName(object, __VA_ARGS__); \
        continue; \
    } \
    \
    IF->EventName(object, __VA_ARGS__); \
} \
