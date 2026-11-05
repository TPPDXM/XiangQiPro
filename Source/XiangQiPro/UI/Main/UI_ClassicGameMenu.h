// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_ClassicGameMenu.generated.h"

/**
 * 经典游戏菜单
 */
UCLASS()
class XIANGQIPRO_API UUI_ClassicGameMenu : public UXQPMobileWidget
{
	GENERATED_BODY()

public:
    // 构造界面并初始化控件与事件绑定。
    virtual void NativeConstruct() override;
    // 根据名称和图标生成菜单条目。
    UFUNCTION(BlueprintCallable)
    void InitializeList(const TArray<FString>& Items, const TArray<UTexture2D*>& Icons);
    // 选中指定索引，条目尚未生成时先保存逻辑选择。
    UFUNCTION(BlueprintCallable)
    void SetSelection(int32 Index);
    // 条目延迟生成时只初始化一次，之后不覆盖玩家当前选中的模式。
    UFUNCTION(BlueprintCallable)
    void EnsureInitialSelection();
    // 清除所有已生成列表项的选中状态。
    UFUNCTION(BlueprintCallable)
    void ClearSelection();
    // 向蓝图通知新的选择索引。
    UFUNCTION(BlueprintImplementableEvent)
    void OnSelectedChange(int32 Index);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional))
    class UListView* ItemListView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<class UUI_Item_ClassicGame> ListItemClass;

    UPROPERTY(BlueprintReadOnly)
    int32 SelectedIndex = -1;
    UPROPERTY(EditDefaultsOnly, Category="Classic Game")
    TSubclassOf<class UUI_LANLobby> LANIntroductionClass;
    // 移动端详情页返回独立的三模式选择页。
    bool HandleMobileBack();

private:
    // PC直接创建四个条目，由UMG垂直框分配整列空间；移动端仍使用原模式选择页。
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UVerticalBox> ModeList;
    UUI_Item_ClassicGame* FindEntry(int32 Index) const;
    UFUNCTION() void OpenMobileReenactment();
    void ShowMobileSelection();
    UFUNCTION() void OpenMobileAI();
    UFUNCTION() void OpenMobileEnding();
    UFUNCTION() void OpenMobileLAN();
    UFUNCTION() void MobileBackClicked();
    // 更新列表选择并通知对应业务。
    UFUNCTION()
    void OnListItemClicked(class UUI_Item_ClassicGame* ClickedItem);

    UPROPERTY(Transient)
    TArray<class UUI_Item_ClassicGame*> ListItems;

    UPROPERTY(Transient)
    TArray<class UClassicGameData*> DataObjList;

};
