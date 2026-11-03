// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChessMoveEffectsComponent.generated.h"

class AChesses;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 所有棋子共用的两段式视觉表现：移动过程中发射拖尾，到达目标上方时播放独立命中实例。
 * 本组件不决定走法和吃子是否合法；仅由棋子已确认的动画节点调用，避免预览走法也播放特效。
 * 资源默认全部为空，派生棋子或蓝图按需赋值。移动、吃子和落子闪点可以分别启用。
 */
UCLASS(ClassGroup=(Chess), meta=(BlueprintSpawnableComponent))
class XIANGQIPRO_API UChessMoveEffectsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UChessMoveEffectsComponent();

    /** 本机画面偏好，桌面和移动端共用；未保存过时默认开启，不影响网络走子结算。 */
    static bool AreChessEffectsEnabled();
    static void SetChessEffectsEnabled(bool bEnabled);

    /** PC 的命中系统；为空时吃子仍正常结算，只是不生成此层视觉表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> CaptureEffect = nullptr;

    /** PC 的持续拖尾系统；每次移动使用新实例，上一段拖尾可以自然结束。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> MoveTrailEffect = nullptr;

    /** 移动端独立 CPU 资源。未配置时不回退到可能含 GPU 发射器的 PC 资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Mobile")
    TObjectPtr<UNiagaraSystem> MobileCaptureEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Mobile")
    TObjectPtr<UNiagaraSystem> MobileMoveTrailEffect = nullptr;

    /** 普通落子层：炮使用余烬闪点，车使用缩小地面震尘；吃子使用独立 CaptureEffect。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> LandingEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects", meta=(ClampMin="0.01", ClampMax="4.0"))
    float ImpactScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects", meta=(ClampMin="1.0"))
    float MaxSpeed = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    FLinearColor TrailColor = FLinearColor(1.f, 0.34f, 0.035f, 1.f);

    /**
     * 车的独立烟尘色：同时控制移动尘带、扬尘、吃子尘云、尘浪和落子震尘。
     * 与战车的 PhantomColor、轮迹/火花的金属色分开，防止提亮铜色时把土尘染成橙铜色。
     * RGB 默认灰土褐，A 是烟尘全局透明度倍率；各层仍使用自身的寿命淡出曲线。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(DisplayName="烟尘颜色"))
    FLinearColor DustColor = FLinearColor(0.38f, 0.30f, 0.20f, 1.f);

    /** 车使用随棋子移动的拖尾与悬浮战车；炮仍沿用原来的火药喷射表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot")
    bool bChariotEffects = false;

    /** 移动与冲撞共用的亮铜主色，独立于被吃棋子的阵营色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot")
    FLinearColor PhantomColor = FLinearColor(0.9f, 0.52f, 0.18f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PhantomOpacity = 1.f;

    /** 战车独立缩放，不改变尘带和冲击环；在上一版 0.32 的基础上放大 1.5 倍。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.1", ClampMax="1.0"))
    float PhantomScale = 0.48f;

    /** 战车底部高于棋子网格顶面；拖尾使用独立的跟随高度，二者不能互相代替。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.0", ClampMax="5.0"))
    float PhantomLiftAbovePiece = 0.7f;

    /** 拖尾发射点随棋子 XYZ 移动，放在棋子腰部，避免条带和烟尘铺在棋盘上。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.1", ClampMax="5.0"))
    float TrailHeightOffset = 1.25f;

    /** 虚影中心落在棋子后缘附近；与整段攻击距离无关，避免残像拖在数格之外。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.5", ClampMax="10.0"))
    float PhantomLagDistance = 4.2f;

    /** 落点后只多碾过一小段；冲撞总路程为滞后距离加此值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Chariot", meta=(ClampMin="0.0", ClampMax="4.0"))
    float PhantomRushOvershoot = 4.f;

    /** 开始一段真实移动，并清除上一段的到达锁存；网络重复到达通知不会重复爆发。 */
    void BeginMove();

    /** 由棋子的实际位移入口在更新位置后调用，使用同一帧位移采样速度，不增加额外组件 Tick。 */
    void UpdateMove(float DeltaTime);

    /** 在首次到达节点调用。Victim 为空时只播放落子闪点，非空时读取被吃棋子的阵营色。 */
    void FinishMove(const AChesses* Victim = nullptr);

    /** 中断移动或棋子阵亡时仅停发拖尾，不伪造命中，也不截断已生成的粒子。 */
    void CancelMove();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 当前仍在发射的拖尾。停止后立即释放本组件引用，由 Niagara 自行完成并回池。 */
    TWeakObjectPtr<UNiagaraComponent> ActiveTrail;
    /** 本枚车当前可见虚影所属实例，可为已脱离棋子的拖尾或独立吃子实例。 */
    TWeakObjectPtr<UNiagaraComponent> PhantomInstance;
    FVector PreviousLocation = FVector::ZeroVector;
    /** 开始移动时保存攻击起点，供吃子方向与攻击距离使用；不再把拖尾锁在这个 Z 上。 */
    FVector MoveStartLocation = FVector::ZeroVector;
    FVector FireDirection = FVector::ForwardVector;
    bool bMoveInProgress = false;
    /** 一帧内时间线和 Actor Tick 可能先后提交位移，零位移的重复采样不能把虚影关掉。 */
    uint64 LastChariotUpdateFrame = MAX_uint64;
    FVector ChariotFrameDelta = FVector::ZeroVector;
    /** 只平滑可见强度，不平滑实际位移，保证距离采样 Ribbon 在抬升和落下时仍保持轨迹连续。 */
    float ChariotVisibility = 0.95f;

    /** 与现有 PC“特效质量”及移动端“画面质量”保持同一来源。 */
    int32 GetEffectQuality() const;
    bool UsesMobileEffects() const;
    void ApplyQualityParameters(UNiagaraComponent* Effect) const;
    /** 短冲撞时只延续轮迹/擦火约 0.2 秒，主条带和烟尘仍立即停止发射。 */
    void StopTrail(bool bContinueWheelEmission = false);
    void SpawnImpact(UNiagaraSystem* System, const AChesses* Victim, bool bContinueTrailPhantom = false);

    /** 只替换本棋子的残像，保留其他车的虚影与所有系统的尘土层。 */
    void ClaimPhantom(UNiagaraComponent* Effect);

    /** 回池前解除引用，防止池实例被另一枚棋子复用后仍被本棋子改写参数。 */
    UFUNCTION()
    void OnPhantomFinished(UNiagaraComponent* FinishedEffect);
};
