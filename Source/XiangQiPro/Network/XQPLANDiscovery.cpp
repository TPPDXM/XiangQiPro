#include "XQPLANDiscovery.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <winsock2.h>
#include <iphlpapi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

TArray<uint32> XQPLANDiscovery::GetTargets()
{
    // 保留原有全局广播和本机多进程发现。附加目标只查询系统已连接的网络，
    // 不扫描整个子网，也不假设热点一定使用 192.168.137.1 或固定 /24 掩码。
    TArray<uint32> Targets{0xffffffffu, 0x7f000001u};
#if PLATFORM_WINDOWS
    ULONG Size = 16384;
    TArray<uint8> Storage;
    ULONG Result = ERROR_BUFFER_OVERFLOW;
    // 网卡列表可能在查询期间变化，遇到缓冲区不足时使用系统返回的新大小重试。
    for (int32 Attempt = 0; Attempt < 3 && Result == ERROR_BUFFER_OVERFLOW; ++Attempt)
    {
        Storage.SetNumUninitialized(Size);
        Result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_DNS_SERVER |
            GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, nullptr,
            reinterpret_cast<IP_ADAPTER_ADDRESSES*>(Storage.GetData()), &Size);
    }
    if (Result == NO_ERROR)
        for (auto* Adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(Storage.GetData()); Adapter; Adapter = Adapter->Next)
        {
            // 不按 DNS_ELIGIBLE 过滤：热点虚拟网卡常常没有 DNS 注册资格。
            if (Adapter->OperStatus != IfOperStatusUp || Adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (auto* Address = Adapter->FirstUnicastAddress; Address; Address = Address->Next)
            {
                if (!Address->Address.lpSockaddr || Address->Address.lpSockaddr->sa_family != AF_INET ||
                    Address->OnLinkPrefixLength == 0 || Address->OnLinkPrefixLength > 30) continue;
                const uint32 IP = ntohl(reinterpret_cast<const sockaddr_in*>(Address->Address.lpSockaddr)->sin_addr.s_addr);
                const uint32 Mask = 0xffffffffu << (32 - Address->OnLinkPrefixLength);
                Targets.AddUnique(IP | ~Mask);
            }
            // 热点主机就是客户端的网关。单播可以绕过热点对广播的丢弃；
            // 普通路由器收到探测也不会被当成房间，后续仍须通过完整协议及房间 ID 校验。
            for (auto* Gateway = Adapter->FirstGatewayAddress; Gateway; Gateway = Gateway->Next)
                if (Gateway->Address.lpSockaddr && Gateway->Address.lpSockaddr->sa_family == AF_INET)
                {
                    const uint32 IP = ntohl(reinterpret_cast<const sockaddr_in*>(Gateway->Address.lpSockaddr)->sin_addr.s_addr);
                    if (IP != 0) Targets.AddUnique(IP);
                }
        }
#elif PLATFORM_ANDROID
    // Java 从所有 Wi-Fi 网络的 LinkProperties 获取网关，不能只看默认网络：
    // 无互联网的热点可能不是默认网络，默认网络此时仍然指向蜂窝数据。
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
            "AndroidThunkJava_XQPDiscoveryTargets", "()[Ljava/lang/String;", true);
        if (Method)
        {
            auto Values = static_cast<jobjectArray>(Env->CallObjectMethod(FJavaWrapper::GameActivityThis, Method));
            if (Env->ExceptionCheck())
            {
                Env->ExceptionClear();
                if (Values) Env->DeleteLocalRef(Values);
                return Targets;
            }
            if (Values)
            {
                for (jsize Index = 0; Index < Env->GetArrayLength(Values); ++Index)
                {
                    auto Value = static_cast<jstring>(Env->GetObjectArrayElement(Values, Index));
                    if (!Value) continue;
                    const char* Text = Env->GetStringUTFChars(Value, nullptr);
                    if (Text)
                    {
                        auto Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
                        bool bValid = false;
                        Address->SetIp(UTF8_TO_TCHAR(Text), bValid);
                        uint32 IP = 0;
                        if (bValid) Address->GetIp(IP);
                        if (IP != 0) Targets.AddUnique(IP);
                        Env->ReleaseStringUTFChars(Value, Text);
                    }
                    Env->DeleteLocalRef(Value);
                }
                Env->DeleteLocalRef(Values);
            }
        }
        // 可选 Java 方法缺失时回退原广播，不让 JNI 异常污染后续引擎调用。
        if (Env->ExceptionCheck()) Env->ExceptionClear();
    }
#endif
    return Targets;
}
