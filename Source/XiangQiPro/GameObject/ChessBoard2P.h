// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "../Util/ChessInfo.h"
#include "../Util/ChessMove.h"

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "ChessBoard2P.generated.h"

class AChesses;
class AChessBoard2PActor;
class ASettingPoint;

typedef UKismetMathLibrary Math;
typedef UChessBoard2P ChessBoard2P;

/**
 * 
 */
UCLASS()
class XIANGQIPRO_API UChessBoard2P : public UGameInstanceSubsystem
{
	GENERATED_BODY()

    EChessColor sideToMove;

public:

	UChessBoard2P();

    TArray<TArray<TWeakObjectPtr<AChesses>>> AllChess;  // 10行9列的棋盘

    TArray<TArray<FVector>> BoardLocs; // 棋盘置棋点位置

    TArray<TArray<TWeakObjectPtr<ASettingPoint>>> SettingPoints;

public:

    // 初始化棋盘
    void InitializeBoard(TWeakObjectPtr<AChessBoard2PActor> ChessBoard2PActor);

    // 显示置棋位置标记
    void ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target);

    // 使所有置棋位置标记消失
    void DismissSettingPoint2P();

    // 设置执棋方
    void SetSideToMove(EChessColor color);

    // 获取棋子
    TWeakObjectPtr<AChesses> GetChess(int32 x, int32 y) const;

    TWeakObjectPtr<AChesses> GetChess(FIntPoint Pos) const;

    // 设置棋子
    void SetChess(int32 x, int32 y, TWeakObjectPtr<AChesses> Chess);

    void DebugCheckBoardState() const;

    // 移动棋子,仅AI计算使用,你不应该调用这个函数
    void MakeTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece);

    // 撤销移动,仅AI计算使用,你不应该调用这个函数
    void UndoTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece, TWeakObjectPtr<AChesses> capturedPiece);

    // 应用棋子移动
    void ApplyMove(TWeakObjectPtr<AChesses> target, FChessMove2P move);

    // 检查位置是否在棋盘内
    bool IsValidPosition(int32 x, int32 y) const;

    // 检查位置是否在九宫格内
    bool IsInPalace(int32 x, int32 y, EChessColor color) const;

    // 检查两个位置之间是否有棋子（用于炮和车的移动）
    int32 CountPiecesBetween(int32 fromX, int32 fromY, int32 toX, int32 toY) const;

    // 生成所有合法走法
    TArray<FChessMove2P> GenerateAllMoves(EChessColor color);

    // 为特定棋子生成走法
    TArray<FChessMove2P> GenerateMovesForChess(int32 x, int32 y, TWeakObjectPtr<AChesses> chess);

    // 生成将/帅的走法
    void GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 检查将帅是否面对面且中间无棋子阻挡
    bool AreKingsFacingEachOther() const;

    // 获取将帅之间的棋子数量
    int32 CountPiecesBetweenKings() const;

    // 生成将帅直接攻击的走法
    void GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成士/仕的走法
    void GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成象/相的走法
    void GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成马/傌的走法
    void GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成车/俥的走法
    void GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成炮/砲的走法
    void GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 生成兵/卒的走法
    void GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves) const;

    // 将军检测
    bool IsKingInCheck(EChessColor color);

    bool CanAttackPosition(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor attackerColor) const;

    bool CanJiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanShiAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanXiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanMaAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanJvAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanPaoAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    bool CanBingAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
};
