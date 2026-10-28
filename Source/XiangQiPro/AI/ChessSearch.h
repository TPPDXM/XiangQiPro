// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include <atomic>
#include "ChessPosition.h"
#include "ChessLearning.h"

// 单次搜索的输出，只在工作线程完成后交给游戏线程读取。
struct FChessSearchResult
{
    FChessMove2P Move;
    int32 CompletedDepth = 0;
    int64 Nodes = 0;
    bool bNoLegalMoves = false;
    bool bCancelled = false;
};

// 每次请求独享搜索状态，唯一跨线程可写数据是原子取消标志。
class XIANGQIPRO_API FChessSearch
{
public:
    // 接收游戏线程生成的快照及搜索预算。
    FChessSearch(const FChessPosition& InPosition, EChessColor InColor, int32 InDepth, double InSeconds, FChessLearning InLearning = {});
    // 获取创建请求时的局面键，搜索开始后该值不再改变。
    uint64 GetPositionKey() const { return InitialPositionKey; }
    // 使用迭代加深搜索最佳走法，超时保留上一次完整搜索结果。
    FChessSearchResult FindBestMove();
    // 请求停止搜索，可从游戏线程安全调用。
    void Cancel();

private:
    struct FTableEntry
    {
        uint64 Key = 0;
        uint64 Context = 0;
        FChessMove2P Move;
        int32 Score = 0;
        int16 Depth = -1;
        uint8 Bound = 0;
    };

    // 检查取消和时间限制，未完成的搜索层不会提交结果。
    bool ShouldStop();
    // 执行带Alpha-Beta剪枝和置换表的负极大搜索。
    int32 Negamax(int32 Depth, int32 Alpha, int32 Beta, EChessColor Side, int32 Ply);
    // 延伸吃子局面；被将军时必须搜索所有应将走法。
    int32 Quiescence(int32 Alpha, int32 Beta, EChessColor Side, int32 Ply, int32 Remaining);
    // 按置换表、吃子收益和历史启发排序走法。
    void OrderMoves(TArray<FChessMove2P>& Moves, const FChessMove2P& Preferred, EChessColor Side);
    // 检查当前搜索路径是否出现重复局面。
    bool IsRepeated(uint64 Key) const;
    int32 Evaluate(EChessColor Side) const;

    FChessPosition Board;
    FChessLearning Learning;
    EChessColor Color;
    int32 MaxDepth;
    double TimeBudget;
    double Deadline = 0.0;
    const uint64 InitialPositionKey;
    uint64 PathContext = 0;
    std::atomic<bool> bCancelled{false};
    bool bAborted = false;
    int64 Nodes = 0;
    TArray<FTableEntry> Table;
    TArray<uint64> Path;
    int32 History[2][90][90] = {};
    FChessMove2P RootMove;
};
