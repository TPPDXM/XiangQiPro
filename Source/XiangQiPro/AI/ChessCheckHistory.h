#pragma once

#include "ChessPosition.h"

// 本局已实际发生的将军模式；与单次搜索的路径去重分开保存。
struct XIANGQIPRO_API FChessCheckHistory
{
    TMap<uint64, int32> Checks[2];

    void Reset();
    void Record(const FChessPosition& Before, const FChessMove2P& Move);
    bool WouldRepeat(const FChessPosition& Before, const FChessMove2P& Move) const;

private:
    static uint64 CheckKey(const FChessPosition& After, EChessColor Attacker);
};
