// Copyright 2026 Ultimate Player All Rights Reserved.

#include "AI2P.h"
#include "XIANGQIPRO/GameObject/ChessBoard2P.h"
#include "XIANGQIPRO/Chess/Chesses.h"
#include <Kismet/GameplayStatics.h>

// ===== 编译期常量：子力价值表 =====
namespace ChessConsts
{
    // 棋子基础价值（编译期数组替代 switch-case）
    constexpr int32 PieceValues[] = {
        0,      // EMPTY
        10000,  // JIANG
        120,    // SHI
        120,    // XIANG
        265,    // MA
        500,    // JV
        270,    // PAO
        60      // BING
    };

    // 各难度对应搜索深度
    constexpr int32 DepthPerDifficulty[] = { 3, 4, 5, 6 };

    // ===== 编译期常量：子力位置价值表 =====
    // 兵/红
    constexpr int32 PST_Bing_Red[10][9] = {
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

    // 兵/黑（红方镜像）
    constexpr int32 PST_Bing_Black[10][9] = {
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

    // 马/红
    constexpr int32 PST_Ma_Red[10][9] = {
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

    // 马/黑（红方镜像）
    constexpr int32 PST_Ma_Black[10][9] = {
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

    // 空表——其他棋子类型暂无位置价值数据，用全零表占位
    constexpr int32 PST_Empty[10][9] = {};

    // 将位置价值表索引到对应数组指针的辅助函数
    inline static const int32(*GetPST(EChessType Type, EChessColor Color))[9]
    {
        const uint8 t = static_cast<uint8>(Type);
        const uint8 c = static_cast<uint8>(Color);
        if (t == static_cast<uint8>(EChessType::BING))
            return (c == 0) ? PST_Bing_Red : PST_Bing_Black;
        if (t == static_cast<uint8>(EChessType::MA))
            return (c == 0) ? PST_Ma_Red : PST_Ma_Black;
        return PST_Empty;
    }
}

UAI2P::UAI2P()
    : BlackKingPos(-1, -1), RedKingPos(-1, -1)
{
    // 位置价值表现在是编译期常量，无需运行时初始化
}

void UAI2P::SetBoard(TWeakObjectPtr<UChessBoard2P> AIMove2P)
{
    LocalAllChess = AIMove2P->AllChess;

    int32 ChessNum = 0;
    BlackKingPos = Position(-1, -1);
    RedKingPos = Position(-1, -1);

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            const auto& chess = LocalAllChess[i][j];
            if (chess.IsValid())
            {
                ++ChessNum;

                // 同时缓存将帅位置
                if (chess->GetType() == EChessType::JIANG)
                {
                    if (chess->GetColor() == EChessColor::BLACKCHESS)
                        BlackKingPos = Position(i, j);
                    else
                        RedKingPos = Position(i, j);
                }
            }
        }
    }

    // 根据剩余棋子数量判断游戏阶段
    if (ChessNum > 30)
        Phase = EGamePhase::Opening;
    else if (ChessNum <= 15)
        Phase = EGamePhase::Ending;
    else
        Phase = EGamePhase::Middle;
}

// 获取AI最优走法（对外接口）
FChessMove2P UAI2P::GetBestMove(TWeakObjectPtr<UChessBoard2P> InBoard2P, EChessColor InAiColor, EAI2PDifficulty InDifficulty)
{
    bStopThinking = false;
    SetBoard(InBoard2P);
    GlobalAIColor = InAiColor;
    GlobalPlayerColor = (GlobalAIColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);

    const int32 Depth = ChessConsts::DepthPerDifficulty[static_cast<uint8>(InDifficulty)];
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
        return { BestMove, EvaluateBoard(GlobalAIColor) };
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

        for (const auto& move : moves)
        {
            WeakChessPtr OriginalChess = MakeTestMove(move);
            const auto [_, evaluation] = Minimax(depth - 1, alpha, beta, false);
            UndoTestMove(move, OriginalChess);

            if (evaluation > maxEval)
            {
                maxEval = evaluation;
                BestMove = move;
            }

            alpha = Math::Max(alpha, evaluation);
            if (beta <= alpha || bStopThinking)
                break;
        }

        return { BestMove, maxEval };
    }
    else
    {
        int32 minEval = INT_MAX;

        for (const auto& move : moves)
        {
            WeakChessPtr OriginalChess = MakeTestMove(move);
            const auto [_, evaluation] = Minimax(depth - 1, alpha, beta, true);
            UndoTestMove(move, OriginalChess);

            if (evaluation < minEval)
            {
                minEval = evaluation;
                BestMove = move;
            }

            beta = Math::Min(beta, evaluation);
            if (beta <= alpha || bStopThinking)
                break;
        }

        return { BestMove, minEval };
    }
}

int32 UAI2P::EvaluateBoard(EChessColor Color)
{
    if (GetKingPos(Color) == Position{-1, -1})
        return INT_MIN;

    int32 Score = 0;
    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto piece = GetChess(i, j); piece.IsValid())
            {
                const int32 value = GetChessValue(piece->GetType())
                    + GetChessPositionValue(piece->GetType(), piece->GetColor(), piece->GetPosition());
                Score += (piece->GetColor() == Color) ? value : -value;
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
        const EChessColor OppoColor = (Color == EChessColor::REDCHESS) ? EChessColor::BLACKCHESS : EChessColor::REDCHESS;
        const Position KingPos = GetKingPos(Color);
        const Position OppoKingPos = GetKingPos(OppoColor);

        for (const auto& move : Moves)
        {
            if (move.to == OppoKingPos)
                return { move };

            WeakChessPtr chess = MakeTestMove(move);
            if (!IsInCheck(Color, move.from != KingPos ? KingPos : move.to))
                SelectedMoves.Emplace(move);
            UndoTestMove(move, chess);
        }
    }

    if (SelectedMoves.IsEmpty())
        SelectedMoves.Append(Moves);

    // MVV-LVA 移动排序（Most Valuable Victim - Least Valuable Attacker）
    SelectedMoves.Sort([this, Color](const FChessMove2P& a, const FChessMove2P& b)
    {
        // 吃子走法优先级排序
        const bool aCapture = GetChess(a.to.X, a.to.Y).IsValid() && GetChess(a.to.X, a.to.Y)->GetColor() != Color;
        const bool bCapture = GetChess(b.to.X, b.to.Y).IsValid() && GetChess(b.to.X, b.to.Y)->GetColor() != Color;

        if (aCapture != bCapture)
            return bCapture < aCapture;  // 吃子优先

        if (aCapture && bCapture)
        {
            // MVV-LVA：被吃子价值 - 攻击子价值（越大越好）
            const int32 aMVVLVA = GetChessValue(GetChess(a.to.X, a.to.Y)->GetType()) * 10
                - GetChessValue(GetChess(a.from.X, a.from.Y)->GetType());
            const int32 bMVVLVA = GetChessValue(GetChess(b.to.X, b.to.Y)->GetType()) * 10
                - GetChessValue(GetChess(b.from.X, b.from.Y)->GetType());
            if (aMVVLVA != bMVVLVA)
                return aMVVLVA > bMVVLVA;
        }

        // 非吃子走法：攻击子价值越小越好（让小子先动）
        const int32 aValue = GetChessValue(GetChess(a.from.X, a.from.Y)->GetType());
        const int32 bValue = GetChessValue(GetChess(b.from.X, b.from.Y)->GetType());
        if (aValue != bValue)
            return bValue < aValue;

        // 位置排序
        return std::tie(a.to.X, a.to.Y) < std::tie(b.to.X, b.to.Y);
    });

    return SelectedMoves;
}

WeakChessPtr UAI2P::MakeTestMove(FChessMove2P Move)
{
    WeakChessPtr OriginalChess = GetChess(Move.to.X, Move.to.Y);

    // 如果移动的是将/帅，更新缓存
    if (auto movedPiece = GetChess(Move.from.X, Move.from.Y); movedPiece.IsValid() && movedPiece->GetType() == EChessType::JIANG)
    {
        if (movedPiece->GetColor() == EChessColor::BLACKCHESS)
            BlackKingPos = Move.to;
        else
            RedKingPos = Move.to;
    }
    
    if (OriginalChess.IsValid() && OriginalChess->GetType() == EChessType::JIANG)
    {
        if (OriginalChess->GetColor() == EChessColor::BLACKCHESS)
            BlackKingPos = { -1, -1 };
        else
            RedKingPos = { -1, -1 };
    }

    LocalAllChess[Move.to.X][Move.to.Y] = GetChess(Move.from.X, Move.from.Y);
    LocalAllChess[Move.from.X][Move.from.Y] = nullptr;

    return OriginalChess;
}

void UAI2P::UndoTestMove(FChessMove2P Move, WeakChessPtr OriginalChess)
{
    LocalAllChess[Move.from.X][Move.from.Y] = GetChess(Move.to.X, Move.to.Y);
    LocalAllChess[Move.to.X][Move.to.Y] = OriginalChess;

    // 恢复将帅位置缓存
    if (auto movedPiece = GetChess(Move.from.X, Move.from.Y); movedPiece.IsValid() && movedPiece->GetType() == EChessType::JIANG)
    {
        if (movedPiece->GetColor() == EChessColor::BLACKCHESS)
            BlackKingPos = Move.from;
        else
            RedKingPos = Move.from;
    }
    
    if (OriginalChess.IsValid() && OriginalChess->GetType() == EChessType::JIANG)
    {
        if (OriginalChess->GetColor() == EChessColor::BLACKCHESS)
            BlackKingPos = Move.to;
        else
            RedKingPos = Move.to;
    }
}

int32 UAI2P::GetChessValue(EChessType Type)
{
    return ChessConsts::PieceValues[static_cast<uint8>(Type)];
}

int32 UAI2P::GetChessPositionValue(EChessType Type, EChessColor Color, Position Pos)
{
    // 直接从编译期常量表读取，零运行时开销
    return ChessConsts::GetPST(Type, Color)[Pos.X][Pos.Y];
}

TArray<FChessMove2P> UAI2P::GenerateAllMoves(EChessColor Color)
{
    TArray<FChessMove2P> moves;

    for (int32 i = 0; i < 10; ++i)
    {
        for (int32 j = 0; j < 9; ++j)
        {
            if (auto Chess = GetChess(i, j); Chess.IsValid())
            {
                if (Chess->GetType() != EChessType::EMPTY && Chess->GetColor() == Color)
                {
                    TArray<FChessMove2P> chessMoves = GenerateMovesForChess(i, j, Chess);
                    moves.Append(chessMoves);
                }
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

void UAI2P::GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
{
    const EChessColor opponentColor = (color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    const Position& opponentKingPos = (opponentColor == EChessColor::BLACKCHESS) ? BlackKingPos : RedKingPos;

    if (opponentKingPos.X == -1) return;

    if (y == opponentKingPos.Y)
    {
        const int32 minX = FMath::Min(x, opponentKingPos.X);
        const int32 maxX = FMath::Max(x, opponentKingPos.X);
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
            moves.Emplace(Position(x, y), opponentKingPos);
    }
}

void UAI2P::GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
{
    static constexpr int32 directions[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

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
}

void UAI2P::GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
{
    static constexpr int32 directions[4][2] = { {-2, -2}, {-2, 2}, {2, -2}, {2, 2} };

    for (const auto& [dx, dy] : directions)
    {
        const int32 newX = x + dx;
        const int32 newY = y + dy;

        if (IsValidPosition(newX, newY))
        {
            if ((color == EChessColor::BLACKCHESS && newX >= 5) || (color == EChessColor::REDCHESS && newX <= 4))
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

void UAI2P::GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
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

void UAI2P::GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves)
{
    TArray<TPair<int32, int32>> directions;

    if (color == EChessColor::BLACKCHESS)
    {
        directions.Emplace(-1, 0);  // 黑方向下
        if (x <= 4)  // 过河后可以左右
        {
            directions.Emplace(0, -1);
            directions.Emplace(0, 1);
        }
    }
    else
    {
        directions.Emplace(1, 0);  // 红方向上
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

WeakChessPtr UAI2P::GetChess(int32 X, int32 Y)
{
    return LocalAllChess[X][Y];
}

bool UAI2P::IsValidPosition(int32 x, int32 y) const noexcept
{
    return x >= 0 && x < 10 && y >= 0 && y < 9;
}

bool UAI2P::IsInPalace(int32 x, int32 y, EChessColor color) const noexcept
{
    return (color == EChessColor::REDCHESS)
        ? (x >= 0 && x <= 2 && y >= 3 && y <= 5)
        : (x >= 7 && x <= 9 && y >= 3 && y <= 5);
}

bool UAI2P::IsInCheck(EChessColor Color, Position KingPos)
{
    const EChessColor OppoColor = (Color == EChessColor::BLACKCHESS) ? EChessColor::REDCHESS : EChessColor::BLACKCHESS;
    const auto OppoMoves = GenerateAllMoves(OppoColor);
    for (const auto& move : OppoMoves)
    {
        if (move.to == KingPos) return true;
    }
    return false;
}

Position UAI2P::GetKingPos(EChessColor Color)
{
    return (Color == EChessColor::BLACKCHESS) ? BlackKingPos : RedKingPos;
}

bool UAI2P::IsJueSha(EChessColor AIColor)
{
    const EChessColor PlayerColor = (AIColor == EChessColor::BLACKCHESS ? EChessColor::REDCHESS : EChessColor::BLACKCHESS);
    const Position PlayerKingPos = GetKingPos(PlayerColor);

    const auto AIMoves = GetAllPossibleMoves(AIColor);

    for (const auto& aimove : AIMoves)
    {
        if (aimove.to == PlayerKingPos)
            return false; // 能直接吃掉玩家的将（对面笑等情况）

        WeakChessPtr chess = MakeTestMove(aimove);
        // 将移动后使用移动后位置检查
        const Position KingToCheck = (aimove.from == GetKingPos(AIColor)) ? aimove.to : GetKingPos(AIColor);

        const auto playerMoves = GetAllPossibleMoves(PlayerColor);
        bool StillInCheck = false;
        for (const auto& player_move : playerMoves)
        {
            if (player_move.to == KingToCheck)
            {
                StillInCheck = true;
                break;
            }
        }
        UndoTestMove(aimove, chess);

        if (!StillInCheck)
            return false;
    }

    return true;
}
