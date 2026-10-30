#pragma once
#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "XQPDPISettings.generated.h"

UCLASS()
class XIANGQIPRO_API UXQPDPISettings : public UDPICustomScalingRule
{
    GENERATED_BODY()
public:
    virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;
};
