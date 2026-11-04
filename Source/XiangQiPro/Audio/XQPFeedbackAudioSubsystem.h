// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/SlateEnums.h"
#include "XQPFeedbackAudioSubsystem.generated.h"

class UUserWidget;
class USoundBase;

/**
 * 本机界面与对局提示音入口。
 * 资源在 GameInstance 建立时加载并持有，交互回调不读取磁盘；按钮使用 Slate
 * 的有效点击声音槽，避免业务回调清理委托、触摸取消或界面重建产生重复声音。
 */
UCLASS()
class XIANGQIPRO_API UXQPFeedbackAudioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UXQPFeedbackAudioSubsystem();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    /** 遍历嵌套 WB_Btn 在内的控件树；可重复调用，保留视觉样式和业务绑定。 */
    static void ConfigureFor(UUserWidget* Widget);
    /** 倒计时只由准备组件的数字变化触发，不能由多份 HUD 各自播放。 */
    void PlayCountdownTick();
    /** 由唯一终局入口调用；平局使用独立中性乐句，避免被当作失败。 */
    void PlayMatchResult(bool bWon, bool bDraw = false);

private:
    UPROPERTY() TObjectPtr<USoundBase> ClickSound;
    UPROPERTY() TObjectPtr<USoundBase> SelectionSound;
    UPROPERTY() TObjectPtr<USoundBase> CountdownSound;
    UPROPERTY() TObjectPtr<USoundBase> VictorySound;
    UPROPERTY() TObjectPtr<USoundBase> DefeatSound;
    UPROPERTY() TObjectPtr<USoundBase> DrawSound;
    void ConfigureWidgetTree(UUserWidget* Widget);
    void PlayFeedback(USoundBase* Sound);
    /** Direct 包含默认值恢复、列表复用与网络刷新，这些程序修改不能发出选择音。 */
    UFUNCTION() void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleComboOpening();
};
