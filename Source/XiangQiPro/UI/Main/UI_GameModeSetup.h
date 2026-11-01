#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "UI_GameModeSetup.generated.h"

// 可复用的玩法介绍窗口；专用玩法通过子类扩展配置与开局行为。
UCLASS()
class XIANGQIPRO_API UUI_GameModeSetup : public UUI_CommonDialog
{
    GENERATED_BODY()
public:
    // 在加入UI栈之前传入稳定的玩法编号。
    void SetMode(int32 Id) { ModeId = Id; }
    virtual void NativeConstruct() override;
protected:
    int32 ModeId = 3;
    virtual void ConfigureMode();
    virtual void StartGame();
};
