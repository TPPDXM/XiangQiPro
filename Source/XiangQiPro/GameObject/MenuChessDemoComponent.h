// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "MenuChessDemoComponent.generated.h"

struct FMenuChessGame
{
    FChessPosition Position;
    TArray<FChessMove2P> Moves;
    EChessColor FirstSide = EChessColor::REDCHESS;
    FText Title;
};

/** Small, deterministic legal replays. No gameplay Actors, input, AI workers or save data. */
UCLASS()
class XIANGQIPRO_API UMenuChessDemoComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMenuChessDemoComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    const FText& GetCaption() const { return Caption; }
    static FMenuChessGame MakeGame(int32 Index);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
    UPROPERTY() TArray<TObjectPtr<class UDecalComponent>> Labels;
    UPROPERTY() TObjectPtr<class UStaticMesh> PieceMesh;
    FMenuChessGame Game;
    FChessPosition Position;
    FChessMove2P Moving;
    int32 Episode = 0;
    int32 Ply = 0;
    float Elapsed = 0.f;
    float Wait = 2.5f;
    bool bMoving = false;
    bool bFinished = false;
    EChessColor Side = EChessColor::REDCHESS;
    FText Caption;
    void UpdateCaption();
    void ResetBoard();
    void ClearPieces();
    void FinishMove();
    FVector SquareLocation(FIntPoint Square) const;
};
