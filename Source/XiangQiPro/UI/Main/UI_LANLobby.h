#pragma once
#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_LANLobby.generated.h"
class UEditableTextBox;
class UTextBlock;
class UComboBoxString;
class UUI_XQP_ComboBox;
class UUI_CommonDialog;

// 联机页面由UMG资产提供，嵌入经典模式原有介绍框。
UCLASS()
class XIANGQIPRO_API UUI_LANLobby : public UXQPMobileWidget
{
    GENERATED_BODY()
public:
    bool bProModeLobby = false;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> RoomName;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Address;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PlayersText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RedPlayersText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BlackPlayersText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RedFactionLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BlackFactionLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUI_XQP_ComboBox> RoomSelector;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> RoomChoice;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> StartButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> ReadyButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> ColorButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> CreateButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> SearchButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> JoinButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUserWidget> LeaveButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> BrowserPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RoomPanel;
    FString RoomSignature;
    float RefreshTime = 0;
    TWeakObjectPtr<UUI_CommonDialog> MismatchDialog;
    void ShowModeMismatch(const FString& Reason);
    UFUNCTION() void OnCreate();
    UFUNCTION() void OnSearch();
    UFUNCTION() void OnJoin();
    UFUNCTION() void OnStart();
    UFUNCTION() void OnReady();
    UFUNCTION() void OnColor();
    UFUNCTION() void OnLeave();
    UFUNCTION() void OnRoomSelected(FString Item, ESelectInfo::Type Selection);
};
