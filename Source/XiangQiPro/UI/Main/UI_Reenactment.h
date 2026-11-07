#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "Styling/SlateTypes.h"
#include "UI_Reenactment.generated.h"

class UUI_Reenactment;

/** 桌面与移动列表项共用行为，各自UMG资产负责布局样式，动态内容由正式存档驱动。 */
UCLASS()
class XIANGQIPRO_API UUI_ReenactmentEntry : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    void InitEntry(UUI_Reenactment* InOwner, const FString& InId, const FString& Name, const FString& Author);
    void SetSelected(bool bSelected);
    // 正常/选中两套样式由UMG默认值提供；选中直接替换卡片皮肤，不叠加白色描边。
    UPROPERTY(EditDefaultsOnly, Category="Entry Style") FButtonStyle UnselectedStyle;
    UPROPERTY(EditDefaultsOnly, Category="Entry Style") FButtonStyle SelectedStyle;
    UPROPERTY(EditDefaultsOnly, Category="Entry Style") FLinearColor UnselectedTextColor = FLinearColor::White;
    UPROPERTY(EditDefaultsOnly, Category="Entry Style") FLinearColor SelectedTextColor = FLinearColor(.83f,.69f,.22f,1.f);
private:
    TWeakObjectPtr<UUI_Reenactment> Page;
    FString Id;
    UFUNCTION() void Choose();
    UFUNCTION() void RequestRemoval();
};

/** UMG资产持有布局与视觉，C++只绑定行为并刷新动态值。 */
UCLASS()
class XIANGQIPRO_API UUI_Reenactment : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    /** 0列表、1棋盘编辑、2保存、3编辑暂停、4导入。由各UMG默认值配置。 */
    UPROPERTY(EditDefaultsOnly, Category="Reenactment") int32 Screen = 0;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    void SelectEntry(const FString& Id);
    void RequestRemoveEntry(const FString& Id);
    /** 列表项点击统一入口：单击选中，双击同一条目时直接选中并进入对弈。 */
    void HandleEntryClick(const FString& Id);
private:
    FString SelectedId;
    // 双击判定：记录上次点击的条目与时间，避免单击与双击两种意图互相冲突。
    FString LastEntryClickId;
    double LastEntryClickTime = -1.0;
    TWeakObjectPtr<class ACamera2PActor> EditorCamera;
    bool bOpeningBattle = false;
    // 保存页实例携带入口意图，不能把暂停保存、开始前保存和退出前保存混为一条流程。
    enum class ESaveCompletion : uint8 { KeepEditing, StartBattle, ReturnToMainMenu };
    ESaveCompletion SaveCompletion = ESaveCompletion::KeepEditing;
    void OpenSaveWithCompletion(ESaveCompletion Completion);
    void SaveBeforeMainMenu();
    void TravelToMainMenu();
    FString PendingRemoveId;
    TWeakObjectPtr<class UUI_CommonDialog> RemoveDialog;
    void ConfirmRemoveEntry();
    void RefreshLibrary();
    void RefreshEditor();
    void SetStatus(const FString& Message);
    void OpenPage(const TCHAR* Path);
    void TravelToBattle();
    UFUNCTION() void NewGame();
    UFUNCTION() void EditGame();
    UFUNCTION() void StartGame();
    UFUNCTION() void OpenSave();
    UFUNCTION() void SaveGame();
    UFUNCTION() void ExportGame();
    UFUNCTION() void OpenImport();
    UFUNCTION() void ImportGame();
    UFUNCTION() void PasteImport();
    UFUNCTION() void ClosePage();
    UFUNCTION() void MainMenu();
    UFUNCTION() void OpenSettings();
    UFUNCTION() void OpenPause();
    UFUNCTION() void ClearBoard();
    UFUNCTION() void ConfirmClear();
    UFUNCTION() void StandardBoard();
    UFUNCTION() void SwitchColor();
    UFUNCTION() void RemovePiece();
    UFUNCTION() void AIChanged(bool bChecked);
    UFUNCTION() void DifficultyChanged(FString Item, ESelectInfo::Type Selection);
    UFUNCTION() void SelectKing();
    UFUNCTION() void SelectAdvisor();
    UFUNCTION() void SelectElephant();
    UFUNCTION() void SelectHorse();
    UFUNCTION() void SelectChariot();
    UFUNCTION() void SelectCannon();
    UFUNCTION() void SelectPawn();
};
