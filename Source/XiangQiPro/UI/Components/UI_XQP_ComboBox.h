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
    UUI_XQP_ComboBox(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* ComboBox;
protected:
    UPROPERTY() TObjectPtr<class UTexture2D> ArrowTexture;
    // 使用显式引用的随包贴图，避免打包遗漏Slate样式文件。
    virtual void NativePreConstruct() override;
    // 移动端按钮在触摸按下时切换，防止弹层先关闭、抬手时又被同一次点击重新打开。
    virtual void NativeConstruct() override;
};
