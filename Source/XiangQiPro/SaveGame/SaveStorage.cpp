#include "SaveStorage.h"
#include "Misc/Crc.h"
#include "SavePlayerProfile.h"
#include "SaveGameProgress.h"
#include "ReenactmentLibrary.h"

namespace
{
    constexpr uint32 SaveMagic = 0x53505158; // XQPS
    constexpr uint32 SaveFormat = 1;
    constexpr int32 SaveEnvelopeHeaderBytes = sizeof(uint32) * 4;
    constexpr int32 MaxSaveBytes = 16 * 1024 * 1024;
}

USaveGame* XQPSaveStorage::LoadSlot(const FString& SlotName, int32 UserIndex, bool bAllowLegacy)
{
    TArray<uint8> Bytes;
    if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, UserIndex) || Bytes.Num() < SaveEnvelopeHeaderBytes || Bytes.Num() > MaxSaveBytes)
        return nullptr;
    uint32 Header[4];
    FMemory::Memcpy(Header, Bytes.GetData(), SaveEnvelopeHeaderBytes);
    if (Header[0] == SaveMagic)
    {
        if (Header[1] != SaveFormat || Header[2] != static_cast<uint32>(Bytes.Num() - SaveEnvelopeHeaderBytes) ||
            FCrc::MemCrc32(Bytes.GetData() + SaveEnvelopeHeaderBytes, Header[2]) != Header[3]) return nullptr;
        Bytes.RemoveAt(0, SaveEnvelopeHeaderBytes, EAllowShrinking::No);
    }
    else
    {
        // UE历史存档没有校验包头；只从明确的旧槽迁移，拒绝非GVAS或明显截断的数据。
        constexpr uint32 LegacyMagic = 0x53415647;
        if (!bAllowLegacy || Header[0] != LegacyMagic || Bytes.Num() < 128) return nullptr;
    }
    return UGameplayStatics::LoadGameFromMemory(Bytes);
}

FString FXQPSaveKey::SlotName(int32 Index) const
{
    const TCHAR* TypeId = nullptr;
    switch (DataType)
    {
    case EXQPSaveDataType::PlayerProfile: TypeId = TEXT("Profile"); break;
    case EXQPSaveDataType::Progress: TypeId = TEXT("Progress"); break;
    case EXQPSaveDataType::Arrangements: TypeId = TEXT("Arrangements"); break;
    default: return FString();
    }
    // 枚举已有数值保持不变；槽名不依赖枚举显示文本或蓝图资源名。
    return FString::Printf(TEXT("XQP_%s_Mode%d_%c"), TypeId, static_cast<uint8>(GameMode), Index == 0 ? TEXT('A') : TEXT('B'));
}

bool XQPSaveStorage::Save(UXQPSaveData* Data, const FXQPSaveKey& Key)
{
    if (!Data || Data->Generation < 0 || Data->Generation == MAX_int64) return false;
    switch (Key.DataType)
    {
    case EXQPSaveDataType::PlayerProfile: if (!Data->IsA<USavePlayerProfile>()) return false; break;
    case EXQPSaveDataType::Progress: if (!Data->IsA<USaveGameProgress>()) return false; break;
    case EXQPSaveDataType::Arrangements: if (!Data->IsA<USaveReenactments>()) return false; break;
    default: return false;
    }
    const int64 Previous = Data->Generation;
    ++Data->Generation;
    const FString Slot = Key.SlotName(Data->Generation % 2);
    TArray<uint8> Payload;
    if (!Slot.IsEmpty() && UGameplayStatics::SaveGameToMemory(Data, Payload) && Payload.Num() <= MaxSaveBytes - SaveEnvelopeHeaderBytes)
    {
        const uint32 Header[] = {SaveMagic, SaveFormat, static_cast<uint32>(Payload.Num()), FCrc::MemCrc32(Payload.GetData(), Payload.Num())};
        TArray<uint8> Envelope;
        Envelope.Append(reinterpret_cast<const uint8*>(Header), SaveEnvelopeHeaderBytes);
        Envelope.Append(Payload);
        if (UGameplayStatics::SaveDataToSlot(Envelope, Slot, Key.UserIndex)) return true;
    }
    Data->Generation = Previous; // 失败不能推进代数，重试仍然写同一槽。
    return false;
}
