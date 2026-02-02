// Copyright 2026 Ultimate Player All Rights Reserved.

#include "AI2P.h"
#include "XIANGQIPRO/GameObject/ChessBoard2P.h"
#include "XIANGQIPRO/Chess/Chesses.h"
#include <Kismet/GameplayStatics.h>

UAI2P::UAI2P()
{
    PositionValues[EChessType::BING][EChessColor::REDCHESS] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {10, 0, 10, 0, 15, 0, 10, 0, 10},
        {20, 0, 20, 0, 20, 0, 20, 0, 20},
        {30, 0, 30, 0, 35, 0, 30, 0, 30},
        {40, 0, 40, 0, 45, 0, 40, 0, 40},
        {50, 0, 50, 0, 55, 0, 50, 0, 50}
    };

    PositionValues[EChessType::BING][EChessColor::BLACKCHESS] = {
        {50, 0, 50, 0, 55, 0, 50, 0, 50},
        {40, 0, 40, 0, 45, 0, 40, 0, 40},
        {30, 0, 30, 0, 35, 0, 30, 0, 30},
        {20, 0, 20, 0, 20, 0, 20, 0, 20},
        {10, 0, 10, 0, 15, 0, 10, 0, 10},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0}
    };

    PositionValues[EChessType::MA][EChessColor::REDCHESS] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {90, 90, 100, 80, 70, 80, 100, 90, 90},
        {90, 100, 120, 110, 100, 110, 120, 100, 90},
        {90, 110, 120, 130, 120, 130, 120, 110, 90},
        {100, 120, 130, 140, 140, 140, 130, 120, 100},
        {100, 120, 130, 140, 140, 140, 130, 120, 100},
        {90, 110, 120, 130, 120, 130, 120, 110, 90},
        {90, 100, 120, 110, 100, 110, 120, 100, 90},
        {90, 90, 100, 80, 70, 80, 100, 90, 90}
    };

    PositionValues[EChessType::MA][EChessColor::BLACKCHESS] = {
        {90, 90, 100, 80, 70, 80, 100, 90, 90},
        {90, 100, 120, 110, 100, 110, 120, 100, 90},
        {90, 110, 120, 130, 120, 130, 120, 110, 90},
        {100, 120, 130, 140, 140, 140, 130, 120, 100},
        {100, 120, 130, 140, 140, 140, 130, 120, 100},
        {90, 110, 120, 130, 120, 130, 120, 110, 90},
        {90, 100, 120, 110, 100, 110, 120, 100, 90},
        {90, 90, 100, 80, 70, 80, 100, 90, 90},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
}

void UAI2P::SetBoard(TWeakObjectPtr<UChessBoard2P> AIMove2P)
{
    LocalAllChess = AIMove2P->AllChess; 
    int32 ChessNum = 0;
    for (const auto& list : LocalAllChess)
    {
        for (const auto& chess : list)
        {
            if (chess.IsValid())
            {
                ChessNum++;
            }
        }
    }

    if (ChessNum > 30)
    {
        Phase = EGamePhase::Opening;
    }
    else if (ChessNum <= 15)
    {
        Phase = EGamePhase::Ending;
    }
    else
    {
        Phase = EGamePhase::Middle;
    }
}

// 获取AI最优走法（对外接口）
FChessMove2P UAI2P::GetBestMove(TWeakObjectPtr<UChessBoard2P> InBoard2P, EChessColor InAiColor, EAI2PDifficulty InDifficulty)
{
    bStopThinking = false;
    SetBoard(InBoard2P);
    GlobalAIColor = InAiColor;
    GlobalPlayerColor = (GlobalAIColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);

    int32 Depth = 4;
    switch (InDifficulty)
    {
    case EAI2PDifficulty::Easy:
        Depth = 3;
        break;
    case EAI2PDifficulty::Normal:
        Depth = 4;
        break;
    case EAI2PDifficulty::Hard:
        Depth = 5;
        break;
    case EAI2PDifficulty::Master:
        Depth = 6;
        break;
    default:
        Depth = 4;
        break;
    }
    return Minimax(Depth, -INT_MAX, INT_MAX, true).first;
}

void UAI2P::StopThinkingImmediately()
{
    bStopThinking = true;
}

std::pair<FChessMove2P, int32> UAI2P::Minimax(int32 depth, int32 alpha, int32 beta, bool maximiziongPlayer)
{
    FChessMove2P BestMove;
    BestMove.bIsValid = false;

    if (depth == 0)
    {
        return { BestMove, EvaluateBoard(GlobalAIColor)};
    }


    EChessColor CurrentColor = maximiziongPlayer ? GlobalAIColor : GlobalPlayerColor;
    TArray<FChessMove2P> moves = GetAllPossibleMoves(CurrentColor);

    if (moves.Num() == 0)
    {
        return { BestMove, maximiziongPlayer ? -10000 : 10000 };
    }

    if (maximiziongPlayer)
    {
        int32 maxEval = -INT_MAX;

        for (const FChessMove2P& move : moves)
        {
            // 执行移动
            WeakChessPtr OriginalChess = MakeTestMove(move);
            int32 evaluation = Minimax(depth - 1, alpha, beta, false).second;

            // 恢复移动
            UndoTestMove(move, OriginalChess);

            if (evaluation > maxEval)
            {
                maxEval = evaluation;
                BestMove = move;
            }

            // Alpha-Beta剪枝
            alpha = Math::Max(alpha, evaluation);
            if (beta <= alpha)
            {
                break;
            }

            if (bStopThinking)
            {
                break;
            }
        }

        return { BestMove, maxEval };
    }
    else
    {
        int32 minEval = INT_MAX;

        for (const FChessMove2P& move : moves)
        {
            // 执行移动
            WeakChessPtr OriginalChess = MakeTestMove(move);
            int32 evaluation = Minimax(depth - 1, alpha, beta, true).second;

            // 恢复移动
            UndoTestMove(move, OriginalChess); 
            
            if (evaluation < minEval) {
                minEval = evaluation;
                BestMove = move;
            }

            beta = Math::Min(beta, evaluation);
            if (beta <= alpha)
            {
                break;
            }

            if (bStopThinking)
            {
                break;
            }
        }

        return { BestMove, minEval };
    }
}

int32 UAI2P::EvaluateBoard(EChessColor Color)
{
    int32 Score = 0;
    // 计算双方棋子价值差
    for (int32 i = 0; i < 10; i++)
    {
        for (int32 j = 0; j < 9; j++)
        {
            WeakChessPtr piece = GetChess(i, j);
            if (piece.IsValid())
            {
                int32 value = GetChessValue(piece->GetType());
                value += GetChessPositionValue(piece->GetType(), piece->GetColor(), piece->GetPosition());

                if (piece->GetColor() == Color)
                {
                    Score += value;
                }
                else
                {
                    Score -= value;
                }
            }
        }
    }
    return Score;
}

TArray<FChessMove2P> UAI2P::GetAllPossibleMoves(EChessColor Color)
{
    TArray<FChessMove2P> Moves = GenerateAllMoves(Color);
    TArray<FChessMove2P> SelectedMoves;

    if (Phase == EGamePhase::Ending)
    {
        EChessColor OppoColor = Color == EChessColor::REDCHESS ? EChessColor::BLACKCHESS : EChessColor::REDCHESS;
        Position KingPos = GetKingPos(Color);
        Position OppoKingPos = GetKingPos(OppoColor);

        for (const auto& move : Moves)
        {
            if (move.to == OppoKingPos)
            {
                return { move };
            }

            WeakChessPtr chess = MakeTestMove(move);
            if (!IsInCheck(Color, move.from != KingPos ? KingPos : move.to /* 如果是将移动，则需要传入移动后的位置 */))
            {
                SelectedMoves.Add(move);
            }
            UndoTestMove(move, chess);
        }
    }

    if (SelectedMoves.IsEmpty())
    {
        SelectedMoves.Append(Moves);
    }

    SelectedMoves.Sort([this, Color](const FChessMove2P& a, const FChessMove2P& b) {

        // 优先考虑吃子
        bool aCapture = GetChess(a.to.X, a.to.Y).IsValid() && GetChess(a.to.X, a.to.Y)->GetColor() != Color;
        bool bCapture = GetChess(b.to.X, b.to.Y).IsValid() && GetChess(b.to.X, b.to.Y)->GetColor() != Color;

        if (aCapture != bCapture)
        {
            return bCapture < aCapture;
        }

        int32 aValue = GetChessValue(GetChess(a.from.X, a.from.Y)->GetType());
        int32 bValue = GetChessValue(GetChess(b.from.X, b.from.Y)->GetType());

        if (aValue != bValue)
        {
            return bValue < aValue;
        }
        
        if (a.to.X != b.to.X) {
            return a.to.X < b.to.X;
        }
        return a.to.Y < b.to.Y;
        });
    return SelectedMoves;
}

WeakChessPtr UAI2P::MakeTestMove(FChessMove2P Move)
{
    WeakChessPtr OriginalChess = GetChess(Move.to.X, Move.to.Y);
    LocalAllChess[Move.to.X][Move.to.Y] = GetChess(Move.from.X, Move.from.Y);
    LocalAllChess[Move.from.X][Move.from.Y] = nullptr;
    return OriginalChess;
}

void UAI2P::UndoTestMove(FChessMove2P Move, WeakChessPtr OriginalChess)
{
    LocalAllChess[Move.from.X][Move.from.Y] = GetChess(Move.to.X, Move.to.Y);
    LocalAllChess[Move.to.X][Move.to.Y] = OriginalChess;
}

int32 UAI2P::GetChessValue(EChessType Type)
{
    int32 value = 0;
    switch (Type)
    {
    case EChessType::JIANG:
        value = 10000;
        break;
    case EChessType::SHI:
        value = 120;
        break;
    case EChessType::XIANG:
        value = 120;
        break;
    case EChessType::MA:
        value = 265;
        break;
    case EChessType::JV:
        value = 500;
        break;
    case EChessType::PAO:
        value = 270;
        break;
    case EChessType::BING:
        value = 60;
        break;
    default:
        break;
    }
    return value;
}

int32 UAI2P::GetChessPositionValue(EChessType Type, EChessColor Color, Position Pos)
{
    if (PositionValues.find(Type) == PositionValues.end())
    {
        return 0;
    }
    return PositionValues[Type][Color][Pos.X][Pos.Y];
}

TArray<FChessMove2P> UAI2P::GenerateAllMoves(EChessColor Color)
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
            if (Chess->GetType() != EChessType::EMPTY && Chess->GetColor() == Color)
            {
                TArray<FChessMove2P> chessMoves = GenerateMovesForChess(i, j, Chess);
                moves.Append(chessMoves);
            }
        }
    }

    return moves;
}

TArray<FChessMove2P> UAI2P::GenerateMovesForChess(int32 x, int32 y, TWeakObjectPtr<AChesses> chess)
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

void UAI2P::GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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
            if ((color == EChessColor::BLACKCHESS && newX >= 5) || (color == EChessColor::REDCHESS && newX <= 4))
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

void UAI2P::GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

WeakChessPtr UAI2P::GetChess(int32 X, int32 Y)
{
    return LocalAllChess[X][Y];
}

bool UAI2P::IsValidPosition(int32 x, int32 y)
{
    return x >= 0 && x < 10 && y >= 0 && y < 9;
}

bool UAI2P::IsInPalace(int32 x, int32 y, EChessColor color)
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

bool UAI2P::IsInCheck(EChessColor Color, Position KingPos)
{
    EChessColor OppoColor = (Color == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);
    auto OppoMoves = GenerateAllMoves(OppoColor);
    for (const auto& move : OppoMoves)
    {
        if (move.to == KingPos)
        {
            return true;
        }
    }
    return false;
}

Position UAI2P::GetKingPos(EChessColor Color)
{
    // 找到将/帅的位置
    int32 kingX = -1, kingY = -1;

    for (int32 i = 9; i >= 0; i--)
    {
        for (int32 j = 3; j < 6; j++)
        {
            WeakChessPtr chess = GetChess(i, j);
            if (chess.IsValid() && chess->GetType() == EChessType::JIANG && chess->GetColor() == Color)
            {
                kingX = i;
                kingY = j;
                break;
            }
        }
        if (kingX != -1) break;
    }
    return Position(kingX, kingY);
}

bool UAI2P::IsJueSha(EChessColor AIColor)
{
    EChessColor PlayerColor = (AIColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);
    Position KingPos = GetKingPos(AIColor);
    Position PlayerKingPos = GetKingPos(PlayerColor);

    bool bJueSha = true;
    auto AIMoves = GetAllPossibleMoves(AIColor);

    for (const auto& aimove : AIMoves)
    {
        if (aimove.to == PlayerKingPos)
        {
            return false; // 能吃掉玩家的将，如对面笑的情况
        }
        WeakChessPtr chess = MakeTestMove(aimove);

        auto playerMoves = GetAllPossibleMoves(PlayerColor);

        bool StillInCheck = false;
        for (const auto& player_move : playerMoves)
        {
            if (player_move.to == (aimove.from == KingPos ? aimove.to : KingPos))
            {
                StillInCheck = true;
                break;
            }
        }

        UndoTestMove(aimove, chess);

        if (!StillInCheck) // 存在破除将军的办法
        {
            bJueSha = false;
            break;
        }
    }

    return bJueSha;
}
