// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_Item_QualitySetting.h"
#include "XiangQiPro/Audio/XQPFeedbackAudioSubsystem.h"

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
	// 父菜单重新加入视口时，条目可能保留数据绑定；此时也要恢复对应的两项或四项选项。
	if (UQualitySettingObject* Item = GetListItem<UQualitySettingObject>()) NativeOnListItemObjectSet(Item);
    // 本条目会重建选择委托，在业务绑定完成后恢复统一音效；Direct 初始化仍静音。
    UXQPFeedbackAudioSubsystem::ConfigureFor(this);
}

// 接收列表数据并刷新复用条目的显示。
void UUI_Item_QualitySetting::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	if (!IsValid(XQP_ComboBox) || !IsValid(XQP_ComboBox->ComboBox) || !IsValid(QualityText)) return;
	OnSelectionChangedCallBack = nullptr; // 列表复用时先清除上一条数据的回调
	if (UQualitySettingObject* QSObj = Cast<UQualitySettingObject>(ListItemObject))
	{
		// ListView 会在画质行与开关行之间复用控件，每次绑定都重建选项，避免残留四档文案。
		XQP_ComboBox->ComboBox->ClearOptions();
		const TArray<FString>& Options = QSObj->Options.IsEmpty() ? LevelText : QSObj->Options;
		for (const FString& Option : Options) XQP_ComboBox->ComboBox->AddOption(Option);
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
	{
		const int32 Index = XQP_ComboBox->ComboBox->GetSelectedIndex();
		// 同步列表的数据源，滚出视口再返回时仍显示刚刚保存的选择。
		if (UQualitySettingObject* Item = GetListItem<UQualitySettingObject>()) Item->DefaultSelect = Index;
		OnSelectionChangedCallBack(Index);
	}
}
