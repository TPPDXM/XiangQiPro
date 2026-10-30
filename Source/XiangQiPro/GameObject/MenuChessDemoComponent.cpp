// Copyright 2026 Ultimate Player All Rights Reserved.
#include "MenuChessDemoComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float MoveSeconds = .65f;
    EChessType Type(char Letter)
    {
        switch (Letter)
        {
        case 'K': return EChessType::JIANG; case 'A': return EChessType::SHI;
        case 'E': return EChessType::XIANG; case 'H': return EChessType::MA;
        case 'R': return EChessType::JV; case 'C': return EChessType::PAO;
        case 'P': return EChessType::BING; default: return EChessType::EMPTY;
        }
    }
}

FMenuChessGame UMenuChessDemoComponent::MakeGame(int32 Index)
{
    FMenuChessGame Result;
    auto Put = [&](int32 Row, int32 Column, EChessColor Color, char Letter)
    { Result.Position.Set({Row, Column}, {Type(Letter), Color}); };
    const EChessColor Red = EChessColor::REDCHESS, Black = EChessColor::BLACKCHESS;
    if (Index == 0)
    {
        Result.Title = FText::FromString(TEXT("开局演弈"));
        for (EChessColor Color : {Red, Black})
        {
            const int32 Row = Color == Red ? 0 : 9;
            const char* Back = "RHEAKAEHR";
            for (int32 C = 0; C < 9; ++C) Put(Row, C, Color, Back[C]);
            for (int32 C : {1, 7}) Put(Color == Red ? 2 : 7, C, Color, 'C');
            for (int32 C : {0, 2, 4, 6, 8}) Put(Color == Red ? 3 : 6, C, Color, 'P');
        }
        Result.Moves = {{{2,1},{9,1}},{{6,2},{5,2}},{{2,7},{9,7}},{{6,4},{5,4}},{{9,7},{9,5}},{{5,4},{4,4}},
            {{9,5},{9,8}},{{9,6},{7,4}},{{9,8},{9,3}},{{4,4},{3,4}},{{9,3},{8,3}},{{9,2},{7,0}},
            {{8,3},{9,3}},{{9,0},{9,1}},{{0,0},{2,0}},{{6,6},{5,6}},{{2,0},{2,4}},{{5,6},{4,6}},
            {{2,4},{3,4}},{{4,6},{3,6}},{{3,4},{7,4}},{{9,4},{9,5}},{{7,4},{7,5}}};
    }
    else
    {
        Put(0,4,Red,'K'); Put(9,4,Black,'K'); Put(5,4,Red,'P');
        if (Index == 1)
        {
            Result.Title = FText::FromString(TEXT("残局 · 双车错"));
            Put(6,0,Red,'R'); Put(5,8,Red,'R'); Put(8,8,Black,'R'); Put(7,0,Black,'P');
            Result.Moves = {{{6,0},{6,4}},{{9,4},{9,5}},{{6,4},{9,4}},{{9,5},{8,5}},{{5,8},{5,5}}};
        }
        else if (Index == 2)
        {
            Result.Title = FText::FromString(TEXT("残局 · 马炮合击"));
            Put(6,2,Red,'H'); Put(6,6,Red,'C'); Put(8,3,Black,'A'); Put(9,5,Black,'A');
            Result.Moves = {{{6,6},{9,6}},{{9,4},{8,4}},{{6,2},{8,3}},{{8,4},{7,4}},{{8,3},{9,5}},{{7,4},{7,3}},{{9,6},{7,6}}};
        }
        else
        {
            Result.Title = FText::FromString(TEXT("残局 · 马卒巧胜"));
            Put(6,1,Red,'R'); Put(5,6,Red,'H'); Put(9,5,Black,'A'); Put(7,6,Black,'E');
            Result.Moves = {{{6,1},{9,1}},{{9,4},{8,4}},{{9,1},{9,4}},{{8,4},{8,3}},{{5,6},{7,5}},
                {{8,3},{7,3}},{{9,4},{7,4}},{{7,3},{7,4}},{{5,4},{6,4}},{{7,4},{7,3}},{{6,4},{6,3}}};
            // Reverse the third ending so the black side also wins a demonstration.
            FChessPosition Mirrored;
            for (int32 I = 0; I < 90; ++I)
                if (Result.Position.Squares[I].Type != EChessType::EMPTY)
                {
                    auto Piece = Result.Position.Squares[I];
                    Piece.Color = FChessPosition::Opponent(Piece.Color);
                    Mirrored.Set({9 - I / 9, 8 - I % 9}, Piece);
                }
            Result.Position = Mirrored;
            for (auto& Move : Result.Moves)
            {
                Move.from = {9 - Move.from.X, 8 - Move.from.Y};
                Move.to = {9 - Move.to.X, 8 - Move.to.Y};
            }
            Result.FirstSide = Black;
        }
    }
    return Result;
}

UMenuChessDemoComponent::UMenuChessDemoComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Mesh/Chess/SM_Chess"));
    PieceMesh = Mesh.Object;
}

void UMenuChessDemoComponent::BeginPlay()
{
    Super::BeginPlay();
    ResetBoard();
}

FVector UMenuChessDemoComponent::SquareLocation(FIntPoint Square) const
{
    return GetOwner()->GetActorLocation() + FVector(-38.f + Square.X * (76.f / 9.f), -30.f + Square.Y * 7.5f, 2.8f);
}

void UMenuChessDemoComponent::ClearPieces()
{
    for (const auto& Label : Labels) if (Label) Label->DestroyComponent();
    for (const auto& Piece : Pieces) if (Piece) Piece->DestroyComponent();
    Labels.Empty(); Pieces.Empty();
}

void UMenuChessDemoComponent::ResetBoard()
{
    ClearPieces();
    Game = MakeGame(Episode);
    Position = Game.Position;
    Side = Game.FirstSide;
    Ply = 0; Elapsed = 0; Wait = 2.5f; bMoving = bFinished = false;
    UpdateCaption();
    Pieces.SetNumZeroed(90); Labels.SetNumZeroed(90);
    const TCHAR* RedNames[] = {TEXT(""),TEXT("shuai"),TEXT("shi"),TEXT("xiang"),TEXT("ma"),TEXT("jv"),TEXT("pao"),TEXT("bing")};
    const TCHAR* BlackNames[] = {TEXT(""),TEXT("jiang"),TEXT("shi"),TEXT("xiang"),TEXT("ma"),TEXT("jv"),TEXT("pao"),TEXT("zu")};
    for (int32 I = 0; I < 90; ++I)
    {
        const auto Piece = Position.Squares[I];
        if (Piece.Type == EChessType::EMPTY) continue;
        auto* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
        Mesh->SetStaticMesh(PieceMesh);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetupAttachment(GetOwner()->GetRootComponent());
        Mesh->RegisterComponent();
        Mesh->SetWorldLocationAndRotation(SquareLocation({I / 9, I % 9}), FRotator::ZeroRotator);
        Pieces[I] = Mesh;
        auto* Label = NewObject<UDecalComponent>(GetOwner());
        Label->SetupAttachment(Mesh);
        Label->SetRelativeLocation(FVector(0,0,1));
        Label->SetRelativeScale3D(FVector(.0025f,.008f,.008f));
        const bool IsRed = Piece.Color == EChessColor::REDCHESS;
        Label->SetRelativeRotation(FRotator(-90, IsRed ? 90 : -90, 0));
        const FString MaterialPath = FString::Printf(TEXT("/Game/Material/Decal/ChessMask/%s/MI_ChessMask_%s_%s"),
            IsRed ? TEXT("red") : TEXT("black"), IsRed ? TEXT("rad") : TEXT("black"),
            IsRed ? RedNames[static_cast<int32>(Piece.Type)] : BlackNames[static_cast<int32>(Piece.Type)]);
        Label->SetDecalMaterial(LoadObject<UMaterialInterface>(nullptr, *MaterialPath));
        Label->SetFadeScreenSize(0.f);
        Label->RegisterComponent();
        Labels[I] = Label;
    }
}

void UMenuChessDemoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bMoving)
    {
        Wait -= DeltaTime;
        if (Wait > 0) return;
        if (bFinished) { Episode = Episode % 3 + 1; ResetBoard(); return; }
        if (!Game.Moves.IsValidIndex(Ply)) return;
        Moving = Game.Moves[Ply];
        // Validate the replay against the same rules used by real matches.
        if (!Position.GenerateLegalMoves(Side).Contains(Moving))
        {
            UE_LOG(LogTemp, Error, TEXT("Menu demo %d has an illegal move at ply %d"), Episode, Ply);
            SetComponentTickEnabled(false); return;
        }
        bMoving = true; Elapsed = 0;
    }
    Elapsed += DeltaTime;
    const float Alpha = FMath::Clamp(Elapsed / MoveSeconds, 0.f, 1.f);
    const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha);
    if (UStaticMeshComponent* Mesh = Pieces[Moving.from.X * 9 + Moving.from.Y])
        Mesh->SetWorldLocation(FMath::Lerp(SquareLocation(Moving.from), SquareLocation(Moving.to), Smooth)
            + FVector(0, 0, FMath::Sin(PI * Alpha) * 3.5f));
    if (Alpha >= 1.f) FinishMove();
}

void UMenuChessDemoComponent::FinishMove()
{
    const int32 From = Moving.from.X * 9 + Moving.from.Y, To = Moving.to.X * 9 + Moving.to.Y;
    if (Labels[To]) Labels[To]->DestroyComponent();
    if (Pieces[To]) Pieces[To]->DestroyComponent();
    Pieces[To] = Pieces[From]; Pieces[From] = nullptr;
    Labels[To] = Labels[From]; Labels[From] = nullptr;
    Position.MakeMove(Moving);
    Side = FChessPosition::Opponent(Side);
    ++Ply; bMoving = false;
    bFinished = Position.GenerateLegalMoves(Side).IsEmpty();
    UpdateCaption();
    Wait = bFinished ? 3.5f : (Side == EChessColor::REDCHESS ? 1.35f : 1.65f);
}

void UMenuChessDemoComponent::UpdateCaption()
{
    const TCHAR* Status = bFinished ? (Side == EChessColor::BLACKCHESS ? TEXT("红方胜") : TEXT("黑方胜"))
        : (Side == EChessColor::REDCHESS ? TEXT("红方行棋") : TEXT("黑方行棋"));
    Caption = FText::FromString(Game.Title.ToString() + TEXT("  ·  ") + Status);
}

void UMenuChessDemoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPieces();
    Super::EndPlay(EndPlayReason);
}
