// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "Components/ComboBoxString.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_XQP_ComboBox.generated.h"

/**
 * 
 */
UCLASS()
class XIANGQIPRO_API UUI_XQP_ComboBox : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* ComboBox;
protected:
    // 下拉箭头使用Slate内置矢量资源，任意DPI下保持锐利；其余样式由公共UMG模板编辑。
    virtual void NativePreConstruct() override;
};
