#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "ProBattleTypes.generated.h"

UENUM(BlueprintType)
enum class EProAIDifficulty : uint8
{
    Easy UMETA(DisplayName = "简单"),
    Normal UMETA(DisplayName = "普通"),
    Hard UMETA(DisplayName = "困难"),
    Expert UMETA(DisplayName = "大师")
};

USTRUCT(BlueprintType)
struct FProBattleSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxSupply = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float InitialSupply = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SupplyPerSecond = 0.8f / 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EProAIDifficulty AIDifficulty = EProAIDifficulty::Hard;
};

USTRUCT(BlueprintType)
struct FProBattleStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 CaptureScore = 0;
    UPROPERTY(BlueprintReadOnly) int32 Captures = 0;
    UPROPERTY(BlueprintReadOnly) int32 Moves = 0;
    UPROPERTY(BlueprintReadOnly) int32 SupplySpent = 0;
};

struct FProPieceRule
{
    int32 Cost;
    float Speed;
    int32 Score;
};

namespace ProBattle
{
    // 速度以每秒跨越的棋盘格数计，分值仅用于吃子结算。
    inline FProPieceRule Rule(EChessType Type)
    {
        switch (Type)
        {
        case EChessType::JIANG: return {2, 2.4f, 0};
        case EChessType::SHI:   return {1, 3.2f, 2};
        case EChessType::XIANG: return {2, 2.8f, 3};
        case EChessType::MA:    return {3, 4.8f, 5};
        case EChessType::JV:    return {5, 4.0f, 9};
        case EChessType::PAO:   return {4, 3.6f, 6};
        case EChessType::BING:  return {1, 2.6f, 1};
        default: return {0, 1.f, 0};
        }
    }
    inline float TravelSeconds(EChessType Type, FIntPoint From, FIntPoint To)
    {
        return FMath::Max(0.24f, FVector2D(To - From).Size() / Rule(Type).Speed);
    }
    inline int32 Side(EChessColor Color) { return Color == EChessColor::REDCHESS ? 0 : 1; }
}

// Id 在整局保持不变，联网快照通过该编号关联棋子。
struct FProBattleUnit
{
    EChessType Type = EChessType::EMPTY;
    EChessColor Color = EChessColor::REDCHESS;
    FIntPoint Square = FIntPoint(-1, -1);
    FIntPoint From = FIntPoint(-1, -1);
    FIntPoint To = FIntPoint(-1, -1);
    float DepartAt = 0.f;
    float ArriveAt = 0.f;
    bool bAlive = true;
    bool bMoving = false;
    int32 Support = INDEX_NONE;
};
