// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "../Util/ChessInfo.h"
#include "../Util/ChessMove.h"

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "XiangQiPro/AI/ChessPosition.h"
#include "XiangQiPro/AI/ChessCheckHistory.h"
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

    uint64 Revision = 0;
    EChessColor sideToMove = EChessColor::REDCHESS;

public:
	// 初始化空棋盘，等待棋盘Actor建立坐标。
	UChessBoard2P();

    // 游戏线程内一次性读取棋子类型和阵营，返回独立的纯数据局面。
    FChessPosition CreateSnapshot() const;
    const FChessCheckHistory& GetCheckHistory() const { return CheckHistory; }
    // 验证走法与当前棋盘一致，并检查完整合法性。
    bool IsLegalMove(TWeakObjectPtr<AChesses> Target, const FChessMove2P& Move) const;
    // 清理上一局Actor及所有棋盘缓存。
    void ResetBoard();

    // 获取局面修订号，只能在游戏线程使用。
    uint64 GetRevision() const { return Revision; }
    // 棋子身份改变时使旧搜索结果失效。
    void MarkPositionChanged() { check(IsInGameThread()); ++Revision; }

    bool bVariantRules = false;
    bool bRequireKing = true;
    bool bFlyingKingCapture = true;
    FChessCheckHistory CheckHistory;

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
    // 通过简化坐标读取棋子。
    TWeakObjectPtr<AChesses> GetChess(FIntPoint Pos) const;

    // 设置棋子
    void SetChess(int32 x, int32 y, TWeakObjectPtr<AChesses> Chess);
    // 输出当前棋盘的棋子类型，便于调试。
    void DebugCheckBoardState() const;

    // 游戏线程兼容试走入口，后台搜索使用纯数据棋盘。
    void MakeTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece);

    // 撤销游戏线程中的兼容试走。
    void UndoTestMove(FChessMove2P move, TWeakObjectPtr<AChesses> movedPiece, TWeakObjectPtr<AChesses> capturedPiece);

    // 应用棋子移动
    void ApplyMove(TWeakObjectPtr<AChesses> target, FChessMove2P move);

    // 检查位置是否在棋盘内
    bool IsValidPosition(int32 x, int32 y) const noexcept;

    // 检查位置是否在九宫格内
    bool IsInPalace(int32 x, int32 y, EChessColor color) const noexcept;

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
    // 检查指定阵营的棋子能否攻击目标位置。
    bool CanAttackPosition(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor attackerColor) const;
    // 使用统一规则判断将帅的攻击范围。
    bool CanJiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断士仕的攻击范围。
    bool CanShiAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断象相的攻击范围。
    bool CanXiangAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断马的攻击范围。
    bool CanMaAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断车的攻击范围。
    bool CanJvAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断炮的攻击范围。
    bool CanPaoAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
    // 使用统一规则判断兵卒的攻击范围。
    bool CanBingAttack(int32 fromX, int32 fromY, int32 toX, int32 toY, EChessColor color) const;
};
