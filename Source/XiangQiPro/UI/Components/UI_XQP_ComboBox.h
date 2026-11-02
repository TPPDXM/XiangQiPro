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
};
