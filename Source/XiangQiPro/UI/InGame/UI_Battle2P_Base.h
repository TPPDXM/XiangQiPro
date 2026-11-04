// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/Util/ChessMove.h"

#include "CoreMinimal.h"
#include "XiangQiPro/UI/XQPMobileWidget.h"
#include "UI_Battle2P_Base.generated.h"

class AChesses;
class AXQPGameStateBase;
class UTextBlock;
class UImage;
class UScrollBox;
class UVerticalBox;
class UUI_CommonDialog;
class URecordInputShield;

enum class EPlayerTag : uint8;

typedef UUI_Battle2P_Base UI_Battle2P_Base;

// 已提交棋谱的值数据，不引用可能被悔棋销毁的 Actor 或 Slate 控件。
struct FOperatingRecordEntry
{
    EChessColor Color;
    FString Notation;
    FString PlayerName;
    TOptional<int32> Points;
};

/**
 * 双人对战主界面
 */
UCLASS()
class XIANGQIPRO_API UUI_Battle2P_Base : public UXQPMobileWidget
{
	GENERATED_BODY()

private:

	TWeakObjectPtr<AXQPGameStateBase> GameState;

    bool bInfoPanelsSwapped = false;
    void RefreshPlayerInfoLayout();
    // 只拟合进入观战时的默认镜头，用户开始操作后保留原旋转/缩放行为。
    TWeakObjectPtr<class ACamera2PActor> SpectatorCamera;
    void FitSpectatorCamera(const FGeometry& Geometry);

public:

	// 显示玩家1的名字
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UTextBlock* Text_Name_P1;

	// 显示玩家2的名字
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UTextBlock* Text_Name_P2;

	// 显示玩家2的名字
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UTextBlock* Text_Score_P1;

	// 显示玩家2的名字
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UTextBlock* Text_Score_P2;

	UPROPERTY(EditAnywhere, meta = (BindWidgetOptional))
	UTextBlock* Text_AIThinking;

	// 落子历史记录
	UPROPERTY(EditAnywhere, meta = (BindWidgetOptional))
	UTextBlock* Text_OperatingRecord;

	// 玩家1的回合标记
	UPROPERTY(EditAnywhere, meta = (BindWidgetOptional))
	UImage* Image_RoundMark_P1;

	// 玩家2的回合标记
	UPROPERTY(EditAnywhere, meta = (BindWidgetOptional))
	UImage* Image_RoundMark_P2;

public:
	// 构造界面并初始化控件与事件绑定。
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// 更新界面每帧状态。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// 刷新双方得分控件。
	void UpdateScore(int32 score1, int32 score2);
    UFUNCTION(BlueprintCallable, Category="Battle|Score")
    void SetPlayerScoreVisible(bool bVisible);
    UFUNCTION(BlueprintCallable, Category="Battle|Score")
    void SetOpponentScoreVisible(bool bVisible);
    UFUNCTION(BlueprintCallable, Category="Battle")
    void ShowSoloRideResult(int32 Score);
    void ShowBattleStart();

	// 设置是否为AI回合
	void SetAITurn(bool bAITurn);

	// 显示游戏结束
	UFUNCTION(BlueprintImplementableEvent)
	void ShowGameOver(EChessColor winner);
	UFUNCTION(BlueprintCallable)
	virtual void ShowMatchResult(EChessColor Winner);
	UFUNCTION(BlueprintCallable)
	void ShowCurrentResult();
	// 广播重开事件并清空棋谱显示。
	UFUNCTION(BlueprintCallable)
	void ExecGamePlayAgain();

	// 增加落子历史记录
	void AddOperatingRecord(EPlayerTag player, TWeakObjectPtr<AChesses> targetChess, FChessMove2P move, TOptional<int32> MovePoints = {});
    void ResetOperatingRecord();
    int32 GetOperatingRecordCount() const { return RecordMoveCount; }
    void TruncateOperatingRecord(int32 KeepCount);
    void RefreshOperatingRecord();
	// 将棋子移动转换为基本中文棋谱。
	FString GetMoveNotation(TWeakObjectPtr<AChesses> targetChess, FChessMove2P move);
	// 增强版：考虑同列多个相同棋子的情况
	FString GetEnhancedMoveNotation(TWeakObjectPtr<AChesses> targetChess, FChessMove2P move);
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> RecordPanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<URecordInputShield> RecordDismissArea;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UUserWidget> RecordButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> RecordScroll;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> RecordRows;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RecordCount;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RecordEmpty;
    bool bRecordExpanded = false;
    float RecordExpansion = 0.f;
    int32 RecordMoveCount = 0;
    TArray<FOperatingRecordEntry> OperatingRecords;
    UFUNCTION() void ToggleOperatingRecord();
    void SetRecordExpanded(bool bExpanded);
    void UpdateRecordExpansion(float DeltaTime);
    void AddRecordRow(EChessColor Color, const FString& Notation, const FString& PlayerName, TOptional<int32> MovePoints);
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> PlayerScorePanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> OpponentScorePanel;
    bool bPlayerScoreVisible = true;
    bool bOpponentScoreVisible = true;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ModeStatus;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UUserWidget> MenuButton;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UUI_CommonDialog> CommonDialogClass;
    TWeakObjectPtr<UUI_CommonDialog> ResultDialog;
    UUI_CommonDialog* OpenResultDialog();
    void HandleRoomUpdated();
    void RefreshRematchAction();
    void PlayAgain();
    void ReturnToMainMenu();
    UFUNCTION() void OpenMenu();
};
