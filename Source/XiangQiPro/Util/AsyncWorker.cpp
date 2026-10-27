// Copyright 2026 Ultimate Player All Rights Reserved.

#include "AsyncWorker.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "Logger.h"

// 仅创建同步事件，避免类默认对象被永久加入根集合。
UAsyncWorker::UAsyncWorker()
    : BackgroundTask(nullptr), bShouldStop(false), CurrentState(EAsyncWorkerState::Idle)
    , PauseEvent(FPlatformProcess::GetSynchEventFromPool(false))
{
}

// GC已保证后台结束，再归还事件。
UAsyncWorker::~UAsyncWorker()
{
    if (PauseEvent) FPlatformProcess::ReturnSynchEventToPool(PauseEvent);
}

// 创建一次性工作器，所有UObject操作都在游戏线程执行。
UAsyncWorker* UAsyncWorker::CreateAndStartWorker(TFunction<void(UAsyncWorker*)> Work, TFunction<void(EAsyncWorkerState)> Completed, TFunction<void(float)> Progress)
{
    check(IsInGameThread());
    UAsyncWorker* Worker = NewObject<UAsyncWorker>();
    Worker->SetWorkFunction(MoveTemp(Work));
    FOnAsyncWorkCompleted Completion;
    Completion.BindLambda([Completed](EAsyncWorkerState State) { if (Completed) Completed(State); });
    FOnAsyncWorkProgress ProgressDelegate;
    if (Progress) ProgressDelegate.BindLambda([Progress](float Value) { Progress(Value); });
    return Worker->StartAsyncWorkWithProgress(Completion, ProgressDelegate) ? Worker : nullptr;
}

// 空闲时设置工作函数，运行中的闭包不可被替换。
void UAsyncWorker::SetWorkFunction(TFunction<void(UAsyncWorker*)> InWorkFunction)
{
    check(IsInGameThread());
    if (!BackgroundTask) WorkFunction = MoveTemp(InWorkFunction);
}

// 设置完成委托并启动任务。
bool UAsyncWorker::StartAsyncWork(const FOnAsyncWorkCompleted& InCompletedDelegate)
{
    return StartAsyncWorkWithProgress(InCompletedDelegate, FOnAsyncWorkProgress());
}

// 拒绝运行中或等待完成回调的工作器再次启动。
bool UAsyncWorker::StartAsyncWorkWithProgress(const FOnAsyncWorkCompleted& InCompletedDelegate, const FOnAsyncWorkProgress& InProgressDelegate)
{
    check(IsInGameThread());
    if (BackgroundTask || !WorkFunction) return false;
    CompletedDelegate = InCompletedDelegate;
    ProgressDelegate = InProgressDelegate;
    return StartWorkInternal();
}

// 任务期间保活工作器，结束回调负责释放根引用。
bool UAsyncWorker::StartWorkInternal()
{
    check(IsInGameThread());
    if (BackgroundTask || !WorkFunction) return false;
    bShouldStop = false;
    if (PauseEvent) PauseEvent->Reset();
    if (!IsRooted())
    {
        AddToRoot();
        bOwnsRoot = true;
    }
    SetState(EAsyncWorkerState::Running);
    BackgroundTask = new FAsyncTask<FWorkerTask>(this);
    BackgroundTask->StartBackgroundTask();
    return true;
}

// 合作式取消，不持锁等待后台，避免完成路径与停止路径互相阻塞。
void UAsyncWorker::StopAsyncWork()
{
    bShouldStop = true;
    SetState(EAsyncWorkerState::Cancelled);
    if (PauseEvent) PauseEvent->Trigger();
}

// 原子切换暂停状态，工作函数通过CheckPause配合暂停。
bool UAsyncWorker::PauseAsyncWork()
{
    EAsyncWorkerState Expected = EAsyncWorkerState::Running;
    if (!CurrentState.compare_exchange_strong(Expected, EAsyncWorkerState::Paused)) return false;
    return true;
}

// 恢复暂停状态并唤醒后台等待。
bool UAsyncWorker::ResumeAsyncWork()
{
    EAsyncWorkerState Expected = EAsyncWorkerState::Paused;
    if (!CurrentState.compare_exchange_strong(Expected, EAsyncWorkerState::Running)) return false;
    if (PauseEvent) PauseEvent->Trigger();
    return true;
}

// 进度通知使用弱引用，避免已排队回调持有悬空this。
void UAsyncWorker::ReportProgress(float Progress)
{
    const TWeakObjectPtr<UAsyncWorker> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Progress]()
    {
        if (WeakThis.IsValid() && !WeakThis->ShouldStop()) WeakThis->OnProgressUpdated(Progress);
    });
}

// 检查合作式暂停点。
void UAsyncWorker::CheckPause()
{
    if (ShouldPause() && !bShouldStop) WaitForResume();
}

// 销毁前确保后台停止并真正释放任务对象。
void UAsyncWorker::BeginDestroy()
{
    StopAsyncWork();
    if (BackgroundTask)
    {
        BackgroundTask->EnsureCompletion();
        delete BackgroundTask;
        BackgroundTask = nullptr;
    }
    Super::BeginDestroy();
}

// 状态本身是原子变量，无需递归加锁。
void UAsyncWorker::SetState(EAsyncWorkerState NewState)
{
    CurrentState.store(NewState);
}

// 清理任务后执行用户回调，不在锁内调用外部代码。
void UAsyncWorker::OnWorkCompleted()
{
    check(IsInGameThread());
    if (!BackgroundTask) return;
    BackgroundTask->EnsureCompletion();
    delete BackgroundTask;
    BackgroundTask = nullptr;
    const EAsyncWorkerState FinalState = bShouldStop ? EAsyncWorkerState::Cancelled : EAsyncWorkerState::Completed;
    SetState(FinalState);
    const FOnAsyncWorkCompleted Callback = CompletedDelegate;
    CompletedDelegate.Unbind();
    ProgressDelegate.Unbind();
    WorkFunction = nullptr;
    if (bOwnsRoot)
    {
        RemoveFromRoot();
        bOwnsRoot = false;
    }
    Callback.ExecuteIfBound(FinalState);
}

// 进度委托只在游戏线程执行。
void UAsyncWorker::OnProgressUpdated(float Progress)
{
    ProgressDelegate.ExecuteIfBound(FMath::Clamp(Progress, 0.f, 1.f));
}

// 工作函数只能访问线程安全数据，并定期检查取消请求。
void UAsyncWorker::ExecuteWorkFunction()
{
    if (WorkFunction && !bShouldStop) WorkFunction(this);
}

// 通过事件等待恢复，不使用循环Sleep占用线程。
void UAsyncWorker::WaitForResume()
{
    while (PauseEvent && ShouldPause() && !bShouldStop) PauseEvent->Wait(100);
}

// 即使任务启动前已被取消，也必须通知游戏线程释放根引用。
void UAsyncWorker::FWorkerTask::DoWork()
{
    if (!Worker) return;
    try
    {
        Worker->ExecuteWorkFunction();
    }
    catch (...)
    {
        Worker->StopAsyncWork();
    }
    const TWeakObjectPtr<UAsyncWorker> WeakWorker(Worker);
    AsyncTask(ENamedThreads::GameThread, [WeakWorker]()
    {
        if (WeakWorker.IsValid()) WeakWorker->OnWorkCompleted();
    });
}
