// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SaveGameLibrary.h"
#include "SaveGameProgress.h"

#include <Kismet/GameplayStatics.h>
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/Util/EndingLibrary.h"

USaveGameProgress* USaveGameLibrary::LoadProgress(EXQPGameMode Mode, int32 UserIndex)
{
    const FXQPSaveKey Key{EXQPSaveDataType::Progress, Mode, UserIndex};
    const auto Valid = [](USaveGameProgress* Save)
    {
        if (!Save || (Save->Version != 1 && Save->Version != 2) || Save->Generation < 0) return false;
        Save->MigrateLegacyData();
        const auto& P = Save->Progress;
        if (P.CurrentLevel < 0 || P.MaxUnlockedLevel < P.CurrentLevel || P.BestSteps.Num() > 100000 || P.LevelOrder.Num() > 100000) return false;
        for (int32 Steps : P.BestSteps) if (Steps < 0) return false;
        return true;
    };
    auto* Save = XQPSaveStorage::Load<USaveGameProgress>(Key, Valid);
    bool bLegacy = false;
    if (!Save && Mode == EXQPGameMode::Ending && UGameplayStatics::DoesSaveGameExist(TEXT("GameProgress"), UserIndex))
    {
        auto* Legacy = Cast<USaveGameProgress>(XQPSaveStorage::LoadSlot(TEXT("GameProgress"), UserIndex, true));
        if (Valid(Legacy)) { Save = Legacy; bLegacy = true; }
    }
    const bool Loaded = Save != nullptr;
    if (!Save)
    {
        Save = NewObject<USaveGameProgress>();
        Save->MigrateLegacyData();
    }
    if (bLegacy && !SaveProgress(Save, Mode, UserIndex))
        ULogger::LogError(TEXT("Failed to migrate legacy ending progress"));
    if (Mode != EXQPGameMode::Ending) return Save;
    const TArray<FName> Order = UEndingLibrary::GetEndingLevelOrder();
    if (Order.IsEmpty() || Save->Progress.LevelOrder == Order) return Save;
    if (!Loaded)
    {
        Save->Progress.LevelOrder = Order;
        return Save;
    }
    TArray<FName> PreviousOrder = Save->Progress.LevelOrder;
    // Legacy tables used L1, L2, ... in insertion order, with no saved row IDs.
    if (PreviousOrder.IsEmpty())
        for (int32 Index = 0; Index < FMath::Max(Order.Num(), Save->Progress.BestSteps.Num()); ++Index)
            PreviousOrder.Add(FName(*FString::Printf(TEXT("L%d"), Index + 1)));
    TMap<FName, int32> NewIndices;
    for (int32 Index = 0; Index < Order.Num(); ++Index) NewIndices.Add(Order[Index], Index);
    const int32 OldCurrent = FMath::Clamp(Save->Progress.CurrentLevel, 0, PreviousOrder.Num() - 1);
    const int32 OldMax = FMath::Clamp(Save->Progress.MaxUnlockedLevel, 0, PreviousOrder.Num() - 1);
    Save->Progress.CurrentLevel = NewIndices.FindRef(PreviousOrder[OldCurrent]);
    int32 NewMax = Save->Progress.CurrentLevel;
    TArray<int32> Steps;
    Steps.SetNumZeroed(Order.Num());
    for (int32 Index = 0; Index < PreviousOrder.Num(); ++Index)
        if (const int32* NewIndex = NewIndices.Find(PreviousOrder[Index]))
        {
            if (Index <= OldMax) NewMax = FMath::Max(NewMax, *NewIndex);
            if (Save->Progress.BestSteps.IsValidIndex(Index)) Steps[*NewIndex] = FMath::Max(0, Save->Progress.BestSteps[Index]);
        }
    Save->Progress.MaxUnlockedLevel = NewMax;
    Save->Progress.BestSteps = MoveTemp(Steps);
    Save->Progress.LevelOrder = Order;
    if (!SaveProgress(Save, Mode, UserIndex))
        ULogger::LogError(TEXT("Failed to persist reordered ending progress"));
    return Save;
}

bool USaveGameLibrary::SaveProgress(USaveGameProgress* Save, EXQPGameMode Mode, int32 UserIndex)
{
    if (!Save) return false;
    Save->MigrateLegacyData();
    if (Mode == EXQPGameMode::Ending && Save->Progress.LevelOrder.IsEmpty())
        Save->Progress.LevelOrder = UEndingLibrary::GetEndingLevelOrder();
    return XQPSaveStorage::Save(Save, {EXQPSaveDataType::Progress, Mode, UserIndex});
}

int32 USaveGameLibrary::GetEndingGameLevel()
{
    const auto* Save = LoadProgress(EXQPGameMode::Ending);
    return Save ? Save->Progress.CurrentLevel : 0;
}

int32 USaveGameLibrary::GetEndingGameLevel_Max()
{
    const auto* Save = LoadProgress(EXQPGameMode::Ending);
    return Save ? Save->Progress.MaxUnlockedLevel : 0;
}

void USaveGameLibrary::SetEndingGameLevel(int32 Level)
{
    if (auto* Save = LoadProgress(EXQPGameMode::Ending))
    {
        Save->Progress.CurrentLevel = FMath::Clamp(Level, 0, Save->Progress.MaxUnlockedLevel);
        SaveProgress(Save, EXQPGameMode::Ending);
    }
}

void USaveGameLibrary::SetEndingGameLevel_Max(int32 Level)
{
    if (auto* Save = LoadProgress(EXQPGameMode::Ending))
    {
        const int32 Last = FMath::Max(0, UEndingLibrary::GetEndingGameNum() - 1);
        Save->Progress.MaxUnlockedLevel = FMath::Clamp(Level, Save->Progress.CurrentLevel, Last);
        SaveProgress(Save, EXQPGameMode::Ending);
    }
}

void USaveGameLibrary::UpdateEndingGameLevelData()
{
    if (auto* Save = LoadProgress(EXQPGameMode::Ending))
    {
        const int32 Last = FMath::Max(0, UEndingLibrary::GetEndingGameNum() - 1);
        auto& P = Save->Progress;
        P.CurrentLevel = FMath::Clamp(P.CurrentLevel + 1, 0, Last);
        P.MaxUnlockedLevel = FMath::Clamp(FMath::Max(P.MaxUnlockedLevel, P.CurrentLevel), 0, Last);
        SaveProgress(Save, EXQPGameMode::Ending);
    }
}

int32 USaveGameLibrary::GetEndingBestSteps(int32 LevelIndex)
{
    const auto* Save = LoadProgress(EXQPGameMode::Ending);
    return Save && Save->Progress.BestSteps.IsValidIndex(LevelIndex) ? Save->Progress.BestSteps[LevelIndex] : 0;
}

bool USaveGameLibrary::SaveEndingBestSteps(int32 LevelIndex, int32 Steps)
{
    if (LevelIndex < 0 || LevelIndex >= UEndingLibrary::GetEndingGameNum() || Steps <= 0) return false;
    auto* Save = LoadProgress(EXQPGameMode::Ending);
    if (!Save) return false;
    auto& Scores = Save->Progress.BestSteps;
    if (Scores.Num() <= LevelIndex) Scores.SetNumZeroed(LevelIndex + 1);
    int32& Best = Scores[LevelIndex];
    if (Best > 0 && Steps >= Best) return false;
    Best = Steps;
    return SaveProgress(Save, EXQPGameMode::Ending);
}

bool USaveGameLibrary::CompleteEndingLevel(int32 LevelIndex, int32 Steps, int32& OutBestSteps, bool& bOutRecordBroken)
{
    OutBestSteps = 0;
    bOutRecordBroken = false;
    const int32 Count = UEndingLibrary::GetEndingGameNum();
    if (LevelIndex < 0 || LevelIndex >= Count || Steps <= 0) return false;
    auto* Save = LoadProgress(EXQPGameMode::Ending);
    if (!Save) return false;
    auto& P = Save->Progress;
    if (P.BestSteps.Num() <= LevelIndex) P.BestSteps.SetNumZeroed(LevelIndex + 1);
    const int32 PreviousBest = P.BestSteps[LevelIndex];
    const bool bImproved = PreviousBest == 0 || Steps < PreviousBest;
    if (bImproved) P.BestSteps[LevelIndex] = Steps;
    P.CurrentLevel = FMath::Min(LevelIndex + 1, Count - 1);
    P.MaxUnlockedLevel = FMath::Max(P.MaxUnlockedLevel, P.CurrentLevel);
    const bool bSaved = SaveProgress(Save, EXQPGameMode::Ending);
    OutBestSteps = bSaved ? P.BestSteps[LevelIndex] : PreviousBest;
    bOutRecordBroken = bSaved && bImproved && PreviousBest > 0;
    return bSaved;
}
