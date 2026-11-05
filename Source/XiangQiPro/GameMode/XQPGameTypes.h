#pragma once

#include "CoreMinimal.h"
#include "XQPGameTypes.generated.h"

// 规则类型与运行方式（AI、残局、联机）分开，协议和存储使用稳定的规则标识。
UENUM(BlueprintType)
enum class EXQPGameType : uint8
{
    Classic = 0,
    FengHuo = 1
};

UENUM(BlueprintType)
enum class EXQPGameMode : uint8
{
    Default = 0,
    AI2P = 1,
    Ending = 2,
    SoloRide = 3,
    GuessWho = 4,
    LAN = 5,
    FengHuoAI = 6,
    FengHuoLAN = 7,
    Reenactment = 8
};

struct FXQPLANModeDefinition
{
    EXQPGameType Type;
    const TCHAR* ProtocolId;
    const TCHAR* DisplayName;
    EXQPGameMode Mode;
    const TCHAR* MapPath;
};

namespace XQPGameTypes
{
    XIANGQIPRO_API const FXQPLANModeDefinition* FindLANMode(EXQPGameType Type);
    XIANGQIPRO_API const FXQPLANModeDefinition* FindLANMode(const FString& ProtocolId);
}
