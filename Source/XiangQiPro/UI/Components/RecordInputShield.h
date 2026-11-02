#pragma once
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "RecordInputShield.generated.h"

// 子控件先处理滚动，面板空白处的输入也不得传入棋盘。
UCLASS()
class XIANGQIPRO_API URecordInputShield : public UBorder
{
    GENERATED_BODY()
public:
    // 可选的外围点击操作；面板自身继续只拦截输入。
    FSimpleDelegate OnPressed;
    bool HasPointerCapture() const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
