// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "Logger.h"

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectManager.generated.h"

typedef UObjectManager OM;

/**
 * A class for loading assets from local data
 */
UCLASS()
class XIANGQIPRO_API UObjectManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// 按路径加载资源，路径为空时直接返回空引用。
	template <typename T>
	static T* GetObject(const TCHAR* ObjectToFind, UObject* Outer = nullptr)
	{
		return ObjectToFind && *ObjectToFind ? LoadObject<T>(Outer, ObjectToFind) : nullptr;
	}

	// 按路径加载指定基类的蓝图类型。
	template <typename T>
	static UClass* GetBlueprint(const TCHAR* ClassToFind, UObject* Outer = nullptr)
	{
		return ClassToFind && *ClassToFind ? LoadClass<T>(Outer, ClassToFind) : nullptr;
	}

	// 在构造阶段加载默认资源，可选输出加载失败日志。
	template <typename T>
	static T* GetConstructorObject(const TCHAR* ObjectToFind, bool bPrintLog = false)
	{
		if (!ObjectToFind || !*ObjectToFind) return nullptr;
		ConstructorHelpers::FObjectFinder<T> Obj(ObjectToFind);
		if (bPrintLog)
		{
			if (!Obj.Succeeded())
			{
				ULogger::LogWarning(TEXT("FClassFinder"), FString("Fail to find: ").Append(ObjectToFind));
			}
		}
		return Obj.Object;
	}

	// 在构造阶段加载默认蓝图类型。
	template <typename T>
	static TSubclassOf<T> GetConstructorBlueprint(const TCHAR* ClassToFind, bool bPrintLog = false)
	{
		if (!ClassToFind || !*ClassToFind) return nullptr;
		ConstructorHelpers::FClassFinder<T> BP(ClassToFind);
		if (bPrintLog)
		{
			if (!BP.Succeeded())
			{
				ULogger::LogWarning(TEXT("FClassFinder"), FString("Fail to find: ").Append(ClassToFind));
			}
		}
		return BP.Class;
	}
};
