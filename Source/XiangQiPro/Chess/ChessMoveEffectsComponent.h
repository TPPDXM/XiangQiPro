// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChessMoveEffectsComponent.generated.h"

class AChesses;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UAudioComponent;

/** 九宫与步兵的地面动作风格；None 保留已有车、炮、马的独立表现。 */
UENUM(BlueprintType)
enum class EChessGroundStyle : uint8
{
    None,
    Royal,
    Advisor,
    Elephant,
    Pawn
};

/**
 * 所有棋子共用的走子声画入口：移动拖尾按真实位移采样，马的蹄印与起落尘独立触发。
 * 将帅只在移动时显示古铜符印；真实到达时按棋子配置播放落子或吃子，存量粒子自然完成。
 * 本组件不决定走法和吃子是否合法；仅由棋子已确认的动画节点调用，避免预览走法也播放特效。
 * 资源默认全部为空，派生棋子或蓝图按需赋值。音频独立于特效质量，重复到达通知由移动锁存过滤。
 */
UCLASS(ClassGroup=(Chess), meta=(BlueprintSpawnableComponent))
class XIANGQIPRO_API UChessMoveEffectsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UChessMoveEffectsComponent();

    /** 棋子风格配置。四类主视觉均位于棋子上方，持续拖尾按真实位移采样。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Ground")
    EChessGroundStyle GroundStyle = EChessGroundStyle::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Ground")
    FLinearColor RoyalColor = FLinearColor(0.28f, 0.125f, 0.045f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Ground")
    FLinearColor AdvisorColor = FLinearColor(0.275f, 0.687f, 0.745f, 1.f);

    /** 相的山形镇守印与流云使用朱砂红；数值为线性颜色，避免直接写入 sRGB 造成过亮偏橙。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Ground", meta=(DisplayName="相特效朱砂色"))
    FLinearColor EarthColor = FLinearColor(0.64f, 0.055f, 0.025f, 1.f);

    /** 本机画面偏好，桌面和移动端共用；未保存过时默认开启，不影响网络走子结算。 */
    static bool AreChessEffectsEnabled();
    static void SetChessEffectsEnabled(bool bEnabled);

    /** PC 的命中系统；为空时吃子仍正常结算，只是不生成此层视觉表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> CaptureEffect = nullptr;

    /** PC 的移动系统：持续拖尾或一次性采样完整路径；前一次残留可自然结束。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> MoveTrailEffect = nullptr;

    /** 移动端独立 CPU 资源。未配置时不回退到可能含 GPU 发射器的 PC 资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Mobile")
    TObjectPtr<UNiagaraSystem> MobileCaptureEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Mobile")
    TObjectPtr<UNiagaraSystem> MobileMoveTrailEffect = nullptr;

    /** 普通落子层：炮使用余烬闪点，车使用缩小地面震尘；将帅使用真正落稳后的一次性九宫印。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    TObjectPtr<UNiagaraSystem> LandingEffect = nullptr;

    /** 将帅落印的独立移动端资源，不回退到桌面 Niagara 系统。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Mobile")
    TObjectPtr<UNiagaraSystem> MobileLandingEffect = nullptr;

    /** 走子音源：车使用循环木轮声，其他棋子使用单次起音；独立于画面特效开关和质量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Audio")
    TObjectPtr<USoundBase> MoveSound = nullptr;
    /** 普通落子声；在真实到达时触发，吃子时改播 CaptureSound，避免两层重音叠加。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Audio")
    TObjectPtr<USoundBase> LandingSound = nullptr;
    /** 吃子命中声；脱离攻击方寿命播放，允许炮爆炸等余响自然结束。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Audio")
    TObjectPtr<USoundBase> CaptureSound = nullptr;

    /** 马的蹄印/起落尘采用独立世界空间系统；蓝色拖尾使用单独的持续系统。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Knight")
    bool bKnightEffects = false;

    /** 马的蓝色拖尾独立于一次性的蹄印/起落尘；从真实运动位置持续采样，不预画完整弧线。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Knight")
    TObjectPtr<UNiagaraSystem> KnightMotionTrail = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects|Knight")
    TObjectPtr<UNiagaraSystem> MobileKnightMotionTrail = nullptr;

    /** 二次贝塞尔控制点向上偏移量；取距离的 0.7 倍并限制范围，和真实棋子腾跃共用。 */
    float GetKnightJumpHeight(const FVector& Start, const FVector& End) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects", meta=(ClampMin="0.01", ClampMax="4.0"))
    float ImpactScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects", meta=(ClampMin="1.0"))
    float MaxSpeed = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects")
    FLinearColor TrailColor = FLinearColor(1.f, 0.34f, 0.035f, 1.f);

    /**
     * 车与马的独立烟尘色：控制各自的移动扬尘、命中尘云、尘环与落子震尘。
     * 与战车的 PhantomColor、轮迹/火花的金属色分开，防止提亮铜色时把土尘染成橙铜色。
     * RGB 默认灰土褐，A 是烟尘全局透明度倍率；各层仍使用自身的寿命淡出曲线。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chess|Effects", meta=(DisplayName="烟尘颜色"))
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

    /**
     * 棋子真正落回棋盘时通知。将帅吃子时 FinishMove 是上方命中节点，不能在那里盖落印；
     * 本通知只消费当前走子的单次落印权，重复快照、静止 Tick 和取消后的通知均不会重放。
     */
    void FinishLanding();

    /** 中断移动或棋子阵亡时仅停发拖尾，不伪造命中，也不截断已生成的粒子。 */
    void CancelMove();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 当前移动实例；将帅仅用于移动符印。停发后释放引用，由 Niagara 自行完成并回池。 */
    TWeakObjectPtr<UNiagaraComponent> ActiveTrail;
    /** 本枚车当前可见虚影所属实例，可为已脱离棋子的拖尾或独立吃子实例。 */
    TWeakObjectPtr<UNiagaraComponent> PhantomInstance;
    /** 移动声只在取消/死亡时停止；命中声是脱离棋子寿命的短促单次声。 */
    TWeakObjectPtr<UAudioComponent> ActiveMoveAudio;
    /** 池组件对象仍有效也可能已被复用；序号必须与世界子系统当前租约同时匹配才可写参数。 */
    uint64 ActiveKnightLease = 0;
    /** 持续风带独立持有、自然回池；完成回调防止旧棋子改写复用后的组件。 */
    TWeakObjectPtr<UNiagaraComponent> ActiveKnightRibbon;
    void BeginKnightMotionTrail();
    void UpdateKnightMotionTrail(float DeltaTime);
    void StopKnightMotionTrail();
    UFUNCTION()
    void OnKnightRibbonFinished(UNiagaraComponent* FinishedEffect);
    FVector PreviousLocation = FVector::ZeroVector;
    /** 开始移动时保存攻击起点，供吃子方向与攻击距离使用；不再把拖尾锁在这个 Z 上。 */
    FVector MoveStartLocation = FVector::ZeroVector;
    FVector FireDirection = FVector::ForwardVector;
    bool bMoveInProgress = false;
    /** 将帅本段走子的落印权；吃子命中后仍保留到真实落地，取消或下一段移动会撤销。 */
    bool bRoyalLandingPending = false;
    /** 一帧内时间线和 Actor Tick 可能先后提交位移，零位移的重复采样不能把虚影关掉。 */
    uint64 LastChariotUpdateFrame = MAX_uint64;
    FVector ChariotFrameDelta = FVector::ZeroVector;
    /** 只平滑可见强度，不平滑实际位移，保证距离采样 Ribbon 在抬升和落下时仍保持轨迹连续。 */
    float ChariotVisibility = 0.95f;

    float GroundMoveSpeed = 0.f;

    /** 创建或复用本枚棋子的持续系统；全部参数先写入，再激活池组件。 */
    void EnsureGroundTrail();
    void ApplyGroundParameters(UNiagaraComponent* Effect) const;
    bool HasCrossedRiver() const;
    /** Actor 原点位于棋子中心，地面层需减去网格底面相对原点的高度，再留少量防穿插余量。 */
    float GetGroundOffset() const;
    /** 主拖尾/护印跟随网格顶面，地面环与主视觉分层放置。 */
    float GetPieceTrailOffset() const;

    /** 回池时同步清除活动引用，防止下一段走子修改已经租给另一枚棋子的组件。 */
    UFUNCTION()
    void OnGroundTrailFinished(UNiagaraComponent* FinishedEffect);

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

    /** 世界空间马特效由世界级预算管理器持有；本组件只在移动期间写入落地信号。 */
    void BeginKnightTrail();
    /** 将真实棋子顶面和走子进度交给交替步点采样；已生成的小马蹄保持在世界空间并自然淡出。 */
    void UpdateKnightHoofLocation(UNiagaraComponent* Effect) const;
    void PlayArrivalSound(bool bCapture);
};
