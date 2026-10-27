// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_QualitySetting.h"

// 构造界面并初始化控件与事件绑定。
void UUI_Item_QualitySetting::NativeConstruct()
{
	Super::NativeConstruct();
	if (!IsValid(XQP_ComboBox) || !IsValid(XQP_ComboBox->ComboBox) || !IsValid(QualityText)) return;
	XQP_ComboBox->ComboBox->ClearOptions();
	for (const FString& level : LevelText)
	{
		XQP_ComboBox->ComboBox->AddOption(level);
	}

	XQP_ComboBox->ComboBox->OnSelectionChanged.Clear();
	XQP_ComboBox->ComboBox->OnSelectionChanged.AddDynamic(this, &UUI_Item_QualitySetting::OnSelectionChanged);
}

// 接收列表数据并刷新复用条目的显示。
void UUI_Item_QualitySetting::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	if (!IsValid(XQP_ComboBox) || !IsValid(XQP_ComboBox->ComboBox) || !IsValid(QualityText)) return;
	OnSelectionChangedCallBack = nullptr; // 列表复用时先清除上一条数据的回调
	if (UQualitySettingObject* QSObj = Cast<UQualitySettingObject>(ListItemObject))
	{
		XQP_ComboBox->ComboBox->SetSelectedIndex(QSObj->DefaultSelect); // 已选择的质量
		QualityName = QSObj->QualityName;								// 画质选项名称
		OnSelectionChangedCallBack = QSObj->OnSelectionChangedCallBack; // 选择回调
		QualityText->SetText(QualityName);
		bIsInitItem = true;
	}
}

// 将用户选择应用到对应的画质选项。
void UUI_Item_QualitySetting::OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!IsValid(XQP_ComboBox) || !IsValid(XQP_ComboBox->ComboBox) || !IsValid(QualityText)) return;
	if (SelectionType != ESelectInfo::Direct && OnSelectionChangedCallBack)
		OnSelectionChangedCallBack(XQP_ComboBox->ComboBox->GetSelectedIndex()); // 选择后的回调函数
}
