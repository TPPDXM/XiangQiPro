// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SaveGameLibrary.h"
#include "SaveGameProgress.h"

#include <Kismet/GameplayStatics.h>
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/Util/EndingLibrary.h"

USaveGameProgress* USaveGameLibrary::GetSaveGameProgress(const FString& SlotName, int32 UserIndex)
{
    auto* Save = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)
        ? Cast<USaveGameProgress>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)) : nullptr;
    const bool Loaded = Save != nullptr;
    if (!Save) Save = Cast<USaveGameProgress>(UGameplayStatics::CreateSaveGameObject(USaveGameProgress::StaticClass()));
    if (!Save || SlotName != TEXT("GameProgress")) return Save;
    const TArray<FName> Order = UEndingLibrary::GetEndingLevelOrder();
    if (Order.IsEmpty() || Save->EndingLevelOrder == Order) return Save;
    if (!Loaded)
    {
        Save->EndingLevelOrder = Order;
        return Save;
    }
    TArray<FName> PreviousOrder = Save->EndingLevelOrder;
    // Legacy tables used L1, L2, ... in insertion order, with no saved row IDs.
    if (PreviousOrder.IsEmpty())
        for (int32 Index = 0; Index < FMath::Max(Order.Num(), Save->UsingSteps.Num()); ++Index)
            PreviousOrder.Add(FName(*FString::Printf(TEXT("L%d"), Index + 1)));
    TMap<FName, int32> NewIndices;
    for (int32 Index = 0; Index < Order.Num(); ++Index) NewIndices.Add(Order[Index], Index);
    const int32 OldCurrent = FMath::Clamp(Save->EndingGameLevel, 0, PreviousOrder.Num() - 1);
    const int32 OldMax = FMath::Clamp(Save->EndingGameLevel_Max, 0, PreviousOrder.Num() - 1);
    Save->EndingGameLevel = NewIndices.FindRef(PreviousOrder[OldCurrent]);
    int32 NewMax = Save->EndingGameLevel;
    TArray<int32> Steps;
    Steps.SetNumZeroed(Order.Num());
    for (int32 Index = 0; Index < PreviousOrder.Num(); ++Index)
        if (const int32* NewIndex = NewIndices.Find(PreviousOrder[Index]))
        {
            if (Index <= OldMax) NewMax = FMath::Max(NewMax, *NewIndex);
            if (Save->UsingSteps.IsValidIndex(Index)) Steps[*NewIndex] = FMath::Max(0, Save->UsingSteps[Index]);
        }
    Save->EndingGameLevel_Max = NewMax;
    Save->UsingSteps = MoveTemp(Steps);
    Save->EndingLevelOrder = Order;
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex))
        ULogger::LogError(TEXT("Failed to persist reordered ending progress"));
    return Save;
}

bool USaveGameLibrary::ApplySaveGameProgress(USaveGameProgress* SaveGameInstance, const FString& SlotName, int32 UserIndex)
{
    if (SaveGameInstance)
    {
        if (SlotName == TEXT("GameProgress") && SaveGameInstance->EndingLevelOrder.IsEmpty())
            SaveGameInstance->EndingLevelOrder = UEndingLibrary::GetEndingLevelOrder();
        return UGameplayStatics::SaveGameToSlot(SaveGameInstance, SlotName, UserIndex);
    }
    return false;
}

int32 USaveGameLibrary::GetEndingGameLevel()
{
    USaveGameProgress* SaveGameInstance = GetSaveGameProgress(TEXT("GameProgress"), 0);
    if (SaveGameInstance)
    {
        return SaveGameInstance->EndingGameLevel;
    }
    else
    {
        ULogger::LogError(TEXT("USaveGameLibrary::GetEndingGameLevel"), TEXT("SaveGameInstance is nullptr!"));
        return 0;
    }
}

int32 USaveGameLibrary::GetEndingGameLevel_Max()
{
    USaveGameProgress* SaveGameInstance = GetSaveGameProgress(TEXT("GameProgress"), 0);
    if (SaveGameInstance)
    {
        return SaveGameInstance->EndingGameLevel_Max;
    }
    else
    {
        ULogger::LogError(TEXT("USaveGameLibrary::GetEndingGameLevel_Max"), TEXT("SaveGameInstance is nullptr!"));
        return 0;
    }
}

void USaveGameLibrary::SetEndingGameLevel(int32 EndingGameLevel)
{
    USaveGameProgress* SaveGameInstance = GetSaveGameProgress(TEXT("GameProgress"), 0);
    if (SaveGameInstance)
    {
        SaveGameInstance->EndingGameLevel = EndingGameLevel;
        ApplySaveGameProgress(SaveGameInstance, TEXT("GameProgress"), 0);
    }
    else
    {
        ULogger::LogError(TEXT("USaveGameLibrary::SetEndingGameLevel"), TEXT("SaveGameInstance is nullptr!"));
    }
}

void USaveGameLibrary::SetEndingGameLevel_Max(int32 EndingGameLevel_Max)
{
    USaveGameProgress* SaveGameInstance = GetSaveGameProgress(TEXT("GameProgress"), 0);
    if (SaveGameInstance)
    {
        SaveGameInstance->EndingGameLevel_Max = EndingGameLevel_Max;
        ApplySaveGameProgress(SaveGameInstance, TEXT("GameProgress"), 0);
    }
    else
    {
        ULogger::LogError(TEXT("USaveGameLibrary::SetEndingGameLevel"), TEXT("SaveGameInstance is nullptr!"));
    }
}

void USaveGameLibrary::UpdateEndingGameLevelData()
{
    if (auto* Save = GetSaveGameProgress(TEXT("GameProgress"), 0))
    {
        const int32 Last = FMath::Max(0, UEndingLibrary::GetEndingGameNum() - 1);
        Save->EndingGameLevel = FMath::Clamp(Save->EndingGameLevel + 1, 0, Last);
        Save->EndingGameLevel_Max = FMath::Clamp(FMath::Max(Save->EndingGameLevel_Max, Save->EndingGameLevel), 0, Last);
        ApplySaveGameProgress(Save, TEXT("GameProgress"), 0);
    }
}

int32 USaveGameLibrary::GetEndingBestSteps(int32 LevelIndex)
{
    const auto* Save = GetSaveGameProgress(TEXT("GameProgress"), 0);
    return Save && Save->UsingSteps.IsValidIndex(LevelIndex) ? FMath::Max(0, Save->UsingSteps[LevelIndex]) : 0;
}

bool USaveGameLibrary::SaveEndingBestSteps(int32 LevelIndex, int32 Steps)
{
    if (LevelIndex < 0 || LevelIndex >= UEndingLibrary::GetEndingGameNum() || Steps <= 0) return false;
    auto* Save = GetSaveGameProgress(TEXT("GameProgress"), 0);
    if (!Save) return false;
    if (Save->UsingSteps.Num() <= LevelIndex) Save->UsingSteps.SetNumZeroed(LevelIndex + 1);
    int32& Best = Save->UsingSteps[LevelIndex];
    if (Best > 0 && Steps >= Best) return false;
    Best = Steps;
    return ApplySaveGameProgress(Save, TEXT("GameProgress"), 0);
}
