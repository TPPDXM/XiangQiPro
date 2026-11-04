// Copyright 2026 Ultimate Player All Rights Reserved.
#include "KnightEffectsSubsystem.h"
#include "NiagaraComponent.h"

uint64 UKnightEffectsSubsystem::RegisterEffect(UNiagaraComponent* Effect, bool bMove)
{
    if (!IsValid(Effect)) return 0;
    // 外部关卡清理也可能直接销毁组件而不经过自然完成；登记新实例时顺带清理失效租约。
    for (auto It = Leases.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
    TArray<TWeakObjectPtr<UNiagaraComponent>>& Entries = bMove ? Moves : Impacts;
    Entries.RemoveAll([](const auto& Entry) { return !Entry.IsValid(); });
    const int32 Limit = bMove ? 3 : 6;
    while (Entries.Num() >= Limit)
    {
        // 先移除旧租约再释放池组件，完成委托可以同步执行，不依赖数组迭代器。
        UNiagaraComponent* Oldest = Entries[0].Get();
        Entries.RemoveAt(0);
        ReleaseEffect(Oldest);
    }
    Entries.Add(Effect);
    const uint64 Lease = ++NextLease;
    Leases.Add(Effect, Lease);
    Effect->OnSystemFinished.AddUniqueDynamic(this, &UKnightEffectsSubsystem::OnEffectFinished);
    return Lease;
}

bool UKnightEffectsSubsystem::OwnsEffect(const UNiagaraComponent* Effect, uint64 Lease) const
{
    const uint64* Current = Leases.Find(Effect);
    return Lease != 0 && Current && *Current == Lease;
}

void UKnightEffectsSubsystem::OnEffectFinished(UNiagaraComponent* Effect)
{
    Leases.Remove(Effect);
    Moves.Remove(Effect);
    Impacts.Remove(Effect);
    if (IsValid(Effect)) Effect->OnSystemFinished.RemoveDynamic(this, &UKnightEffectsSubsystem::OnEffectFinished);
}

void UKnightEffectsSubsystem::ReleaseEffect(UNiagaraComponent* Effect)
{
    if (!IsValid(Effect)) return;
    OnEffectFinished(Effect);
    // AutoRelease 将在 Complete 时回池。取消时直接结束，不能留下等待 User.Landed 的发射器。
    Effect->DeactivateImmediate();
}

void UKnightEffectsSubsystem::Deinitialize()
{
    // 复制列表后清理，避免委托回调改变正在遍历的数组；不在世界销毁期间保留活动租约。
    TArray<TWeakObjectPtr<UNiagaraComponent>> Remaining = Moves;
    Remaining.Append(Impacts);
    for (const auto& Effect : Remaining) ReleaseEffect(Effect.Get());
    Moves.Empty();
    Impacts.Empty();
    Leases.Empty();
    Super::Deinitialize();
}
