#include "ReenactmentLibrary.h"
#include "PlayerProfileSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    const FXQPSaveKey LibraryKey{EXQPSaveDataType::Arrangements, EXQPGameMode::Reenactment};
    constexpr int32 MaxGames = 256;
    FString CleanLabel(const FString& Value, int32 MaxLength)
    {
        FString Result;
        for (TCHAR C : Value.TrimStartAndEnd()) if (!FChar::IsControl(C)) Result.AppendChar(C);
        return Result.Left(MaxLength).TrimStartAndEnd();
    }
}

int32 UReenactmentLibrary::PieceLimit(EChessType Type)
{
    static constexpr int32 Counts[] = {0, 1, 2, 2, 2, 2, 2, 5};
    return Counts[FMath::Clamp(static_cast<int32>(Type), 0, 7)];
}

bool UReenactmentLibrary::IsPiecePlacementAllowed(EChessType Type, EChessColor Color, FIntPoint Point)
{
    if (!FChessPosition::IsInside(Point) || Type == EChessType::EMPTY ||
        static_cast<uint8>(Type) > 7 || static_cast<uint8>(Color) > 1) return false;
    // 双方统一换算为从己方底线开始的行号，士、象与兵的静态区域随阵营镜像。
    const int32 Rank = Color == EChessColor::REDCHESS ? Point.X : 9 - Point.X;
    switch (Type)
    {
    case EChessType::JIANG: return FChessPosition::IsInPalace(Point, Color);
    case EChessType::SHI: return (Rank == 0 || Rank == 2) ? Point.Y == 3 || Point.Y == 5 : Rank == 1 && Point.Y == 4;
    case EChessType::XIANG: return (Rank == 0 || Rank == 4) ? Point.Y == 2 || Point.Y == 6 : Rank == 2 && (Point.Y == 0 || Point.Y == 4 || Point.Y == 8);
    case EChessType::BING: return Rank >= 3 && (Rank >= 5 || Point.Y % 2 == 0);
    default: return true; // 车马炮在自定义布阵中可出现在任意棋盘交点。
    }
}

FChessPosition FXQPArrangement::Position() const
{
    FChessPosition Board;
    if (Cells.Num() == 90)
        for (int32 I = 0; I < 90; ++I)
            Board.Squares[I] = {static_cast<EChessType>(Cells[I] & 15), static_cast<EChessColor>(Cells[I] >> 4)};
    return Board;
}

void FXQPArrangement::SetPosition(const FChessPosition& Board)
{
    // SetNumZeroed 在长度已为90时不会清除旧元素；必须整段重置，否则撤阵后的空格会残留棋子。
    Cells.Init(0, 90);
    for (int32 I = 0; I < 90; ++I)
        if (Board.Squares[I].Type != EChessType::EMPTY)
            Cells[I] = static_cast<uint8>(Board.Squares[I].Type) | (static_cast<uint8>(Board.Squares[I].Color) << 4);
}

bool FXQPArrangement::Validate(FString& Error, bool bForPlay) const
{
    Error.Reset();
    if (Cells.Num() != 90 || Difficulty > 4 || Name.Len() > 32 || Author.Len() > 16 || Id.Len() > 36)
    { Error = TEXT("棋局数据格式无效"); return false; }
    int32 Counts[2][8] = {};
    for (uint8 Cell : Cells)
    {
        const int32 Type = Cell & 15, Side = Cell >> 4;
        if (Type > 7 || Side > 1 || (Type == 0 && Cell != 0))
        { Error = TEXT("棋局包含无法识别的棋子"); return false; }
        if (Type && ++Counts[Side][Type] > UReenactmentLibrary::PieceLimit(static_cast<EChessType>(Type)))
        { Error = TEXT("棋子数量超过每方可用上限"); return false; }
    }
    if (!bForPlay) return true;
    if (Counts[0][1] != 1 || Counts[1][1] != 1)
    { Error = TEXT("开始对弈前，请为双方各放置一枚将帅"); return false; }
    FChessPosition Board = Position();
    for (int32 I = 0; I < 90; ++I)
    {
        const auto& Piece = Board.Squares[I];
        const FIntPoint P(I / 9, I % 9);
        if (Piece.Type != EChessType::EMPTY && !UReenactmentLibrary::IsPiecePlacementAllowed(Piece.Type, Piece.Color, P))
        { Error = TEXT("将帅、士相或兵卒的位置不符合经典象棋规则"); return false; }
    }
    // 固定红方先手：黑方不能在等待对手落子时已经受将；红方允许从应将局面开始。
    if (Board.IsInCheck(EChessColor::BLACKCHESS))
    { Error = TEXT("红方先手的棋局中，黑方不能已被将军（请检查将帅照面）"); return false; }
    if (Board.GenerateLegalMoves(EChessColor::REDCHESS).IsEmpty())
    { Error = TEXT("红方没有合法走法，请调整布阵"); return false; }
    return true;
}

void UReenactmentLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Data = XQPSaveStorage::Load<USaveReenactments>(LibraryKey, [](USaveReenactments* Save)
    {
        if (Save->Version != 1 || Save->Games.Num() > MaxGames) return false;
        TSet<FString> Ids;
        FString Error;
        for (const auto& Game : Save->Games)
        {
            if (Game.Id.IsEmpty() || Ids.Contains(Game.Id) || !Game.Validate(Error)) return false;
            Ids.Add(Game.Id);
        }
        return true;
    });
    if (!Data) Data = NewObject<USaveReenactments>(this);
    NewGame();
}

void UReenactmentLibrary::NewGame()
{
    // 新建始终为空盘；默认标准局面由编辑器的显式按钮生成，不隐式覆盖用户意图。
    Draft = {};
    Draft.Cells.SetNumZeroed(90);
    if (const auto* Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>()) Draft.Author = Profile->GetPlayerName();
    bEditing = bDirty = true;
}

bool UReenactmentLibrary::SelectGame(const FString& Id, bool bEdit, FString& Error)
{
    // 编辑可打开未完成草稿；直接开局则额外校验将帅、活动范围与先手合法性。
    const auto* Game = Data->Games.FindByPredicate([&](const FXQPArrangement& Item) { return Item.Id == Id; });
    if (!Game || !Game->Validate(Error, !bEdit))
    { if (!Game) Error = TEXT("棋局已不存在"); return false; }
    Draft = *Game;
    bEditing = bEdit;
    bDirty = false;
    return true;
}

bool UReenactmentLibrary::SaveDraft(const FString& Name, const FString& Author, FString& Error)
{
    FXQPArrangement Candidate = Draft;
    Candidate.Name = CleanLabel(Name, 32);
    Candidate.Author = CleanLabel(Author, 16);
    if (Candidate.Name.IsEmpty() || Candidate.Author.IsEmpty())
    { Error = TEXT("请填写棋局名称与作者署名"); return false; }
    if (!Candidate.Validate(Error)) return false;
    if (Candidate.Id.IsEmpty()) Candidate.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    const auto Previous = Data->Games;
    auto* Existing = Data->Games.FindByPredicate([&](const FXQPArrangement& Game) { return Game.Id == Candidate.Id; });
    if (Existing) *Existing = Candidate;
    else if (Data->Games.Num() < MaxGames) Data->Games.Add(Candidate);
    else { Error = TEXT("棋局列表已满（256局）"); return false; }
    // 保存失败恢复内存列表与脏标记，界面不能误报成功或直接开始对弈。
    if (!XQPSaveStorage::Save(Data, LibraryKey))
    { Data->Games = Previous; Error = TEXT("保存失败，请检查存储空间与写入权限"); return false; }
    Draft = Candidate;
    bDirty = false;
    return true;
}

bool UReenactmentLibrary::RemoveGame(const FString& Id, FString& Error)
{
    Error.Reset();
    const int32 Index = Data->Games.IndexOfByPredicate([&Id](const FXQPArrangement& Game) { return Game.Id == Id; });
    if (Id.IsEmpty() || Index == INDEX_NONE) { Error = TEXT("棋局已不存在，请刷新列表"); return false; }
    const auto Previous = Data->Games;
    Data->Games.RemoveAt(Index);
    if (!XQPSaveStorage::Save(Data, LibraryKey))
    {
        Data->Games = Previous;
        Error = TEXT("移除失败，请检查存储空间与写入权限");
        return false;
    }
    // 保留可能仍在编辑的内容，但解除与已删除存档的关联；再次保存会创建新身份。
    if (Draft.Id == Id) { Draft.Id.Reset(); bDirty = true; }
    return true;
}

bool UReenactmentLibrary::BeginBattle(FString& Error)
{
    // 仅更改运行状态，不改写已保存的Cells；进行中走子与重开都以该开局快照为准。
    if (IsDirty()) { Error = TEXT("请先保存当前棋局"); return false; }
    if (!Draft.Validate(Error, true)) return false;
    bEditing = false;
    return true;
}

FString UReenactmentLibrary::ExportDraft(FString& Error) const
{
    if (!Draft.Validate(Error)) return {};
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("format"), TEXT("XQP.Arrangement"));
    Json->SetNumberField(TEXT("version"), 1);
    Json->SetStringField(TEXT("name"), Draft.Name);
    Json->SetStringField(TEXT("author"), Draft.Author);
    Json->SetBoolField(TEXT("ai"), Draft.bAI);
    Json->SetNumberField(TEXT("difficulty"), Draft.Difficulty);
    TArray<TSharedPtr<FJsonValue>> Cells;
    for (uint8 Cell : Draft.Cells) Cells.Add(MakeShared<FJsonValueNumber>(Cell));
    Json->SetArrayField(TEXT("cells"), Cells);
    FString Result;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

bool UReenactmentLibrary::ImportGame(const FString& Text, FString& Error)
{
    TSharedPtr<FJsonObject> Json;
    if (Text.Len() > 32768 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json)
    { Error = TEXT("无法读取棋局，请导入有效的棋局分享数据"); return false; }
    FXQPArrangement Candidate;
    FString Format;
    double Version, Difficulty;
    const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
    if (!Json->TryGetStringField(TEXT("format"), Format) || Format != TEXT("XQP.Arrangement") ||
        !Json->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
        !Json->TryGetStringField(TEXT("name"), Candidate.Name) || !Json->TryGetStringField(TEXT("author"), Candidate.Author) ||
        !Json->TryGetBoolField(TEXT("ai"), Candidate.bAI) || !Json->TryGetNumberField(TEXT("difficulty"), Difficulty) ||
        !FMath::IsFinite(Difficulty) || Difficulty < 0 || Difficulty > 4 || FMath::FloorToDouble(Difficulty) != Difficulty ||
        !Json->TryGetArrayField(TEXT("cells"), Cells) || Cells->Num() != 90)
    { Error = TEXT("棋局版本或字段无效"); return false; }
    Candidate.Difficulty = static_cast<uint8>(Difficulty);
    for (const auto& Value : *Cells)
    {
        double Cell;
        if (!Value || !Value->TryGetNumber(Cell) || !FMath::IsFinite(Cell) || Cell < 0 || Cell > 255 || FMath::FloorToDouble(Cell) != Cell)
        { Error = TEXT("棋盘数据无效"); return false; }
        Candidate.Cells.Add(static_cast<uint8>(Cell));
    }
    if (!Candidate.Validate(Error)) return false;
    const auto Previous = Draft;
    const bool WasDirty = bDirty;
    Draft = Candidate; // 导入永远创建新ID，不能覆盖同名或同作者的现有棋局。
    if (!SaveDraft(Candidate.Name, Candidate.Author, Error)) { Draft = Previous; bDirty = WasDirty; return false; }
    bEditing = true;
    return true;
}
