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

    // 清空棋盘
    for (int32 i = 0; i < 10; i++)
    {
        TArray<TWeakObjectPtr<AChesses>> ChessList = {};
        for (int32 j = 0; j < 9; j++)
        {
            ChessList.Add(nullptr);
        }
        AllChess.Add(ChessList);
    }
    for (int32 i = 0; i < 10; i++)
    {
        TArray<TWeakObjectPtr<ASettingPoint>> PointList = {};
        for (int32 j = 0; j < 9; j++)
        {
            PointList.Add(nullptr);
        }
        SettingPoints.Add(PointList);
    }

    // 从Actor处获取边界坐标
    FVector BorderLoc1 = ChessBoard2PActor->BorderLoc1;
    FVector BorderLoc2 = ChessBoard2PActor->BorderLoc2;
    
    // 计算间隔长度
    float LengthX = (BorderLoc2.X - BorderLoc1.X) / 9.f;
    float LengthY = (BorderLoc2.Y - BorderLoc1.Y) / 8.f;

    for (int32 i = 0; i < 10; i++)
    {
        TArray<FVector> LocList = {};
        for (int32 j = 0; j < 9; j++)
        {
            LocList.Add(FVector(
                BorderLoc1.X + LengthX * i - 0.02 * i, 
                BorderLoc1.Y + LengthY * j, 
                BorderLoc1.Z
            ));
        }
        BoardLocs.Add(LocList);
    }
}

void UChessBoard2P::ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target)
{
    for (const FChessMove2P& move : Moves)
    {
        if (IsValidPosition(move.to.X, move.to.Y))
        {
            SettingPoints[move.to.X][move.to.Y]->SetActivate(true);
            SettingPoints[move.to.X][move.to.Y]->SetTargetChess(Target);
        }
        else
        {
            ULogger::LogWarning(FString("UChessBoard2P::ShowSettingPoint2P: Movement is invalid, move is :").Append(FString::FromInt(move.to.X)).Append(",").Append(FString::FromInt(move.to.Y)));
        }
    }
}

void UChessBoard2P::DismissSettingPoint2P()
{
    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
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
    if (x >= 0 && x < 10 && y >= 0 && y < 9)
    {
        return AllChess[x][y];
    }
    return nullptr;
}

TWeakObjectPtr<AChesses> UChessBoard2P::GetChess(FIntPoint Pos) const
{
    int32 x = Pos.X, y = Pos.Y;
    if (x >= 0 && x < 10 && y >= 0 && y < 9)
    {
        return AllChess[x][y];
    }
    return nullptr;
}

void UChessBoard2P::SetChess(int32 x, int32 y, TWeakObjectPtr<AChesses> Chess)
{
    if (x >= 0 && x < 10 && y >= 0 && y < 9) 
    {
        AllChess[x][y] = Chess;
    }
}

void UChessBoard2P::ApplyMove(TWeakObjectPtr<AChesses> target, FChessMove2P move)
{
    if (!target.IsValid())
    {
        ULogger::LogError(TEXT("UChessBoard2P::ApplyMove: Can't apply movement, because target chess is nullptr!"));
        return;
    }

    TWeakObjectPtr<AChesses> CaptureChess = GetChess(move.to.X, move.to.Y); // 尝试获取原有位置的棋子
    if (CaptureChess.IsValid()) // 存在棋子
    {
        CaptureChess->Defeated(); // 棋子被吃掉
    }

    SetChess(move.to.X, move.to.Y, target);
    SetChess(move.from.X, move.from.Y, nullptr);
    target->ApplyMove(move);
}

// 在UChessBoard2P类中添加
void UChessBoard2P::DebugCheckBoardState() const
{
    FString boardState = "===================\n";
    for (int32 y = 9; y >= 0; y--) {
        for (int32 x = 0; x < 9; x++) {
            auto chess = GetChess(y, x);
            if (chess.IsValid()) {
                switch (chess->GetType())
                {
                case EChessType::BING:
                    boardState += UTF8_TO_TCHAR("兵");
                    break;
                case EChessType::JIANG:
                    boardState += UTF8_TO_TCHAR("将");
                    break;
                case EChessType::SHI:
                    boardState += UTF8_TO_TCHAR("士");
                    break;
                case EChessType::XIANG:
                    boardState += UTF8_TO_TCHAR("象");
                    break;
                case EChessType::MA:
                    boardState += UTF8_TO_TCHAR("马");
                    break;
                case EChessType::JV:
                    boardState += UTF8_TO_TCHAR("车");
                    break;
                case EChessType::PAO:
                    boardState += UTF8_TO_TCHAR("炮");
                    break;
                default:
                    boardState += UTF8_TO_TCHAR("？");
                    break;
                }
            }
            else {
                boardState += UTF8_TO_TCHAR("。");
            }
            boardState += " ";
        }
        boardState += "\n";
    }
    ULogger::Log("Current Board State:");
    ULogger::Log(boardState);
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

bool UChessBoard2P::IsValidPosition(int32 x, int32 y) const
{
    return x >= 0 && x < 10 && y >= 0 && y < 9;
}

bool UChessBoard2P::IsInPalace(int32 x, int32 y, EChessColor color) const
{
    if (color == EChessColor::REDCHESS) 
    {
        return x >= 0 && x <= 2 && y >= 3 && y <= 5;
    }
    else 
    {
        return x >= 7 && x <= 9 && y >= 3 && y <= 5;
    }
}

int32 UChessBoard2P::CountPiecesBetween(int32 fromX, int32 fromY, int32 toX, int32 toY) const
{
    int32 count = 0;

    if (fromX == toX) // 同一行
    {
        int32 minY = FMath::Min(fromY, toY);
        int32 maxY = FMath::Max(fromY, toY);
        for (int32 y = minY + 1; y < maxY; y++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(fromX, y);
            if (chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            {
                count++;
            }
        }
    }
    else if (fromY == toY) // 同一列
    {
        int32 minX = FMath::Min(fromX, toX);
        int32 maxX = FMath::Max(fromX, toX);
        for (int32 x = minX + 1; x < maxX; x++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(x, fromY);
            if (chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            {
                count++;
            }
        }
    }
    // 如果不是直线，返回-1表示无效
    else
    {
        return -1;
    }

    return count;
}

TArray<FChessMove2P> UChessBoard2P::GenerateAllMoves(EChessColor color)
{
    TArray<FChessMove2P> moves;

    for (int32 i = 0; i < 10; i++) 
    {
        for (int32 j = 0; j < 9; j++) 
        {
            TWeakObjectPtr<AChesses> Chess = GetChess(i, j);
            if (!Chess.IsValid())
            {
                continue;
            }
            if (Chess->GetType() != EChessType::EMPTY && Chess->GetColor() == color)
            {
                TArray<FChessMove2P> chessMoves = GenerateMovesForChess(i, j, Chess);
                moves.Append(chessMoves);
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
        return TArray<FChessMove2P>();
    }
    TArray<FChessMove2P> moves;
    EChessType type = chess->GetType();
    EChessColor color = chess->GetColor();

    switch (type) 
    {
    case EChessType::JIANG:
        GenerateJiangMoves(x, y, color, moves);
        break;
    case EChessType::SHI:
        GenerateShiMoves(x, y, color, moves);
        break;
    case EChessType::XIANG:
        GenerateXiangMoves(x, y, color, moves);
        break;
    case EChessType::MA:
        GenerateMaMoves(x, y, color, moves);
        break;
    case EChessType::JV:
        GenerateJvMoves(x, y, color, moves);
        break;
    case EChessType::PAO:
        GeneratePaoMoves(x, y, color, moves);
        break;
    case EChessType::BING:
        GenerateBingMoves(x, y, color, moves);
        break;
    default:
        break;
    }

    return moves;
}

void UChessBoard2P::GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 将/帅的移动方向：上、下、左、右
    int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (int32 i = 0; i < 4; i++)
    {
        int32 newX = x + directions[i][0];
        int32 newY = y + directions[i][1];

        // 检查是否在九宫格内
        if (IsInPalace(newX, newY, color))
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color)
            {
                moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
            }
        }
    }

    // 添加将帅直接攻击的走法
    GenerateKingDirectAttackMoves(x, y, color, moves);
}

bool UChessBoard2P::AreKingsFacingEachOther() const
{
    // 查找黑将和红帅的位置
    int32 blackKingX = -1, blackKingY = -1;
    int32 redKingX = -1, redKingY = -1;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG)
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

    // 如果没找到将或帅，返回false
    if (blackKingX == -1 || redKingX == -1)
        return false;

    // 将帅必须在同一列（y坐标相同）
    if (blackKingY != redKingY)
        return false;

    // 检查中间是否有棋子阻挡
    int32 minX = FMath::Min(blackKingX, redKingX);
    int32 maxX = FMath::Max(blackKingX, redKingX);

    for (int32 x = minX + 1; x < maxX; x++)
    {
        TWeakObjectPtr<AChesses> chess = GetChess(x, blackKingY);
        if (chess.IsValid() && chess->GetType() != EChessType::EMPTY)
        {
            return false; // 中间有棋子阻挡
        }
    }

    return true; // 将帅面对面且中间无阻挡
}

int32 UChessBoard2P::CountPiecesBetweenKings() const
{
    // 查找黑将和红帅的位置
    int32 blackKingX = -1, blackKingY = -1;
    int32 redKingX = -1, redKingY = -1;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG)
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

    int32 count = 0;
    int32 minX = FMath::Min(blackKingX, redKingX);
    int32 maxX = FMath::Max(blackKingX, redKingX);

    for (int32 x = minX + 1; x < maxX; x++)
    {
        TWeakObjectPtr<AChesses> chess = GetChess(x, blackKingY);
        if (chess.IsValid() && chess->GetType() != EChessType::EMPTY)
        {
            count++;
        }
    }

    return count;
}

void UChessBoard2P::GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 查找对方将/帅的位置
    EChessColor opponentColor = (color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    int32 opponentKingX = -1, opponentKingY = -1;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == opponentColor)
            {
                opponentKingX = i;
                opponentKingY = j;
                break;
            }
        }
        if (opponentKingX != -1) break;
    }

    if (opponentKingX == -1) return;

    // 检查是否在同一列且中间无棋子
    if (y == opponentKingY)
    {
        int32 minX = FMath::Min(x, opponentKingX);
        int32 maxX = FMath::Max(x, opponentKingX);
        bool hasPieceBetween = false;

        for (int32 checkX = minX + 1; checkX < maxX; checkX++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(checkX, y);
            if (chess.IsValid() && chess->GetType() != EChessType::EMPTY)
            {
                hasPieceBetween = true;
                break;
            }
        }

        // 如果中间没有棋子，可以吃掉对方将/帅
        if (!hasPieceBetween)
        {
            moves.Add(FChessMove2P(Position(x, y), Position(opponentKingX, opponentKingY)));
        }
    }
}

void UChessBoard2P::GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 士/仕的移动方向：四个斜方向
    int32 directions[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

    for (int32 i = 0; i < 4; i++) 
    {
        int32 newX = x + directions[i][0];
        int32 newY = y + directions[i][1];

        // 检查是否在九宫格内
        if (IsInPalace(newX, newY, color)) 
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color) 
            {
                moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
            }
        }
    }
}

void UChessBoard2P::GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 象/相的移动方向：四个斜方向（走田字）
    int32 directions[4][2] = { {-2, -2}, {-2, 2}, {2, -2}, {2, 2} };

    for (int32 i = 0; i < 4; i++) 
    {
        int32 newX = x + directions[i][0];
        int32 newY = y + directions[i][1];

        if (IsValidPosition(newX, newY))
        {
            // 检查是否过河
            if ((color == EChessColor::BLACKCHESS && newX >= 5) || (color == EChessColor::REDCHESS && newX <= 4) || 
                (color == EChessColor::BLACKCHESS && x <= 4) || (color == EChessColor::REDCHESS && x >= 5)) // 特殊游戏模式象可能本身就在和对面，此时允许全场移动
            {
                // 检查象眼是否被塞
                int32 eyeX = x + directions[i][0] / 2;
                int32 eyeY = y + directions[i][1] / 2;

                if (GetChess(eyeX, eyeY) == nullptr)
                {
                    TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
                    if (target == nullptr || target->GetColor() != color)
                    {
                        moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
                    }
                }
            }
        }
    }
}

void UChessBoard2P::GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 马/傌的移动方向：八个方向（走日字）
    int32 directions[8][2] = { {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                           {1, -2}, {1, 2}, {2, -1}, {2, 1} };
    // 马腿位置
    int32 horseLegs[8][2] = { {-1, 0}, {-1, 0}, {0, -1}, {0, 1},
                          {0, -1}, {0, 1}, {1, 0}, {1, 0} };

    for (int32 i = 0; i < 8; i++) 
    {
        int32 newX = x + directions[i][0];
        int32 newY = y + directions[i][1];

        if (IsValidPosition(newX, newY)) 
        {
            // 检查马腿是否被绊
            int32 legX = x + horseLegs[i][0];
            int32 legY = y + horseLegs[i][1];

            if (GetChess(legX, legY) == nullptr) 
            {
                TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
                if (target == nullptr || target->GetColor() != color) 
                {
                    moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
                }
            }
        }
    }
}

void UChessBoard2P::GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 车/俥的移动方向：上、下、左、右
    int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (int32 i = 0; i < 4; i++) 
    {
        int32 step = 1;
        while (true)
        {
            int32 newX = x + directions[i][0] * step;
            int32 newY = y + directions[i][1] * step;

            if (!IsValidPosition(newX, newY))
            {
                break;
            }

            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr)
            {
                moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
            }
            else
            {
                if (target->GetColor() != color)
                {
                    moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
                }
                break;
            }

            step++;
        }
    }
}

void UChessBoard2P::GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 炮/砲的移动方向：上、下、左、右
    int32 directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

    for (int32 i = 0; i < 4; i++) 
    {
        int32 step = 1;
        bool foundPiece = false;

        while (true)
        {
            int32 newX = x + directions[i][0] * step;
            int32 newY = y + directions[i][1] * step;

            if (!IsValidPosition(newX, newY))
            {
                break;
            }

            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (!foundPiece)
            {
                if (target == nullptr)
                {
                    moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
                }
                else
                {
                    foundPiece = true;
                }
            }
            else {
                if (target != nullptr)
                {
                    if (target->GetColor() != color)
                    {
                        moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
                    }
                    break;
                }
            }

            step++;
        }
    }
}

void UChessBoard2P::GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const
{
    // 兵/卒的移动方向
    TArray<TPair<int32, int32>> directions;

    if (color == EChessColor::BLACKCHESS) 
    {
        directions.Add({ -1, 0 });  // 黑方向下移动
        if (x <= 4)  // 过河后可以左右移动
        {
            directions.Add({ 0, -1 });
            directions.Add({ 0, 1 });
        }
    }
    else 
    {
        directions.Add({ 1, 0 });  // 红方向上移动
        if (x >= 5) // 过河后可以左右移动
        {
            directions.Add({ 0, -1 });
            directions.Add({ 0, 1 });
        }
    }

    for (TPair<int32, int32> dir : directions) 
    {
        int32 newX = x + dir.Key;
        int32 newY = y + dir.Value;

        if (IsValidPosition(newX, newY))
        {
            TWeakObjectPtr<AChesses> target = GetChess(newX, newY);
            if (target == nullptr || target->GetColor() != color)
            {
                moves.Add(FChessMove2P(Position(x, y), Position(newX, newY)));
            }
        }
    }
}

bool UChessBoard2P::IsKingInCheck(EChessColor color)
{
    if (!IsValidPosition(0, 0)) return false; // 简单检查棋盘是否有效

    // 找到将/帅的位置
    int32 kingX = -1, kingY = -1;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == color)
            {
                kingX = i;
                kingY = j;
                break;
            }
        }
        if (kingX != -1) break;
    }

    if (kingX == -1 || kingY == -1)
    {
        // 将/帅不存在，这通常意味着已经被将死
        //ULogger::LogWarning(TEXT("UChessBoard2P::IsKingInCheck: King not found! Possibly checkmate."));
        return true;
    }

    EChessColor opponentColor = (color == EChessColor::REDCHESS) ? EChessColor::BLACKCHESS : EChessColor::REDCHESS;

    // 检查每个对手棋子是否能攻击到将/帅
    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (!chess.IsValid() || chess->GetColor() != opponentColor)
                continue;

            // 检查这个棋子是否能攻击到将/帅
            if (CanAttackPosition(i, j, kingX, kingY, opponentColor))
            {
                return true;
            }
        }
    }

    return false;
}

// 检查指定位置的棋子是否能攻击目标位置
bool UChessBoard2P::CanAttackPosition(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor attackerColor) const
{
    TWeakObjectPtr<AChesses> attacker = GetChess(fromX, fromY);
    if (!attacker.IsValid()) return false;

    EChessType type = attacker->GetType();

    // 根据棋子类型检查攻击能力
    switch (type)
    {
    case EChessType::JIANG:
        return CanJiangAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::SHI:
        return CanShiAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::XIANG:
        return CanXiangAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::MA:
        return CanMaAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::JV:
        return CanJvAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::PAO:
        return CanPaoAttack(fromX, fromY, toX, toY, attackerColor);

    case EChessType::BING:
        return CanBingAttack(fromX, fromY, toX, toY, attackerColor);

    default:
        return false;
    }
}

// 将/帅的攻击判断
bool UChessBoard2P::CanJiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    // 首先检查常规的一步移动攻击
    if (FMath::Abs(fromX - toX) + FMath::Abs(fromY - toY) == 1)
    {
        // 目标位置必须在宫殿内
        if (IsInPalace(toX, toY, color))
        {
            return true;
        }
    }

    // 特殊规则：将帅对脸情况下的攻击
    // 查找对方将的位置
    EChessColor opponentColor = (color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    int32 opponentKingX = -1, opponentKingY = -1;

    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            TWeakObjectPtr<AChesses> chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == opponentColor)
            {
                opponentKingX = i;
                opponentKingY = j;
                break;
            }
        }
        if (opponentKingX != -1) break;
    }

    // 如果目标位置就是对方将的位置，且在同一列，中间没有棋子，则可以攻击
    if (opponentKingX != -1 && toX == opponentKingX && toY == opponentKingY)
    {
        // 必须在同一列
        if (fromY == toY)
        {
            // 中间没有棋子，可以攻击
            if (CountPiecesBetweenKings() == 0)
            {
                return true;
            }
        }
    }

    return false;
}

// 士的攻击判断
bool UChessBoard2P::CanShiAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    // 士走斜线一格
    if (FMath::Abs(fromX - toX) != 1 || FMath::Abs(fromY - toY) != 1)
        return false;

    // 必须在九宫内
    if (!IsInPalace(toX, toY, color))
        return false;

    return true;
}

// 象的攻击判断
bool UChessBoard2P::CanXiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    // 象走田字
    if (FMath::Abs(fromX - toX) != 2 || FMath::Abs(fromY - toY) != 2)
        return false;

    // 不能过河
    if ((color == EChessColor::BLACKCHESS && toX > 4) || (color == EChessColor::REDCHESS && toX < 5))
        return false;

    // 检查象眼是否被塞住
    int32 eyeX = (fromX + toX) / 2;
    int32 eyeY = (fromY + toY) / 2;

    if (GetChess(eyeX, eyeY).IsValid())
        return false;

    return true;
}

// 马的攻击判断
bool UChessBoard2P::CanMaAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    int32 dx = FMath::Abs(fromX - toX);
    int32 dy = FMath::Abs(fromY - toY);

    // 马走日字
    if (!((dx == 1 && dy == 2) || (dx == 2 && dy == 1)))
        return false;

    // 检查马腿是否被绊
    int32 legX, legY;
    if (dx == 1)
    {
        // 竖着走日字
        legX = fromX;
        legY = (fromY + toY) / 2;
    }
    else
    {
        // 横着走日字
        legX = (fromX + toX) / 2;
        legY = fromY;
    }

    if (GetChess(legX, legY).IsValid())
        return false;

    return true;
}

// 车的攻击判断
bool UChessBoard2P::CanJvAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    // 车走直线
    if (fromX != toX && fromY != toY)
        return false;

    // 检查路径上是否有其他棋子
    if (CountPiecesBetween(fromX, fromY, toX, toY) > 0)
        return false;

    return true;
}

// 炮的攻击判断
bool UChessBoard2P::CanPaoAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    // 炮走直线
    if (fromX != toX && fromY != toY)
        return false;

    int32 piecesBetween = CountPiecesBetween(fromX, fromY, toX, toY);
    TWeakObjectPtr<AChesses> target = GetChess(toX, toY);

    if (target.IsValid())
    {
        // 吃子需要恰好有一个棋子作为炮架
        return piecesBetween == 1;
    }
    else
    {
        // 移动不能有棋子阻挡
        return piecesBetween == 0;
    }
}

// 兵的攻击判断
bool UChessBoard2P::CanBingAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const
{
    int32 dx = toX - fromX;
    int32 dy = toY - fromY;

    if (color == EChessColor::BLACKCHESS)
    {
        // 黑兵：向下移动，或过河后左右移动
        if (dx == -1 && dy == 0) return true; // 向下
        if (fromX <= 4)
        {
            // 过河后可以左右移动
            if (dx == 0 && (dy == 1 || dy == -1)) return true;
        }
    }
    else
    {
        // 红兵：向上移动，或过河后左右移动
        if (dx == 1 && dy == 0) return true; // 向上
        if (fromX >= 5)
        {
            // 过河后可以左右移动
            if (dx == 0 && (dy == 1 || dy == -1)) return true;
        }
    }

    return false;
}
