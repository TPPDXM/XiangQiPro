// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessBoard2P.h"
#include "ChessBoard2PActor.h"
#include "../Chess/Chesses.h"
#include "../GameObject/SettingPoint.h"


UChessBoard2P::UChessBoard2P()
{
}

void UChessBoard2P::InitializeBoard(TWeakObjectPtr<AChessBoard2PActor> ChessBoard2PActor)
{
    if (!ChessBoard2PActor.IsValid())
    {
        ULogger::LogError(TEXT("Can't initialize board data, because ChessBoard2PActor is nullptr!"));
        return;
    }

    // 清空棋盘（预分配避免多次扩容）
    AllChess.Reserve(10);
    for (int32 i = 0; i < 10; ++i)
    {
        TArray<TWeakObjectPtr<AChesses>> ChessList;
        ChessList.Reserve(9);
        for (int32 j = 0; j < 9; ++j)
            ChessList.Add(nullptr);
        AllChess.Add(ChessList);
    }
    SettingPoints.Reserve(10);
    for (int32 i = 0; i < 10; ++i)
    {
        TArray<TWeakObjectPtr<ASettingPoint>> PointList;
        PointList.Reserve(9);
        for (int32 j = 0; j < 9; ++j)
            PointList.Add(nullptr);
        SettingPoints.Add(PointList);
    }

    // 从Actor处获取边界坐标
    const FVector BorderLoc1 = ChessBoard2PActor->BorderLoc1;
    const FVector BorderLoc2 = ChessBoard2PActor->BorderLoc2;

    // 计算间隔长度
    const float LengthX = (BorderLoc2.X - BorderLoc1.X) / 9.f;
    const float LengthY = (BorderLoc2.Y - BorderLoc1.Y) / 8.f;

    BoardLocs.Reserve(10);
    for (int32 i = 0; i < 10; ++i)
    {
        TArray<FVector> LocList;
        LocList.Reserve(9);
        for (int32 j = 0; j < 9; ++j)
        {
            LocList.Emplace(
                BorderLoc1.X + LengthX * i - 0.02f * i,
                BorderLoc1.Y + LengthY * j,
                BorderLoc1.Z
            );
        }
        BoardLocs.Add(LocList);
    }
}

void UChessBoard2P::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target)
{
    for (const auto& move : Moves)
    {
        if (IsValidPosition(move.to.X, move.to.Y))
        {
            SettingPoints[move.to.X][move.to.Y]->SetActivate(true);
            SettingPoints[move.to.X][move.to.Y]->SetTargetChess(Target);
        }
        else
        {
            ULogger::LogWarning(FString::Printf(
                TEXT("UChessBoard2P::ShowSettingPoint2P: Movement is invalid, move is :%d,%d"),
                move.to.X, move.to.Y));
        }
    }
}

void UChessBoard2P::DismissSettingPoint2P()
{
    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            SettingPoints[i][j]->SetActivate(false);
            SettingPoints[i][j]->SetTargetChess(nullptr);
        }
    }
}

void UChessBoard2P::SetSideToMove(EChessColor color)
{
    sideToMove = color;
}

TWeakObjectPtr<AChesses> UChessBoard2P::GetChess(int32 x, int32 y) const
{
    return (x >= 0 && x < 10 && y >= 0 && y < 9) ? AllChess[x][y] : nullptr;
}

TWeakObjectPtr<AChesses> UChessBoard2P::GetChess(FIntPoint Pos) const
{
    return GetChess(Pos.X, Pos.Y);
}

void UChessBoard2P::SetChess(int32 x, int32 y, TWeakObjectPtr<AChesses> Chess)
{
    if (x >= 0 && x < 10 && y >= 0 && y < 9)
        AllChess[x][y] = Chess;
}

void UChessBoard2P::ApplyMove(TWeakObjectPtr<AChesses> target, FChessMove2P move)
{
    if (!target.IsValid())
    {
        ULogger::LogError(TEXT("UChessBoard2P::ApplyMove: Can't apply movement, because target chess is nullptr!"));
        return;
    }

    if (TWeakObjectPtr<AChesses> CaptureChess = GetChess(move.to.X, move.to.Y); CaptureChess.IsValid())
        CaptureChess->Defeated();

    SetChess(move.to.X, move.to.Y, target);
    SetChess(move.from.X, move.from.Y, nullptr);
    target->ApplyMove(move);
}

void UChessBoard2P::DebugCheckBoardState() const
{
    static const TCHAR* PieceChars[] = {
        TEXT("？"), TEXT("将"), TEXT("士"), TEXT("象"),
        TEXT("马"), TEXT("车"), TEXT("炮"), TEXT("兵")
    };

    TStringBuilder<512> BoardState;
    BoardState.Append(TEXT("===================\n"));

    for (int32 y = 9; y >= 0; --y)
    {
        for (int32 x = 0; x < 9; ++x)
        {
            if (auto chess = GetChess(y, x); chess.IsValid())
                BoardState.Append(PieceChars[static_cast<uint8>(chess->GetType())]);
            else
                BoardState.Append(TEXT("。"));
            BoardState.Append(TEXT(" "));
        }
        BoardState.Append(TEXT("\n"));
    }

    ULogger::Log(TEXT("Current Board State:"));
    ULogger::Log(BoardState.ToString());
}

void UChessBoard2P::MakeTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece)
{
    SetChess(move.to.X, move.to.Y, movedPiece);
    SetChess(move.from.X, move.from.Y, nullptr);
}

void UChessBoard2P::UndoTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece, TWeakObjectPtr<AChesses> capturedPiece)
{
    SetChess(move.from.X, move.from.Y, movedPiece);
    SetChess(move.to.X, move.to.Y, capturedPiece);
}

bool UChessBoard2P::IsValidPosition(int32 x, int32 y) const noexcept
{
    return x >= 0 && x < 10 && y >= 0 && y < 9;
}

bool UChessBoard2P::IsInPalace(int32 x, int32 y, EChessColor color) const noexcept
{
    return (color == EChessColor::REDCHESS)
        ? (x >= 0 && x <= 2 && y >= 3 && y <= 5)
        : (x >= 7 && x <= 9 && y >= 3 && y <= 5);
}

int32 UChessBoard2P::CountPiecesBetween(int32 fromX, int32 fromY, int32 toX, int32 toY) const
{
    if (fromX == toX)
    {
        const int32 minY = FMath::Min(fromY, toY);
        const int32 maxY = FMath::Max(fromY, toY);
        int32 count = 0;
        for (int32 y = minY + 1; y < maxY; ++y)
        {
            if (auto chess = GetChess(fromX, y); chess.IsValid() && chess->GetType() != EChessType::EMPTY)
                ++count;
        }
        return count;
    }
    if (fromY == toY)
    {
        const int32 minX = FMath::Min(fromX, toX);
        const int32 maxX = FMath::Max(fromX, toX);
        int32 count = 0;
        for (int32 x = minX + 1; x < maxX; ++x)
        {
            if (auto chess = GetChess(x, fromY); chess.IsValid() && chess->GetType() != EChessType::EMPTY)
                ++count;
        }
        return count;
    }
    return -1;
}

TArray<FChessMove2P> UChessBoard2P::GenerateAllMoves(EChessColor color)
{
    TArray<FChessMove2P> moves;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto Chess = GetChess(i, j); Chess.IsValid())
            {
                if (Chess->GetType() != EChessType::EMPTY && Chess->GetColor() == color)
                {
                    TArray<FChessMove2P> chessMoves = GenerateMovesForChess(i, j, Chess);
                    moves.Append(chessMoves);
                }
            }
        }
    }

    return moves;
}

TArray<FChessMove2P> UChessBoard2P::GenerateMovesForChess(int32 x, int32 y, TWeakObjectPtr<AChesses> chess)
{
    if (!chess.IsValid())
    {
        ULogger::LogError(TEXT("Can't generate moves for chess, because chess is nullptr!"));
        return {};
    }
    TArray<FChessMove2P> moves;
    const EChessType type = chess->GetType();
    const EChessColor color = chess->GetColor();

    switch (type)
    {
    case EChessType::JIANG: GenerateJiangMoves(x, y, color, moves); break;
    case EChessType::SHI:   GenerateShiMoves(x, y, color, moves);   break;
    case EChessType::XIANG: GenerateXiangMoves(x, y, color, moves); break;
    case EChessType::MA:    GenerateMaMoves(x, y, color, moves);    break;
    case EChessType::JV:    GenerateJvMoves(x, y, color, moves);    break;
    case EChessType::PAO:   GeneratePaoMoves(x, y, color, moves);   break;
    case EChessType::BING:  GenerateBingMoves(x, y, color, moves);  break;
    default: break;
    }

    return moves;
}

void UChessBoard2P::GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (const auto& [dx, dy] : directions)
    {
        const int32 newX = x + dx;
        const int32 newY = y + dy;
        if (IsInPalace(newX, newY, color))
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color)
                moves.Emplace(Position(x, y), Position(newX, newY));
        }
    }

    GenerateKingDirectAttackMoves(x, y, color, moves);
}

bool UChessBoard2P::AreKingsFacingEachOther() const
{
    int32 blackKingX = -1, blackKingY = -1;
    int32 redKingX = -1, redKingY = -1;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); chess.IsValid() && chess->GetType() == EChessType::JIANG)
            {
                if (chess->GetColor() == EChessColor::BLACKCHESS)
                {
                    blackKingX = i;
                    blackKingY = j;
                }
                else
                {
                    redKingX = i;
                    redKingY = j;
                }
            }
        }
    }

    if (blackKingX == -1 || redKingX == -1 || blackKingY != redKingY)
        return false;

    const int32 minX = FMath::Min(blackKingX, redKingX);
    const int32 maxX = FMath::Max(blackKingX, redKingX);

    for (int32 x = minX + 1; x < maxX; ++x)
    {
        if (auto chess = GetChess(x, blackKingY); chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            return false;
    }

    return true;
}

int32 UChessBoard2P::CountPiecesBetweenKings() const
{
    int32 blackKingX = -1, blackKingY = -1;
    int32 redKingX = -1, redKingY = -1;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); chess.IsValid() && chess->GetType() == EChessType::JIANG)
            {
                if (chess->GetColor() == EChessColor::BLACKCHESS)
                {
                    blackKingX = i;
                    blackKingY = j;
                }
                else
                {
                    redKingX = i;
                    redKingY = j;
                }
            }
        }
    }

    if (blackKingX == -1 || redKingX == -1 || blackKingY != redKingY)
        return -1;

    const int32 minX = FMath::Min(blackKingX, redKingX);
    const int32 maxX = FMath::Max(blackKingX, redKingX);
    int32 count = 0;

    for (int32 x = minX + 1; x < maxX; ++x)
    {
        if (auto chess = GetChess(x, blackKingY); chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            ++count;
    }

    return count;
}

void UChessBoard2P::GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    const EChessColor opponentColor = (color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    int32 opponentKingX = -1, opponentKingY = -1;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == opponentColor)
            {
                opponentKingX = i;
                opponentKingY = j;
                break;
            }
        }
        if (opponentKingX != -1) break;
    }

    if (opponentKingX == -1) return;

    if (y == opponentKingY)
    {
        const int32 minX = FMath::Min(x, opponentKingX);
        const int32 maxX = FMath::Max(x, opponentKingX);
        bool hasPieceBetween = false;

        for (int32 checkX = minX + 1; checkX < maxX; ++checkX)
        {
            if (auto chess = GetChess(checkX, y); chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            {
                hasPieceBetween = true;
                break;
            }
        }

        if (!hasPieceBetween)
            moves.Emplace(Position(x, y), Position(opponentKingX, opponentKingY));
    }
}

void UChessBoard2P::GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

    for (const auto& [dx, dy] : directions)
    {
        const int32 newX = x + dx;
        const int32 newY = y + dy;
        if (IsInPalace(newX, newY, color) || !IsInPalace(x, y, color))
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color)
                moves.Emplace(Position(x, y), Position(newX, newY));
        }
    }
}

void UChessBoard2P::GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[4][2] = { {-2, -2}, {-2, 2}, {2, -2}, {2, 2} };

    for (const auto& [dx, dy] : directions)
    {
        const int32 newX = x + dx;
        const int32 newY = y + dy;

        if (IsValidPosition(newX, newY))
        {
            if ((color == EChessColor::BLACKCHESS && newX >= 5) || (color == EChessColor::REDCHESS && newX <= 4) ||
                (color == EChessColor::BLACKCHESS && x <= 4) || (color == EChessColor::REDCHESS && x >= 5))
            {
                const int32 eyeX = x + dx / 2;
                const int32 eyeY = y + dy / 2;

                if (GetChess(eyeX, eyeY) == nullptr)
                {
                    TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
                    if (target == nullptr || target->GetColor() != color)
                        moves.Emplace(Position(x, y), Position(newX, newY));
                }
            }
        }
    }
}

void UChessBoard2P::GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    static constexpr int32 horseLegs[8][2] = {
        {-1, 0}, {-1, 0}, {0, -1}, {0, 1},
        {0, -1}, {0, 1}, {1, 0}, {1, 0}
    };

    for (int32 i = 0; i < 8; ++i)
    {
        const int32 newX = x + directions[i][0];
        const int32 newY = y + directions[i][1];

        if (IsValidPosition(newX, newY))
        {
            const int32 legX = x + horseLegs[i][0];
            const int32 legY = y + horseLegs[i][1];

            if (GetChess(legX, legY) == nullptr)
            {
                TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
                if (target == nullptr || target->GetColor() != color)
                    moves.Emplace(Position(x, y), Position(newX, newY));
            }
        }
    }
}

void UChessBoard2P::GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (const auto& [dx, dy] : directions)
    {
        int32 step = 1;
        while (true)
        {
            const int32 newX = x + dx * step;
            const int32 newY = y + dy * step;

            if (!IsValidPosition(newX, newY)) break;

            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr)
            {
                moves.Emplace(Position(x, y), Position(newX, newY));
            }
            else
            {
                if (target->GetColor() != color)
                    moves.Emplace(Position(x, y), Position(newX, newY));
                break;
            }
            ++step;
        }
    }
}

void UChessBoard2P::GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    static constexpr int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (const auto& [dx, dy] : directions)
    {
        int32 step = 1;
        bool foundPiece = false;

        while (true)
        {
            const int32 newX = x + dx * step;
            const int32 newY = y + dy * step;

            if (!IsValidPosition(newX, newY)) break;

            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (!foundPiece)
            {
                if (target == nullptr)
                    moves.Emplace(Position(x, y), Position(newX, newY));
                else
                    foundPiece = true;
            }
            else
            {
                if (target != nullptr)
                {
                    if (target->GetColor() != color)
                        moves.Emplace(Position(x, y), Position(newX, newY));
                    break;
                }
            }
            ++step;
        }
    }
}

void UChessBoard2P::GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    TArray<TPair<int32, int32>> directions;

    if (color == EChessColor::BLACKCHESS)
    {
        directions.Emplace(-1, 0);
        if (x <= 4)
        {
            directions.Emplace(0, -1);
            directions.Emplace(0, 1);
        }
    }
    else
    {
        directions.Emplace(1, 0);
        if (x >= 5)
        {
            directions.Emplace(0, -1);
            directions.Emplace(0, 1);
        }
    }

    for (const auto& dir : directions)
    {
        const int32 newX = x + dir.Key;
        const int32 newY = y + dir.Value;

        if (IsValidPosition(newX, newY))
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color)
                moves.Emplace(Position(x, y), Position(newX, newY));
        }
    }
}

bool UChessBoard2P::IsKingInCheck(EChessColor color)
{
    if (!IsValidPosition(0, 0)) return false;

    int32 kingX = -1, kingY = -1;
    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == color)
            {
                kingX = i;
                kingY = j;
                break;
            }
        }
        if (kingX != -1) break;
    }

    if (kingX == -1 || kingY == -1) return true;

    const EChessColor opponentColor = (color == EChessColor::REDCHESS) ? EChessColor::BLACKCHESS : EChessColor::REDCHESS;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); !chess.IsValid() || chess->GetColor() != opponentColor)
                continue;

            if (CanAttackPosition(i, j, kingX, kingY, opponentColor))
                return true;
        }
    }

    return false;
}

bool UChessBoard2P::CanAttackPosition(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor attackerColor) const
{
    if (auto attacker = GetChess(fromX, fromY); attacker.IsValid())
    {
        switch (attacker->GetType())
        {
        case EChessType::JIANG: return CanJiangAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::SHI:   return CanShiAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::XIANG: return CanXiangAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::MA:    return CanMaAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::JV:    return CanJvAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::PAO:   return CanPaoAttack(fromX, fromY, toX, toY, attackerColor);
        case EChessType::BING:  return CanBingAttack(fromX, fromY, toX, toY, attackerColor);
        default: return false;
        }
    }
    return false;
}

bool UChessBoard2P::CanJiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    if (FMath::Abs(fromX - toX) + FMath::Abs(fromY - toY) == 1)
    {
        if (IsInPalace(toX, toY, color))
            return true;
    }

    const EChessColor opponentColor = (color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    int32 opponentKingX = -1, opponentKingY = -1;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto chess = GetChess(i, j); chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == opponentColor)
            {
                opponentKingX = i;
                opponentKingY = j;
                break;
            }
        }
        if (opponentKingX != -1) break;
    }

    if (opponentKingX != -1 && toX == opponentKingX && toY == opponentKingY)
    {
        if (fromY == toY && CountPiecesBetweenKings() == 0)
            return true;
    }

    return false;
}

bool UChessBoard2P::CanShiAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    return (FMath::Abs(fromX - toX) == 1 && FMath::Abs(fromY - toY) == 1) && IsInPalace(toX, toY, color);
}

bool UChessBoard2P::CanXiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    if (FMath::Abs(fromX - toX) != 2 || FMath::Abs(fromY - toY) != 2) return false;
    if ((color == EChessColor::BLACKCHESS && toX > 4) || (color == EChessColor::REDCHESS && toX < 5)) return false;

    const int32 eyeX = (fromX + toX) / 2;
    const int32 eyeY = (fromY + toY) / 2;
    return !GetChess(eyeX, eyeY).IsValid();
}

bool UChessBoard2P::CanMaAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    const int32 dx = FMath::Abs(fromX - toX);
    const int32 dy = FMath::Abs(fromY - toY);

    if (!((dx == 1 && dy == 2) || (dx == 2 && dy == 1))) return false;

    const int32 legX = (dx == 1) ? fromX : (fromX + toX) / 2;
    const int32 legY = (dx == 1) ? (fromY + toY) / 2 : fromY;
    return !GetChess(legX, legY).IsValid();
}

bool UChessBoard2P::CanJvAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    return (fromX == toX || fromY == toY) && CountPiecesBetween(fromX, fromY, toX, toY) == 0;
}

bool UChessBoard2P::CanPaoAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    if (fromX != toX && fromY != toY) return false;

    const int32 piecesBetween = CountPiecesBetween(fromX, fromY, toX, toY);
    return GetChess(toX, toY).IsValid() ? (piecesBetween == 1) : (piecesBetween == 0);
}

bool UChessBoard2P::CanBingAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    const int32 dx = toX - fromX;
    const int32 dy = toY - fromY;

    if (color == EChessColor::BLACKCHESS)
    {
        if (dx == -1 && dy == 0) return true;
        if (fromX <= 4 && dx == 0 && (dy == 1 || dy == -1)) return true;
    }
    else
    {
        if (dx == 1 && dy == 0) return true;
        if (fromX >= 5 && dx == 0 && (dy == 1 || dy == -1)) return true;
    }

    return false;
}
