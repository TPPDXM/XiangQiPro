// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include <atomic>

#include "XiangQiPro/Interface/IF_EndingGame.h"

#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/ChessMove.h"
#include "XiangQiPro/Util/Clock.h"

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "AI2P.generated.h"

class UChessBoard2P;
class UTacticsLibrary2P;
class AChesses;

typedef UAI2P AI2P;
typedef UKismetMathLibrary Math;
typedef TWeakObjectPtr<AChesses> WeakChessPtr;

UENUM(BlueprintType)
enum class EAI2PDifficulty : uint8
{
    Easy = 0,   // 简单
    Normal = 1, // 普通
    Hard = 2,   // 困难
    Master = 3  // 大师
};

enum class EGamePhase : uint8
{
    Opening = 0,
    Middle = 1,
    Ending = 2
};

UCLASS()
class XIANGQIPRO_API UAI2P : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI2P")
    EAI2PDifficulty AIDifficulty = EAI2PDifficulty::Hard;

    // 构造函数
    UAI2P();

    // 核心：获取AI最优走法
    FChessMove2P GetBestMove(TWeakObjectPtr<UChessBoard2P> InBoard2P, EChessColor InAiColor, EAI2PDifficulty InDifficulty);

    // 立刻停止搜索
    UFUNCTION(BlueprintCallable, Category = "Chess AI")
    void StopThinkingImmediately();

    // 设置棋盘引用
    void SetBoard(TWeakObjectPtr<UChessBoard2P> newBoard);

private:

    std::atomic<bool> bStopThinking{false};

    int32 MaxTime = 10000;

    FClock Clock;

    EGamePhase Phase;

    EChessColor GlobalAIColor = EChessColor::BLACKCHESS;

    EChessColor GlobalPlayerColor = EChessColor::REDCHESS;

    TArray<TArray<WeakChessPtr>> LocalAllChess;  // 10行9列的棋盘

    // 缓存将/帅位置，避免每次全盘扫描
    Position BlackKingPos;
    Position RedKingPos;

private:

    std::pair<FChessMove2P, int32> Minimax(int32 depth, int32 alpha, int32 beta, bool maximiziongPlayer);

    int32 EvaluateBoard(EChessColor Color);

    // 获取所有可能走法
    TArray<FChessMove2P> GetAllPossibleMoves(EChessColor Color);

    WeakChessPtr MakeTestMove(FChessMove2P Move);

    void UndoTestMove(FChessMove2P Move, WeakChessPtr OriginalChess);

    int32 GetChessValue(EChessType Type);

    int32 GetChessPositionValue(EChessType Type, EChessColor Color, Position Pos);

    // 生成所有合法走法
    TArray<FChessMove2P> GenerateAllMoves(EChessColor color);

    TArray<FChessMove2P> GenerateMovesForChess(int32 x, int32 y, TWeakObjectPtr<AChesses> chess);

    void GenerateJiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateKingDirectAttackMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateShiMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateXiangMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateMaMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateJvMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GeneratePaoMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    void GenerateBingMoves(int32 x, int32 y, EChessColor color, TArray<FChessMove2P>& moves);

    WeakChessPtr GetChess(int32 X, int32 Y);

    // 检查位置是否在棋盘内
    bool IsValidPosition(int32 x, int32 y) const noexcept;

    // 检查位置是否在九宫格内
    bool IsInPalace(int32 x, int32 y, EChessColor color) const noexcept;

    bool IsInCheck(EChessColor Color, Position KingPos);

    // 找到将的位置
    Position GetKingPos(EChessColor Color);

public:

    // 检查是否绝杀
    bool IsJueSha(EChessColor AIColor);
};