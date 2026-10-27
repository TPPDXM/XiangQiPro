// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArrayToolLibrary.generated.h"

/**
 * 数组工具库
 */
UCLASS()
class XIANGQIPRO_API UArrayToolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    // 原地洗牌，不创建额外数组。
    template<typename Type>
    static inline void ShuffleArray(TArray<Type>& Array)
    {
        if (Array.Num() <= 1) return;

        for (int32 i = Array.Num() - 1; i > 0; i--)
        {
            int32 j = FMath::RandRange(0, i);
            Array.Swap(i, j);
        }
    }

    // 尝试随机取值，空数组返回false且保持输出参数不变。
    template<typename Type>
    static bool TryGetRandomElement(const TArray<Type>& Array, Type& OutElement)
    {
        if (Array.IsEmpty()) return false;
        OutElement = Array[FMath::RandRange(0, Array.Num() - 1)];
        return true;
    }

    // 兼容旧接口，空数组返回默认值，业务代码可使用Try版本区分失败。
    template<typename Type>
    static inline Type GetRandomElement(const TArray<Type>& Array)
    {
        if (Array.IsEmpty())
            return Type{};

        int32 seed = FMath::RandRange(0, Array.Num() - 1);
        return Array[seed];
    }
};
