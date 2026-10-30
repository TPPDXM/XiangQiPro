// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_Introduce_Ending.generated.h"

class UListView;
class UUI_Item_EndingGame;
class UEndingGameData;

/**
 * 残局游戏介绍界面
 */
UCLASS()
class XIANGQIPRO_API UUI_Introduce_Ending : public UXQPMobileWidget
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<UUI_Item_EndingGame*> ListItems;

	UPROPERTY(Transient)
	TArray<UEndingGameData*> DataObjList;

public:

    // 获取当前选择的残局关卡，列表项通过弱引用读取。
    int32 GetSelectedIndex() const { return UserSelectedIndex; }

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (BindWidget))
	UListView* EndingGameList;

protected:

	UPROPERTY(BlueprintReadOnly)
	int32 UserSelectedIndex = 0;
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	// 更新列表选择并通知对应业务。
	UFUNCTION()
	void OnListItemClicked(UUI_Item_EndingGame* ClickedItem);
	// 选中指定索引的已生成列表项。
	void SetSelection(int32 Index);
	// 清除所有已生成列表项的选中状态。
	void ClearSelection();
};
