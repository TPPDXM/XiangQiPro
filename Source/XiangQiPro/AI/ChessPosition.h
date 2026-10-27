// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/ChessMove.h"

// 搜索只保存值类型，不持有Actor或UObject引用。
struct FChessPiece2P
{
    EChessType Type = EChessType::EMPTY;
    EChessColor Color = EChessColor::REDCHESS;
};

// 游戏规则与搜索共用的棋盘快照，红方从第0行向第9行前进。
struct XIANGQIPRO_API FChessPosition
{
    FChessPiece2P Squares[90] = {};
    bool bVariantRules = false;
    bool bRequireKing = true;

    // 判断坐标是否位于10行9列的棋盘内。
    static bool IsInside(Position Pos);
    // 获取对方阵营，调用者只传入红方或黑方。
    static EChessColor Opponent(EChessColor Color);
    // 判断坐标是否位于指定阵营的九宫内。
    static bool IsInPalace(Position Pos, EChessColor Color);
    // 安全读取棋子，越界位置返回空格。
    FChessPiece2P Get(Position Pos) const;
    // 写入棋子数据，忽略越界位置。
    void Set(Position Pos, FChessPiece2P Piece);
    // 查找指定阵营的将帅，不存在时返回无效坐标。
    Position FindKing(EChessColor Color) const;
    // 统计同一行或列的两个位置之间的棋子数。
    int32 CountBetween(Position From, Position To) const;
    // 判断棋子能否按自身规则到达目标，不检查己方将帅安全。
    bool CanReach(Position From, Position To) const;
    // 判断指定阵营是否受将军，包括将帅照面。
    bool IsInCheck(EChessColor Color) const;
    // 生成单个棋子的伪合法走法，供特殊玩法及兼容接口使用。
    void GeneratePieceMoves(Position From, TArray<FChessMove2P>& Moves) const;
    // 生成合法走法，试走与撤销仅修改两个格子。
    TArray<FChessMove2P> GenerateLegalMoves(EChessColor Color);
    // 应用已验证的走法并返回被吃棋子，供搜索撤销。
    FChessPiece2P MakeMove(const FChessMove2P& Move);
    // 撤销一次试走，恢复原始棋盘。
    void UndoMove(const FChessMove2P& Move, FChessPiece2P Captured);
    // 获取棋子的基础子力价值。
    static int32 PieceValue(EChessType Type);
    // 从指定阵营视角计算子力、位置和残局价值。
    int32 Evaluate(EChessColor Color) const;
    // 计算包含执棋方和规则设置的局面键。
    uint64 Hash(EChessColor Color) const;
};
