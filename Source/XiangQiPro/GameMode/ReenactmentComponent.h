#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "ReenactmentComponent.generated.h"

class AChesses;
class UReenactmentLibrary;

/** 编辑态的输入、库存及场景表现。对弈态完全交回原有 GameState。 */
UCLASS()
class XIANGQIPRO_API UReenactmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UReenactmentComponent();
    bool IsEditing() const;
    void InitializeEditor();
    void SelectPiece(EChessType Type);
    void ToggleColor();
    void ClearBoard(bool bStandard = false);
    void RemoveSelected();
    void HandlePointer(FVector2D Screen, bool bPressed, bool bReleased, bool bMoved);
    void HidePreview();
    int32 Remaining(EChessType Type) const;
    /** 提示与输入提交共用判断，防止直接点击或滑动绕过落点限制。 */
    bool CanPlaceAt(FIntPoint Point) const;
    EChessType SelectedType = EChessType::EMPTY;
    EChessColor Color = EChessColor::REDCHESS;
    TWeakObjectPtr<AChesses> SelectedChess;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<AChesses> Preview;
    FIntPoint LastPlaced = FIntPoint(-1, -1);
    FVector2D PointerStart;
    bool bPointerDown = false;
    bool bDragging = false;
    bool bBeganOnPiece = false;
    FIntPoint FindPoint(FVector2D Screen, FVector& WorldPoint) const;
    AChesses* SpawnPiece(EChessType Type, EChessColor Side, FIntPoint Point, bool bGhost);
    void RefreshPoints();
    void StoreBoard();
};
