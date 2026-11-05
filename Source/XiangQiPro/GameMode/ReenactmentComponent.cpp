#include "ReenactmentComponent.h"
#include "XQPGameStateBase.h"
#include "XQPGameInstance.h"
#include "XiangQiPro/SaveGame/ReenactmentLibrary.h"
#include "XiangQiPro/Chess/AllChessHeader.h"
#include "XiangQiPro/GameObject/ChessBoard2PActor.h"
#include "XiangQiPro/GameObject/SettingPoint.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineUtils.h"

UReenactmentComponent::UReenactmentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UReenactmentComponent::IsEditing() const
{
    const auto* GI = GetWorld() ? GetWorld()->GetGameInstance<UXQPGameInstance>() : nullptr;
    return GI && GI->GetGameMode() == EXQPGameMode::Reenactment && GI->GetSubsystem<UReenactmentLibrary>()->IsEditing();
}

void UReenactmentComponent::InitializeEditor()
{
    HidePreview();
    SelectedChess.Reset();
    SelectedType = EChessType::EMPTY;
    bPointerDown = false;
    SetComponentTickEnabled(IsEditing());
    if (IsEditing()) RefreshPoints();
}

int32 UReenactmentComponent::Remaining(EChessType Type) const
{
    const auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !State->board2P) return 0;
    int32 Count = 0;
    const auto Board = State->board2P->CreateSnapshot();
    for (const auto& Piece : Board.Squares) if (Piece.Type == Type && Piece.Color == Color) ++Count;
    return FMath::Max(0, UReenactmentLibrary::PieceLimit(Type) - Count);
}

void UReenactmentComponent::SelectPiece(EChessType Type)
{
    if (!IsEditing()) return;
    SelectedType = Type == SelectedType ? EChessType::EMPTY : Type;  // 若点击了相同的则取消棋子选择
    bBeganOnPiece = false;
    if (SelectedChess.IsValid()) SelectedChess->ChessMesh->SetOverlayMaterial(nullptr);
    SelectedChess.Reset();
    HidePreview();
    RefreshPoints();
}

void UReenactmentComponent::ToggleColor()
{
    Color = FChessPosition::Opponent(Color);
    SelectPiece(SelectedType);
}

void UReenactmentComponent::StoreBoard()
{
    if (const auto* State = Cast<AXQPGameStateBase>(GetOwner()); State && State->board2P)
        GetWorld()->GetGameInstance()->GetSubsystem<UReenactmentLibrary>()->EditDraft().SetPosition(State->board2P->CreateSnapshot());
}

void UReenactmentComponent::RefreshPoints()
{
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !State->board2P || !IsEditing()) return;
    // 初始无选中棋子时没有提示；规则区域、占位与库存统一由CanPlaceAt判断。
    for (int32 X = 0; X < 10; ++X)
        for (int32 Y = 0; Y < 9; ++Y)
            if (auto Point = State->board2P->SettingPoints[X][Y]; Point.IsValid())
                Point->SetActivate(CanPlaceAt(FIntPoint(X, Y)));
}

bool UReenactmentComponent::CanPlaceAt(FIntPoint Point) const
{
    const auto* State = Cast<AXQPGameStateBase>(GetOwner());
    return State && State->board2P && UReenactmentLibrary::IsPiecePlacementAllowed(SelectedType, Color, Point) &&
        !State->board2P->GetChess(Point).IsValid() && Remaining(SelectedType) > 0;
}

AChesses* UReenactmentComponent::SpawnPiece(EChessType Type, EChessColor Side, FIntPoint Point, bool bGhost)
{
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (!State || !State->board2P || !State->GetChessBoard2PActor().IsValid()) return nullptr;
    TSubclassOf<AChesses> Classes[] = {nullptr, AChess_Jiang::StaticClass(), AChess_Shi::StaticClass(), AChess_Xiang::StaticClass(),
        AChess_Ma::StaticClass(), AChess_Jv::StaticClass(), AChess_Pao::StaticClass(), AChess_Bing::StaticClass()};
    const int32 Index = static_cast<int32>(Type);
    if (Index < 1 || Index > 7) return nullptr;
    auto* Chess = State->GetChessBoard2PActor()->SpawnChessAt(Classes[Index], Point);
    if (!Chess) return nullptr;
    Chess->Init(Side, Point, State->board2P);
    Chess->FinishSpawning(FTransform(State->board2P->BoardLocs[Point.X][Point.Y]));
    Chess->SetActorEnableCollision(false); // 编辑输入统一投射到棋盘平面，不经过棋子上的旧事件。
    if (bGhost)
    {
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, PATH_M_REENACTMENT_GHOST))
        {
            auto* Dynamic = UMaterialInstanceDynamic::Create(Material, Chess);
            Dynamic->SetVectorParameterValue(TEXT("Color"), Side == EChessColor::REDCHESS ? FLinearColor(.7f,.08f,.04f) : FLinearColor(.08f,.12f,.16f));
            Chess->ChessMesh->SetMaterial(0, Dynamic);
        }
        Chess->ChessMask->SetFadeScreenSize(0.f);
    }
    else
    {
        State->board2P->SetChess(Point.X, Point.Y, Chess);
        Chess->PlayArrangementSpawn();
    }
    return Chess;
}

FIntPoint UReenactmentComponent::FindPoint(FVector2D Screen, FVector& WorldPoint) const
{
    const auto* State = Cast<AXQPGameStateBase>(GetOwner());
    const auto* Board = State ? State->board2P.Get() : nullptr;
    auto* PC = GetWorld()->GetFirstPlayerController();
    FVector Origin, Direction;
    if (!Board || Board->BoardLocs.Num() != 10 || !PC || !PC->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
        return FIntPoint(-1, -1);
    const FVector Base = Board->BoardLocs[0][0];
    const double Distance = (Base.Z - Origin.Z) / Direction.Z;
    if (Distance < 0) return FIntPoint(-1, -1);
    WorldPoint = Origin + Direction * Distance;
    const FVector Row = Board->BoardLocs[1][0] - Base, Col = Board->BoardLocs[0][1] - Base;
    const double X = FVector::DotProduct(WorldPoint - Base, Row) / Row.SizeSquared();
    const double Y = FVector::DotProduct(WorldPoint - Base, Col) / Col.SizeSquared();
    const FIntPoint P(FMath::RoundToInt(X), FMath::RoundToInt(Y));
    // 最近格吸附不超过半格，四周保留边界空隙，PC与移动端遵循相同容错。
    return FChessPosition::IsInside(P) && FMath::Abs(X-P.X) <= .48 && FMath::Abs(Y-P.Y) <= .48 ? P : FIntPoint(-1,-1);
}

void UReenactmentComponent::HandlePointer(FVector2D Screen, bool bPressed, bool bReleased, bool bMoved)
{
    if (!IsEditing()) return;
    auto* Manager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManager>();
    if (GetWorld()->IsPaused() || Manager->HasOverlayUI()) { HidePreview(); bPointerDown = false; return; }
    FVector WorldPoint;
    const FIntPoint P = FindPoint(Screen, WorldPoint);
    if (bPressed)
    {
        bPointerDown = true;
        PointerStart = Screen;
        bDragging = bBeganOnPiece = false;
        LastPlaced = FIntPoint(-1,-1);
    }
    if (!FChessPosition::IsInside(P)) { if (Preview.IsValid()) Preview->SetActorHiddenInGame(true); if (bReleased) bPointerDown = false; return; }
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    if (bPressed && State->board2P->GetChess(P).IsValid())
    {
        if (SelectedChess.IsValid()) SelectedChess->ChessMesh->SetOverlayMaterial(nullptr);
        SelectedChess = State->board2P->GetChess(P);
        SelectedChess->ChessMesh->SetOverlayMaterial(SelectedChess->MI_Stroke);
        bBeganOnPiece = true;
    }
    if (bPointerDown && bMoved && FVector2D::Distance(Screen, PointerStart) > 8.f * UWidgetLayoutLibrary::GetViewportScale(this)) bDragging = true;
    const bool ShowGhost = SelectedType != EChessType::EMPTY && (!XQPMobile::IsPortrait(this) || bPointerDown) && !bBeganOnPiece && !SelectedChess.IsValid() && Remaining(SelectedType) > 0;
    if (ShowGhost && !Preview.IsValid()) Preview = SpawnPiece(SelectedType, Color, P, true);
    if (Preview.IsValid())
    {
        Preview->SetActorHiddenInGame(!ShowGhost);
        Preview->SetActorLocation(WorldPoint + FVector(0,0,2.f));
    }
    // 触摸拖动只移动预览，抬手才提交一枚棋子；PC保留按住滑动连续布阵。
    const bool bCommitPlacement = bReleased || (!XQPMobile::IsPortrait(this) && bDragging);
    if (bPointerDown && !bBeganOnPiece && bCommitPlacement && LastPlaced != P && CanPlaceAt(P))
    {
        if (SpawnPiece(SelectedType, Color, P, false))
        {
            LastPlaced = P;
            StoreBoard();
            RefreshPoints();
        }
    }
    if (bReleased) { bPointerDown = false; if (XQPMobile::IsPortrait(this)) HidePreview(); }
}

void UReenactmentComponent::HidePreview()
{
    if (Preview.IsValid()) Preview->Destroy();
    Preview.Reset();
}

void UReenactmentComponent::RemoveSelected()
{
    if (!IsEditing() || !SelectedChess.IsValid()) return;
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    const auto P = SelectedChess->GetPosition();
    State->board2P->SetChess(P.X, P.Y, nullptr);
    SelectedChess->Defeated(); // 复用正式消散和粒子，完成后由棋子自身释放。
    SelectedChess.Reset();
    StoreBoard();
    RefreshPoints();
}

void UReenactmentComponent::ClearBoard(bool bStandard)
{
    if (!IsEditing()) return;
    HidePreview(); SelectedChess.Reset();
    // 撤阵后的消散Actor已不在逻辑棋盘中，清空/默认局面也要取消它们，避免残影覆盖新布阵。
    for (TActorIterator<AChesses> It(GetWorld()); It; ++It) It->Destroy();
    auto* State = Cast<AXQPGameStateBase>(GetOwner());
    const auto Actor = State->GetChessBoard2PActor();
    State->board2P->InitializeBoard(Actor);
    Actor->Init(State->board2P);
    Actor->GenerateFromPosition(bStandard ? FChessPosition::Standard() : FChessPosition{});
    for (const auto& Row : State->board2P->AllChess) for (const auto& Chess : Row) if (Chess.IsValid()) Chess->SetActorEnableCollision(false);
    StoreBoard(); RefreshPoints();
}

void UReenactmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!IsEditing()) { HidePreview(); SetComponentTickEnabled(false); return; }
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    if (XQPMobile::IsPortrait(this))
    {
        float X,Y; bool Pressed;
        PC->GetInputTouchState(ETouchIndex::Touch1,X,Y,Pressed);
        if (!Pressed || GetWorld()->GetGameInstance()->GetSubsystem<UUIManager>()->HasOverlayUI()) { HidePreview(); bPointerDown = false; }
    }
    else
    {
        float X,Y;
        if (PC->GetMousePosition(X,Y)) HandlePointer(FVector2D(X,Y),false,false,bPointerDown);
    }
}

void UReenactmentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    HidePreview();
    Super::EndPlay(Reason);
}
