#pragma once
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "RecordInputShield.generated.h"

// 子控件先处理滚动，面板空白处的输入也不得传入棋盘。
UCLASS()
class XIANGQIPRO_API URecordInputShield : public UBorder
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
