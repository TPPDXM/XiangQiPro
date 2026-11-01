// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "ChessPosition.h"

// 内置常见布局的坐标谱；按完整局面匹配，离谱后交回搜索。
struct XIANGQIPRO_API FChessOpeningBook
{
    static TArray<FChessMove2P> FindMoves(const FChessPosition& Position, EChessColor Side);
};
