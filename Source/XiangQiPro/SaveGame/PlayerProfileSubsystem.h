#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "XiangQiPro/AI/ChessLearning.h"
#include "PlayerProfileSubsystem.generated.h"
class USavePlayerProfile;
struct FXQPChallengeProgress;
struct FChessDrawHistory;
struct FXQPModeLearningData;

UCLASS()
class XIANGQIPRO_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintPure) FString GetPlayerName() const;
    UFUNCTION(BlueprintCallable) bool SetPlayerName(const FString& Name);
    UFUNCTION(BlueprintPure) int32 GetBestStreak() const;
    UFUNCTION(BlueprintPure) int32 GetCurrentStreak() const;
    UFUNCTION(BlueprintPure) bool HasActiveRun() const;
    UFUNCTION(BlueprintPure) FString GetSaveError() const
    {
        return SaveError;
    }
    UPROPERTY(BlueprintReadWrite) bool bChallengeSelected = false;
    // 开始新挑战或续接已有挑战，返回被锁定的起始难度。
    uint8 BeginOrResumeRun(uint8 Difficulty);
    // 棋盘恢复与逐手保存均使用纯数据快照。
    bool RestoreBoard(FChessPosition& OutBoard, EChessColor& OutTurn) const;
    void SaveBoard(const FChessPosition& Board, EChessColor Turn, int32 RedMoveScore = 0, int32 BlackMoveScore = 0,
        const FChessDrawHistory* DrawHistory = nullptr);
    bool RestoreDrawHistory(FChessDrawHistory& OutHistory, const FChessPosition& Board, EChessColor Turn) const;
    void RestoreMoveScores(int32& RedMoveScore, int32& BlackMoveScore) const;
    void ObserveMove(const FChessPosition& Before, const FChessMove2P& Move);
    void CompleteRound(bool bPlayerWon);
    /** 和棋清除本局棋盘，保留当前连胜、难度及学习数据，下一局重赛。 */
    void CompleteDrawRound();
    /** 重开本盘仅清除恢复快照与待评价的一手，保留连胜、起始难度及已学经验。 */
    void RestartCurrentRound();
    void EndChallenge();
    // 反馈上一组AI落子/玩家回应是否造成明显损失。
    void ReviewAIOutcome(int32 CurrentScore);
    void RecordAIChoice(const FChessPosition& Before, const FChessMove2P& Move, int32 ExpectedScore);
    FChessLearning LearningSnapshot() const;
    // 交替写入双槽并保留可显示的失败信息。
    bool Save();

  private:
    UPROPERTY() TObjectPtr<USavePlayerProfile> Profile;
    FString SaveError;
    FXQPChallengeProgress& ProgressData();
    const FXQPChallengeProgress& ProgressData() const;
    FXQPModeLearningData& LearningData();
    const FXQPModeLearningData& LearningData() const;
    void ClearLearning();
};
