// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "Types/SlateEnums.h"
#include "Components/TextBlock.h"
#include "XiangQiPro/UI/Components/UI_XQP_ComboBox.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "QualitySettingObject.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_Item_QualitySetting.generated.h"

/**
 * 画面设置列表条目
 */
UCLASS()
class XIANGQIPRO_API UUI_Item_QualitySetting : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

private:

	bool bIsInitItem = false;

	TArray<FString> LevelText = {
			FString(UTF8_TO_TCHAR("低")),
			FString(UTF8_TO_TCHAR("中")),
			FString(UTF8_TO_TCHAR("高")),
			FString(UTF8_TO_TCHAR("超高")) };

	TFunction<void(int32)> OnSelectionChangedCallBack;

	FText QualityName;

public:

	UPROPERTY(meta = (BindWidget))
	UUI_XQP_ComboBox* XQP_ComboBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* QualityText;
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	// 接收列表数据并刷新复用条目的显示。
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	// 将用户选择应用到对应的画质选项。
	UFUNCTION()
	void OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

};
