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

    template<typename Type>
    static inline void ShuffleArray(TArray<Type>& Array)
    {
        if (Array.Num() <= 1) return;

        for (int32 i = Array.Num() - 1; i > 0; i--)
        {
            int32 j = FMath::Rand() % (i + 1);
            Array.Swap(i, j);
        }
    }

    template<typename Type>
    static inline Type GetRandomElement(const TArray<Type>& Array)
    {
        if (Array.IsEmpty())
            throw TEXT("Array is empty!");

        int32 seed = FMath::RandRange(0, Array.Num() - 1);
        return Array[seed];
    }
};
