#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "UI_GameModeSetup.h"
#include "UI_FengHuoSetup.generated.h"

class USizeBox;
class UUI_LANLobby;

// 烽火弈适配窗口：扩展通用介绍页，独立管理实时玩法难度与联机房间。
UCLASS()
class XIANGQIPRO_API UUI_FengHuoSetup : public UUI_GameModeSetup
{
    GENERATED_BODY()
public:
    UUI_FengHuoSetup() { ModeId = 6; }
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    bool bLobbyVisible = false;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USizeBox> LobbyHost;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UUI_LANLobby> LobbyClass;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> DifficultyPanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UUI_XQP_ComboBox> DifficultySelector;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> DifficultyChoice;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> DifficultyHint;
    void ShowDifficulty();
    UFUNCTION() void DifficultyChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    void StartGame();
    void ToggleLobby();
    virtual void ConfigureMode() override;
};
