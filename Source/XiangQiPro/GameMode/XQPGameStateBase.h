// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "XiangQiPro/Interface/IF_GameState.h"
#include "XiangQiPro/Util/ChessMove.h"

#include "CoreMinimal.h"
#include "XiangQiPro/AI/ChessSearch.h"
#include "GameFramework/GameStateBase.h"
#include "XQPGameStateBase.generated.h"

class UAI2P;
class UChessBoard2P;
class UUI_Battle2P_Base;
class AChessBoard2PActor;
class AChesses;
class UProBattleComponent;
class UBattleStartComponent;

enum class EChessColor : uint8;
enum class EAI2PDifficulty : uint8;

typedef AXQPGameStateBase GS;

UENUM(BlueprintType)
enum class EBattleType : uint8
{
	P2 = 0 UMETA(DisplayName = "2Players"),
	P2_AI = 1 UMETA(DisplayName = "2Players With AI"),
	P3 = 2 UMETA(DisplayName = "3Players"),
	SoloRide = 3 UMETA(DisplayName = "千里走单骑")
};

enum class EPlayerTag : uint8
{
	AI = 0,
	P1 = 1,
	P2 = 2,
	P3 = 3
};

/**
 *
 */
UCLASS()
class XIANGQIPRO_API AXQPGameStateBase : public AGameStateBase, public IIF_GameState
{
	GENERATED_BODY()

private:

	TWeakObjectPtr<AChessBoard2PActor> board2PActor;

	EBattleType battleType;

	EPlayerTag battleTurn = EPlayerTag::P1;

	EPlayerTag MyPlayerTag = EPlayerTag::P1;

	TWeakObjectPtr<UUI_Battle2P_Base> HUD2P;

	// 玩家1的得分
	int32 score1 = 0;

	// 玩家2的得分
	int32 score2 = 0;

	// 玩家3的得分
	int32 score3 = 0;

	bool bGameOver = false;
    EChessColor WinnerColor = EChessColor::REDCHESS;
    int32 CurrentEndingIndex = INDEX_NONE;
    int32 EndingMoveCount = 0;
    int32 EndingBestSteps = 0;
    bool bEndingRecordBroken = false;

    bool bAIPaused = false;
    bool bMoveInProgress = false;
    bool bHasPendingAIResult = false;
    bool bEndingPlay = false;
    uint64 AIRequestId = 0;
    uint64 SearchBoardKey = 0;
    uint64 SearchBoardRevision = 0;
    FChessSearchResult PendingAIResult;
    bool bApplyingNetworkMove = false;
    TSharedPtr<FChessSearch, ESPMode::ThreadSafe> ActiveSearch;
    FTimerHandle AIReactionTimer;
    double AIActAfter = 0;

    // 取消当前搜索并使已排队的完成回调失效。
    void CancelAIRequest();
    // 在游戏线程接收结果，暂停期间暂存结果。
    void ReceiveAIResult(uint64 RequestId, const FChessSearchResult& Result);
    // 校验局面并提交AI落子或无棋可走的结算。
    void ApplyAIResult(const FChessSearchResult& Result);
    // 统一处理普通胜负和残局通关。
    void Finish2PGame(EChessColor Winner);
    // 更新双方局面评估或玩法得分。
    void UpdateScore();

public:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pro Battle")
    TObjectPtr<UProBattleComponent> ProBattle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Battle")
    TObjectPtr<UBattleStartComponent> BattleStart;
    void OnBattleStarted();
    bool IsProBattle() const;
    void FinishProBattle(EChessColor Winner);

	UPROPERTY(EditAnywhere)
	EAI2PDifficulty AIDifficulty;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAI2P> AI2P;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UChessBoard2P> board2P;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bEnableMachineLearning = false;

	// 初始化默认对战模式和AI难度。
	AXQPGameStateBase();

	// 读取本局的难度配置。
	virtual void BeginPlay() override;

	// 退出关卡时取消搜索并释放本局引用。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 游戏暂停事件
	virtual void GamePause(UObject* OwnerObject) override;

	// 游戏恢复事件
	virtual void GameResume(UObject* OwnerObject) override;

	// 游戏重开事件
	virtual void GamePlayAgain(UObject* OwnerObject) override;

	// 显示置棋位置标记
	void ShowSettingPoint2P(TArray<FChessMove2P> Moves, TWeakObjectPtr<AChesses> Target) const;

	// 使所有置棋位置标记消失
	void DismissSettingPoint2P() const;

	// 获取双人棋盘(非Actor)
	TWeakObjectPtr<UChessBoard2P> GetChessBoard2P();
	// 获取当前棋盘Actor的弱引用。
	TWeakObjectPtr<AChessBoard2PActor> GetChessBoard2PActor() const;

	// 获取对战人数类型
	EBattleType GetBattleType() const;

	// 获取执棋对象
	EPlayerTag GetBattleTurn() const;
	// 修改执棋方时取消旧回合的搜索。
	void SetBattleTurn(EPlayerTag BT);
	// 绑定界面并同步当前得分和回合。
	void SetHUD2P(TWeakObjectPtr<UUI_Battle2P_Base> hud2P);

	// 获取玩家1的得分
	int32 GetScore1() const;

	// 获取玩家2的得分
	int32 GetScore2() const;

	// 获取玩家3的得分
	int32 GetScore3() const;

	// 开始双人游戏
	void Start2PGame(TWeakObjectPtr<AChessBoard2PActor> InBoard2PActor);

	// 应用棋子移动
	void ApplyMove2P(TWeakObjectPtr<AChesses> target, FChessMove2P move);

	// 双人对战棋子移动完毕
	void OnFinishMove2P(TWeakObjectPtr<AChesses> Target);

	// 运行双人象棋AI
	UFUNCTION(BlueprintCallable, Category = "AI")
	void RunAI2P();

	// 判断是否到了玩家的回合
	bool IsMyTurn() const;

	// 转换进攻方
	void SwitchBattleTurn();

	// 游戏结束
	void NotifyGameOver(EChessColor winner);
    bool CanApplyNetworkMove() const;
    void ApplyNetworkMove(const FChessMove2P& Move);
    EChessColor GetTurnColor() const { return battleTurn == EPlayerTag::P1 ? EChessColor::REDCHESS : EChessColor::BLACKCHESS; }
    bool IsGameOver() const { return bGameOver; }
    EChessColor GetWinnerColor() const { return WinnerColor; }
    int32 GetCurrentEndingIndex() const { return CurrentEndingIndex; }
    int32 GetEndingMoveCount() const { return EndingMoveCount; }
    int32 GetEndingBestSteps() const { return EndingBestSteps; }
    bool DidBreakEndingRecord() const { return bEndingRecordBroken; }
    bool IsLANGame() const;
    FTimerHandle ProResultTimer;

};
