// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KnightEffectsSubsystem.generated.h"

class UNiagaraComponent;

/**
 * 马的一次性特效预算与池租约管理。世界而非棋子持有完成回调，因此攻击方阵亡后，
 * 残留仍可自然结束；回池之前删除引用，防止旧棋子误操作已被其他实例复用的组件。
 */
UCLASS()
class XIANGQIPRO_API UKnightEffectsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** 新实例激活前登记；最多保留三个移动系统和六个命中系统，超限回收最早实例。 */
    uint64 RegisterEffect(UNiagaraComponent* Effect, bool bMove);
    /** 使用回池完成回调维护租约，弱引用有效并不代表池组件仍属于原来的走子。 */
    bool OwnsEffect(const UNiagaraComponent* Effect, uint64 Lease) const;
    /** 立即取消一个未完成的腾跃，禁止取消后延迟生成落地粒子。 */
    void ReleaseEffect(UNiagaraComponent* Effect);
    virtual void Deinitialize() override;
private:
    TArray<TWeakObjectPtr<UNiagaraComponent>> Moves;
    TArray<TWeakObjectPtr<UNiagaraComponent>> Impacts;
    TMap<TWeakObjectPtr<UNiagaraComponent>, uint64> Leases;
    uint64 NextLease = 0;
    UFUNCTION()
    void OnEffectFinished(UNiagaraComponent* Effect);
};
