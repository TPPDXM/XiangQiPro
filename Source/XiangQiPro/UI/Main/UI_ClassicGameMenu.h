// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_ClassicGameMenu.generated.h"

/**
 * 经典游戏菜单
 */
UCLASS()
class XIANGQIPRO_API UUI_ClassicGameMenu : public UUserWidget
{
	GENERATED_BODY()

public:
    // 构造界面并初始化控件与事件绑定。
    virtual void NativeConstruct() override;
    // 根据名称和图标生成菜单条目。
    UFUNCTION(BlueprintCallable)
    void InitializeList(const TArray<FString>& Items, const TArray<UTexture2D*>& Icons);
    // 选中指定索引的已生成列表项。
    UFUNCTION(BlueprintCallable)
    void SetSelection(int32 Index);
    // 清除所有已生成列表项的选中状态。
    UFUNCTION(BlueprintCallable)
    void ClearSelection();
    // 向蓝图通知新的选择索引。
    UFUNCTION(BlueprintImplementableEvent)
    void OnSelectedChange(int32 Index);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    class UListView* ItemListView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<class UUI_Item_ClassicGame> ListItemClass;

    UPROPERTY(BlueprintReadOnly)
    int32 SelectedIndex = -1;

private:
    // 更新列表选择并通知对应业务。
    UFUNCTION()
    void OnListItemClicked(class UUI_Item_ClassicGame* ClickedItem);

    UPROPERTY(Transient)
    TArray<class UUI_Item_ClassicGame*> ListItems;

    UPROPERTY(Transient)
    TArray<class UClassicGameData*> DataObjList;

};
