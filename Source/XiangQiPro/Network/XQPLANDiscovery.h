#pragma once
#include "CoreMinimal.h"

namespace XQPLANDiscovery
{
    // 返回主机字节序 IPv4 目标；每轮搜索重新读取，支持建房后开启热点及切换网络。
    TArray<uint32> GetTargets();
}
