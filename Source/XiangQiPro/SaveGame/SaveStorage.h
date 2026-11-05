#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "XiangQiPro/GameMode/XQPGameTypes.h"
#include "SaveStorage.generated.h"

UENUM()
enum class EXQPSaveDataType : uint8
{
    PlayerProfile,
    Progress,
    Arrangements
};

// 数据类别决定序列化类型，模式只决定命名空间，二者不能互相推断。
struct FXQPSaveKey
{
    EXQPSaveDataType DataType;
    EXQPGameMode GameMode = EXQPGameMode::Default;
    int32 UserIndex = 0;
    FString SlotName(int32 Index) const;
};

UCLASS(Abstract)
class XIANGQIPRO_API UXQPSaveData : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) int64 Generation = 0;
};

namespace XQPSaveStorage
{
    XIANGQIPRO_API USaveGame* LoadSlot(const FString& SlotName, int32 UserIndex, bool bAllowLegacy = false);
    // 同一数据类型使用相同的双槽恢复策略；损坏或未知版本由类型自身校验拒绝。
    template <typename T, typename Validator>
    T* Load(const FXQPSaveKey& Key, Validator IsValidData)
    {
        T* Best = nullptr;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (!UGameplayStatics::DoesSaveGameExist(Key.SlotName(Index), Key.UserIndex)) continue;
            T* Loaded = Cast<T>(LoadSlot(Key.SlotName(Index), Key.UserIndex));
            if (Loaded && Loaded->Generation >= 0 && IsValidData(Loaded) &&
                (!Best || Loaded->Generation > Best->Generation)) Best = Loaded;
        }
        return Best;
    }

    XIANGQIPRO_API bool Save(UXQPSaveData* Data, const FXQPSaveKey& Key);
}
