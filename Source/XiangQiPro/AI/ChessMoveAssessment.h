// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "ChessPosition.h"

struct FChessMoveAssessment
{
    bool bValid = false;
    int32 Points = 0;
    int32 BestValue = 0;
    int32 PlayedValue = 0;
    int32 Nodes = 0;
};

// 所有难度和两台联机电脑使用相同的确定性规则，只评价这一手的选择质量。
class XIANGQIPRO_API FChessMoveAssessor
{
public:
    static FChessMoveAssessment Assess(const FChessPosition& Before, const FChessMove2P& Move);
};
