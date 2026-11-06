#pragma once

#include "CoreMinimal.h"
#include "SaveStorage.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ReenactmentLibrary.generated.h"

/** 可分享的开局数据。保存的是布阵，而非进行中的走子或场景对象。 */
USTRUCT(BlueprintType)
struct FXQPArrangement
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FString Id;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString Name;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString Author;
    UPROPERTY(SaveGame) TArray<uint8> Cells;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bAI = true;
    UPROPERTY(SaveGame, BlueprintReadOnly) uint8 Difficulty = 1;
    FChessPosition Position() const;
    void SetPosition(const FChessPosition& Board);
    /** 草稿允许缺将，但所有来源都必须满足90格、身份和每方库存上限。 */
    bool Validate(FString& Error, bool bForPlay = false) const;
};

UCLASS()
class XIANGQIPRO_API USaveReenactments : public UXQPSaveData
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) TArray<FXQPArrangement> Games;
};

/** 跨关卡保存列表、编辑草稿与开局快照；实际下棋从不改写开局。 */
UCLASS()
class XIANGQIPRO_API UReenactmentLibrary : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    const TArray<FXQPArrangement>& GetGames() const { return Data->Games; }
    const FXQPArrangement& GetDraft() const { return Draft; }
    FXQPArrangement& EditDraft() { bDirty = true; return Draft; }
    bool IsEditing() const { return bEditing; }
    bool IsDirty() const { return bDirty || Draft.Id.IsEmpty(); }
    void NewGame();
    bool SelectGame(const FString& Id, bool bEdit, FString& Error);
    bool SaveDraft(const FString& Name, const FString& Author, FString& Error);
    /** 按稳定ID删除并立即持久化；写入失败时恢复内存列表，不能误报删除成功。 */
    bool RemoveGame(const FString& Id, FString& Error);
    bool BeginBattle(FString& Error);
    FString ExportDraft(FString& Error) const;
    bool ImportGame(const FString& Json, FString& Error);
    static int32 PieceLimit(EChessType Type);
    /** 经典棋子可达的静态区域；不要求局面完整，也不进行轮流走子的路径判断。 */
    static bool IsPiecePlacementAllowed(EChessType Type, EChessColor Color, FIntPoint Point);
private:
    UPROPERTY(Transient) TObjectPtr<USaveReenactments> Data;
    UPROPERTY(Transient) FXQPArrangement Draft;
    bool bEditing = true;
    bool bDirty = true;
};
