// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include <atomic>
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Async/AsyncWork.h"
#include "AsyncWorker.generated.h"

// 异步操作状态
UENUM(BlueprintType)
enum class EAsyncWorkerState : uint8
{
    Idle,       // 空闲
    Running,    // 运行中
    Paused,     // 暂停
    Completed,  // 完成
    Cancelled   // 取消
};

DECLARE_DELEGATE_OneParam(FOnAsyncWorkCompleted, EAsyncWorkerState);
DECLARE_DELEGATE_OneParam(FOnAsyncWorkProgress, float);
DECLARE_DELEGATE_OneParam(FOnWorkDelegate, TAtomic<bool>&);

/**
 * 异步工作器，封装耗时操作的线程管理
 */
UCLASS()
class XIANGQIPRO_API UAsyncWorker : public UObject
{
    GENERATED_BODY()

public:
    // 初始化同步事件，构造阶段不加入根集合。
    UAsyncWorker();
    // 归还同步事件并释放已完成任务。
    virtual ~UAsyncWorker();

    // 创建异步工作器
    static UAsyncWorker* CreateAndStartWorker(
        TFunction<void(UAsyncWorker*)> WorkFunction,
        TFunction<void(EAsyncWorkerState)> CompletedCallback = nullptr,
        TFunction<void(float)> ProgressCallback = nullptr);

    /**
     * 开始异步工作（基础版本，不带进度委托）
     */
    bool StartAsyncWork(
        const FOnAsyncWorkCompleted& CompletedDelegate);

    /**
     * 开始异步工作（带进度委托版本）
     */
    bool StartAsyncWorkWithProgress(
        const FOnAsyncWorkCompleted& CompletedDelegate,
        const FOnAsyncWorkProgress& ProgressDelegate);

    /**
     * 停止异步工作（线程安全）
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    void StopAsyncWork();

    /**
     * 暂停异步工作（线程安全）
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    bool PauseAsyncWork();

    /**
     * 恢复异步工作（线程安全）
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    bool ResumeAsyncWork();

    /**
     * 获取当前状态
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    EAsyncWorkerState GetCurrentState() const { return CurrentState; }

    /**
     * 是否正在运行
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    bool IsRunning() const { return CurrentState == EAsyncWorkerState::Running; }

    /**
     * 是否已暂停
     */
    UFUNCTION(BlueprintCallable, Category = "Async")
    bool IsPaused() const { return CurrentState == EAsyncWorkerState::Paused; }

    /**
     * 是否应该停止
     */
    bool ShouldStop() const { return bShouldStop; }

    /**
     * 是否应该暂停
     */
    bool ShouldPause() const { return CurrentState.load() == EAsyncWorkerState::Paused; }

    /**
     * 设置工作函数
     */
    void SetWorkFunction(TFunction<void(UAsyncWorker*)> InWorkFunction);

    /**
     * 更新进度（线程安全，可在工作函数中调用）
     */
    void ReportProgress(float Progress);

    /**
     * 检查暂停状态（在工作函数中定期调用）
     */
    void CheckPause();

protected:
    // GC销毁前等待任务退出，保证任务不访问已销毁的工作器。
    virtual void BeginDestroy() override;

private:
    // 内部异步任务类
    class FWorkerTask : public FNonAbandonableTask
    {
    public:
        // 任务运行期间工作器由根引用保活。
        FWorkerTask(UAsyncWorker* InWorker)
            : Worker(InWorker) {
        }

        // 执行工作并始终派送完成通知。
        void DoWork();
        // 获取线程池统计标识。
        FORCEINLINE TStatId GetStatId() const
        {
            RETURN_QUICK_DECLARE_CYCLE_STAT(FWorkerTask, STATGROUP_ThreadPoolAsyncTasks);
        }

    private:
        UAsyncWorker* Worker;
    };

    // 实际启动工作的方法
    bool StartWorkInternal();

    // 线程安全的状态更新
    void SetState(EAsyncWorkerState NewState);

    // 完成回调（在主线程执行）
    void OnWorkCompleted();

    // 进度更新回调（在主线程执行）
    void OnProgressUpdated(float Progress);

    // 在工作线程中执行工作函数
    void ExecuteWorkFunction();

    // 等待暂停恢复
    void WaitForResume();

private:
    // 异步任务实例（避免使用AsyncTask名称）
    FAsyncTask<FWorkerTask>* BackgroundTask;

    // 线程安全的取消标志
    std::atomic<bool> bShouldStop;

    // 当前状态
    std::atomic<EAsyncWorkerState> CurrentState;

    // 工作函数（现在接受UAsyncWorker*参数，以便访问暂停功能）
    TFunction<void(UAsyncWorker*)> WorkFunction;

    // 完成委托
    FOnAsyncWorkCompleted CompletedDelegate;

    // 进度委托
    FOnAsyncWorkProgress ProgressDelegate;

    // 记录工作器是否主动添加了根引用
    bool bOwnsRoot = false;

    // 暂停恢复事件
    FEvent* PauseEvent;

};
