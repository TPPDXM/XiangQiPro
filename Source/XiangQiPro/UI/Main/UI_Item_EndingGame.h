// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI_Item_EndingGame.generated.h"

class UButton;
class UTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEndingGameListItemClicked, UUI_Item_EndingGame*, ClickedItem);

/**
 * 残局关卡条目
 */
UCLASS()
class XIANGQIPRO_API UUI_Item_EndingGame : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

	bool bIsInitItem = false;
	bool bSelected = false;
	TWeakObjectPtr<class UUI_Introduce_Ending> SelectionOwner;

	FLinearColor DefaultTextColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	FLinearColor SelectedTextColor = FLinearColor(0.831373f, 0.686275f, 0.215686f, 1.0f);
	FLinearColor DefaultColor = FLinearColor(0.f, 0.f, 0.f, 0.3f);
	FLinearColor SelectedColor = FLinearColor(0.63f, 0.04f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UTextBlock* IndexText;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UButton* ItemButton;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UImage* SelectedIndicator;

protected:

	UPROPERTY(BlueprintAssignable)
	FOnEndingGameListItemClicked OnItemClickedDelegate;
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	// 接收列表数据并刷新复用条目的显示。
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	// 列表条目被回收时清理状态和引用。
	virtual void NativeOnEntryReleased() override;
	// 广播列表条目的点击事件。
	UFUNCTION()
	void OnItemClicked();

public:

	int32 Index = 0;
	// 更新条目选中颜色和轮廓。
	void SetSelected(bool bIsSelected);

};
