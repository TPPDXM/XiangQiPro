#pragma once
#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_LANPause.generated.h"

UCLASS()
class XIANGQIPRO_API UUI_LANPause : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> ResumeButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> LeaveRoomButton;
    UFUNCTION() void Resume();
    UFUNCTION() void Leave();
};
