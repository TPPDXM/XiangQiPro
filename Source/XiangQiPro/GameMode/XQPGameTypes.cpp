#include "XQPGameTypes.h"
#include "XiangQiPro/Util/AssetPaths.h"

namespace
{
    // 新联机玩法在这里登记自己的协议标识、运行模式和地图；未知类型不能落入已有玩法。
    const FXQPLANModeDefinition LANModes[] = {
        {EXQPGameType::Classic, TEXT("classic"), TEXT("经典象棋"), EXQPGameMode::LAN, PATH_MAP_BATTLE2P},
        {EXQPGameType::FengHuo, TEXT("feng_huo"), TEXT("烽火弈"), EXQPGameMode::FengHuoLAN, PATH_MAP_BATTLE2P}
    };
}

const FXQPLANModeDefinition* XQPGameTypes::FindLANMode(EXQPGameType Type)
{
    for (const auto& Mode : LANModes)
        if (Mode.Type == Type) return &Mode;
    return nullptr;
}

const FXQPLANModeDefinition* XQPGameTypes::FindLANMode(const FString& ProtocolId)
{
    for (const auto& Mode : LANModes)
        if (ProtocolId == Mode.ProtocolId) return &Mode;
    return nullptr;
}
