// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessMoveEffectsComponent.h"
#include "Chesses.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstanceController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"
#include "XiangQiPro/UI/XQPMobile.h"

UChessMoveEffectsComponent::UChessMoveEffectsComponent()
{
    // 位移入口直接提交速度和位置变化，32 枚静止棋子无需额外组件 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

bool UChessMoveEffectsComponent::AreChessEffectsEnabled()
{
    // 与画面质量写入同一份本地配置；旧配置缺少此键时沿用开启状态。
    bool bEnabled = true;
    if (GConfig) GConfig->GetBool(TEXT("XQP.Graphics"), TEXT("ChessEffectsEnabled"), bEnabled, GGameUserSettingsIni);
    return bEnabled;
}

void UChessMoveEffectsComponent::SetChessEffectsEnabled(bool bEnabled)
{
    if (!GConfig) return;
    GConfig->SetBool(TEXT("XQP.Graphics"), TEXT("ChessEffectsEnabled"), bEnabled, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    if (!bEnabled)
    {
        // 设置可在对局中修改：立即停发所有本机拖尾，存量粒子仍自然消散。
        // 不取消棋子的移动状态，落点、吃子结算和网络同步继续使用原逻辑。
        for (TObjectIterator<UChessMoveEffectsComponent> It; It; ++It)
            if (It->GetWorld() && It->GetWorld()->IsGameWorld()) It->StopTrail();
    }
}

bool UChessMoveEffectsComponent::UsesMobileEffects() const
{
    // 同时覆盖真实 Android/iOS 和本项目的竖屏移动端预览，复用现有画质界面的平台判断。
    return XQPMobile::IsPortrait(this);
}

int32 UChessMoveEffectsComponent::GetEffectQuality() const
{
    const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    int32 Quality = Settings ? Settings->GetVisualEffectQuality() : 2;
    if (UsesMobileEffects() && GConfig)
    {
        // 移动端单独保存整体档位；不能从经过清晰度优化的 TextureQuality 反推整体画质。
        GConfig->GetInt(TEXT("XQP.Graphics"), TEXT("OverallQuality"), Quality, GGameUserSettingsIni);
    }
    return FMath::Clamp(Quality, 0, 3);
}

void UChessMoveEffectsComponent::ApplyQualityParameters(UNiagaraComponent* Effect) const
{
    if (!IsValid(Effect)) return;
    const int32 Quality = GetEffectQuality();
    const bool bMobile = UsesMobileEffects();
    // 档位只调整新粒子的数量和昂贵层的发射，不缩短已存在粒子的寿命。
    // 移动端使用独立 CPU 资源与较低预算，但保留低档烟气，避免材质层完全不可见。
    static constexpr float DesktopBudgets[] = {0.22f, 0.42f, 0.7f, 1.f};
    static constexpr float MobileBudgets[] = {0.25f, 0.40f, 0.55f, 0.70f};
    Effect->SetVariableInt(TEXT("User.Quality"), Quality);
    Effect->SetVariableInt(TEXT("User.Mobile"), bMobile ? 1 : 0);
    Effect->SetVariableFloat(TEXT("User.Intensity"), bMobile ? MobileBudgets[Quality] : DesktopBudgets[Quality]);
    Effect->SetVariableFloat(TEXT("User.EnableSmoke"), Quality == 0 ? 0.65f : 1.f);
    Effect->SetVariableFloat(TEXT("User.EnableDebris"), Quality >= (bMobile ? 2 : 1) ? 1.f : 0.f);
    Effect->SetVariableFloat(TEXT("User.ImpactScale"), FMath::Clamp(ImpactScale, 0.01f, 4.f));
    // 车的数量预算与亮度分开绑定：各档均保留一份低面数立体战车和基础轮迹，
    // 只减少粒子数量、拉大轨迹采样间距及关闭第二层尘云，避免低档丢失主视觉。
    if (bChariotEffects)
    {
        Effect->SetVariableFloat(TEXT("User.ParticleBudget"), bMobile ? MobileBudgets[Quality] : DesktopBudgets[Quality]);
        Effect->SetVariableFloat(TEXT("User.Intensity"), 1.f);
        Effect->SetVariableFloat(TEXT("User.EnableDebris"), Quality == 0 ? 0.5f : 1.f);
        // 所有车特效入口（移动、吃子与落子）都经过这里，因此使用同一个可编辑土尘色。
        // 不再从 TrailColor、PhantomColor 或被吃方颜色推导，铜质金属层可独立提亮。
        Effect->SetVariableLinearColor(TEXT("User.DustColor"), DustColor);
        // 每枚车独立控制自己的一个虚影；多枚车同时移动时互不抢占。
        Effect->SetVariableFloat(TEXT("User.PhantomOpacity"), FMath::Clamp(PhantomOpacity, 0.f, 1.f));
        Effect->SetVariableLinearColor(TEXT("User.PhantomColor"), PhantomColor);
        Effect->SetVariableFloat(TEXT("User.PhantomScale"), FMath::Clamp(PhantomScale, 0.1f, 1.f));
        Effect->SetVariableFloat(TEXT("User.PhantomLag"), FMath::Clamp(PhantomLagDistance, 0.5f, 10.f));
        Effect->SetVariableFloat(TEXT("User.PhantomRushDistance"), FMath::Clamp(PhantomLagDistance, 0.5f, 10.f)
            + FMath::Clamp(PhantomRushOvershoot, 0.f, 4.f));
        Effect->SetVariableFloat(TEXT("User.PhantomVisibility"), ChariotVisibility);
        // 拖尾组件跟随棋子的 XYZ，战车额外对齐当前网格顶面。分开计算两者的高度，
        // 既让条带随棋子抬升，也让虚影和绑定轮底的效果继续保持在棋子上方。
        const AActor* Owner = GetOwner();
        const AChesses* Chess = Cast<AChesses>(Owner);
        const float SurfaceZ = Chess ? Chess->GetSurfaceZ()
            : (Owner ? Owner->GetActorLocation().Z + 2.6f : Effect->GetComponentLocation().Z + 2.6f);
        const float EffectBaseZ = ActiveTrail.Get() == Effect && Owner
            ? Owner->GetActorLocation().Z + FMath::Clamp(TrailHeightOffset, 0.1f, 5.f) : Effect->GetComponentLocation().Z;
        Effect->SetVariableFloat(TEXT("User.PhantomHeight"), FMath::Max(0.f,
            SurfaceZ + FMath::Clamp(PhantomLiftAbovePiece, 0.f, 5.f) - EffectBaseZ));
    }
}

void UChessMoveEffectsComponent::BeginMove()
{
    // 新移动不复位旧实例：旧拖尾停发、脱离棋子后继续消散，可与本次移动同时存在。
    StopTrail();
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || GetNetMode() == NM_DedicatedServer) return;
    bMoveInProgress = true;
    PreviousLocation = Owner->GetActorLocation();
    MoveStartLocation = PreviousLocation;
    LastChariotUpdateFrame = MAX_uint64;
    ChariotFrameDelta = FVector::ZeroVector;
    ChariotVisibility = 0.95f;
    FireDirection = Owner->GetActorForwardVector();
    if (!AreChessEffectsEnabled()) return;
    UNiagaraSystem* System = UsesMobileEffects() ? MobileMoveTrailEffect : MoveTrailEffect;
    if (!System) return;

    const FVector TrailOffset(0.f, 0.f, bChariotEffects ? FMath::Clamp(TrailHeightOffset, 0.1f, 5.f) : 0.25f);
    UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(),
        NAME_None, TrailOffset, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
        true, false, ENCPoolMethod::AutoRelease);
    if (!Trail) return;
    ActiveTrail = Trail;
    // 附加关系只跟随位置；尺寸始终使用厘米制参数，不能继承棋子网格的缩放。
    Trail->SetAbsolute(false, true, true);
    Trail->SetWorldScale3D(FVector::OneVector);
    Trail->SetWorldLocation(Owner->GetActorLocation() + TrailOffset);
    if (bChariotEffects)
    {
        // 保留完整位置附着，条带、烟尘以及轮部特效共同随棋子抬升，吃子冲击环另在棋盘落点生成。
        Trail->SetTranslucentSortPriority(-1);
    }
    // Niagara 必须在速度采样之后执行，保证本帧 User 参数立即用于发射与局部条带补偿。
    Trail->SetTickBehavior(ENiagaraTickBehavior::UsePrereqs);
    Trail->AddTickPrerequisiteActor(Owner);
    if (AChesses* Chess = Cast<AChesses>(Owner))
        Trail->AddTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
    if (bChariotEffects) ClaimPhantom(Trail);
    ApplyQualityParameters(Trail);
    Trail->SetVariableFloat(TEXT("User.MoveSpeed"), 0.f);
    Trail->SetVariableFloat(TEXT("User.MaxSpeed"), FMath::Max(MaxSpeed, 1.f));
    Trail->SetVariableLinearColor(TEXT("User.TrailColor"), TrailColor);
    Trail->SetVariableInt(TEXT("User.Emit"), 1);
    Trail->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
    Trail->SetVariableVec3(TEXT("User.LocalMoveDelta"), FVector::ZeroVector);
    // 池实例可能刚完成上一段冲撞；新的移动必须清掉局部短冲撞状态。
    if (bChariotEffects)
    {
        Trail->SetVariableInt(TEXT("User.PhantomRush"), 0);
        Trail->SetVariableInt(TEXT("User.WheelRushToken"), 0);
        Trail->SetVariableFloat(TEXT("User.RushStartAge"), 0.f);
    }
    Trail->Activate(true);
}

void UChessMoveEffectsComponent::UpdateMove(float DeltaTime)
{
    AActor* Owner = GetOwner();
    UNiagaraComponent* Trail = ActiveTrail.Get();
    if (!IsValid(Owner) || !IsValid(Trail) || !bMoveInProgress)
    {
        return;
    }
    const FVector Location = Owner->GetActorLocation();
    // 历史 Ribbon 点需要抵消完整 XYZ 位移；吃子时的竖向动画也应留在拖尾轨迹中。
    const FVector Delta = Location - PreviousLocation;
    FVector SampleDelta = Delta;
    if (bChariotEffects)
    {
        if (LastChariotUpdateFrame != GFrameCounter)
        {
            LastChariotUpdateFrame = GFrameCounter;
            ChariotFrameDelta = FVector::ZeroVector;
        }
        else if (Delta.IsNearlyZero())
        {
            // 时间线已经在本帧提交了有效位移，Actor Tick 的重复调用不代表真正静止。
            // 保留本帧速度、发射开关和累计补偿，避免同一帧内先显示、后隐藏的闪烁。
            return;
        }
        ChariotFrameDelta += Delta;
        SampleDelta = ChariotFrameDelta;
    }
    const float Speed = DeltaTime > SMALL_NUMBER
        ? (bChariotEffects ? SampleDelta.Size2D() : SampleDelta.Size()) / DeltaTime : 0.f;
    // 只保存棋盘平面的行进方向；吃子动画最后一段可能纯竖直，不能覆盖之前的喷射朝向。
    const FVector PlanarDelta(Delta.X, Delta.Y, 0.f);
    if (!PlanarDelta.IsNearlyZero()) FireDirection = PlanarDelta.GetSafeNormal();
    if (bChariotEffects && Speed > 0.1f)
    {
        // 一格短走也保持清晰轮廓；速度只在可见基线之上增加重量感，不再把慢走乘到近乎透明。
        const float SpeedRatio = FMath::Clamp(Speed / FMath::Max(MaxSpeed, 1.f), 0.f, 1.f);
        ChariotVisibility = FMath::FInterpTo(ChariotVisibility, 0.92f + 0.08f * FMath::Sqrt(SpeedRatio), DeltaTime, 10.f);
    }
    ApplyQualityParameters(Trail);
    if (bChariotEffects)
        Trail->SetWorldLocation(Location + FVector(0.f, 0.f, FMath::Clamp(TrailHeightOffset, 0.1f, 5.f)));
    // 时间线可能在一次移动中短暂停顿；静止帧立即停发，恢复位移时重新发射。
    Trail->SetVariableInt(TEXT("User.Emit"), Speed > KINDA_SMALL_NUMBER ? 1 : 0);
    Trail->SetVariableFloat(TEXT("User.MoveSpeed"), Speed);
    Trail->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
    // 发射点跟着棋子走，历史采样点则抵消载体平移，形成悬在运动路径上的三维条带。
    Trail->SetVariableVec3(TEXT("User.LocalMoveDelta"), Trail->GetComponentTransform().InverseTransformVectorNoScale(SampleDelta));
    PreviousLocation = Location;
}

void UChessMoveEffectsComponent::StopTrail(bool bContinueWheelEmission)
{
    if (UNiagaraComponent* Trail = ActiveTrail.Get())
    {
        Trail->SetVariableInt(TEXT("User.Emit"), 0);
        Trail->SetVariableFloat(TEXT("User.MoveSpeed"), 0.f);
        Trail->SetVariableVec3(TEXT("User.LocalMoveDelta"), FVector::ZeroVector);
        Trail->RemoveTickPrerequisiteActor(GetOwner());
        if (AChesses* Chess = Cast<AChesses>(GetOwner()))
            Trail->RemoveTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
        // 不调用 DeactivateImmediate/DestroyComponent，也不设置强制回收定时器。
        // Complete 的 inactive response 允许存量粒子走完生命，再由 AutoRelease 安全回池。
        Trail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        const auto Controller = Trail->GetSystemInstanceController();
        UWorld* World = GetWorld();
        if (bContinueWheelEmission && World && Controller.IsValid())
        {
            // 虚影的短冲撞发生在系统内部，载体已脱离棋子。Niagara 用同一系统年龄
            // 计算左右车轮的位置，仅轮迹/擦火在这 0.2 秒继续发射，主烟尘仍由 Emit=0 停发。
            Trail->SetVariableFloat(TEXT("User.RushStartAge"), Controller->GetAge());
            static int32 NextWheelRushToken = 0;
            NextWheelRushToken = NextWheelRushToken == MAX_int32 ? 1 : NextWheelRushToken + 1;
            const int32 Token = NextWheelRushToken;
            Trail->SetVariableInt(TEXT("User.WheelRushToken"), Token);
            TWeakObjectPtr<UNiagaraComponent> WeakTrail(Trail);
            FTimerHandle WheelStopHandle;
            World->GetTimerManager().SetTimer(WheelStopHandle, [WeakTrail, Token]()
            {
                if (UNiagaraComponent* Effect = WeakTrail.Get())
                {
                    // 实例可能因可扩展性裁剪提前回池。唯一租约标记可防止旧定时器
                    // 把另一枚棋子刚复用的池实例停掉；本回调也不延长棋子或组件的寿命。
                    bool bValidToken = false;
                    if (Effect->GetVariableInt(TEXT("User.WheelRushToken"), bValidToken) == Token && bValidToken)
                        Effect->Deactivate();
                }
            }, 0.22f, false);
        }
        else
        {
            Trail->Deactivate();
        }
    }
    ActiveTrail.Reset();
}

void UChessMoveEffectsComponent::ClaimPhantom(UNiagaraComponent* Effect)
{
    // 连续走子或吃子时，旧系统仍可能有土尘未消散；只隐藏旧虚影，避免本枚车叠成两辆。
    // 每个 UChessMoveEffectsComponent 都有自己的引用，多车并行动画互不影响。
    if (UNiagaraComponent* Previous = PhantomInstance.Get())
    {
        Previous->SetVariableFloat(TEXT("User.PhantomOpacity"), 0.f);
        Previous->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnPhantomFinished);
    }
    PhantomInstance = Effect;
    Effect->OnSystemFinished.AddUniqueDynamic(this, &UChessMoveEffectsComponent::OnPhantomFinished);
}

void UChessMoveEffectsComponent::OnPhantomFinished(UNiagaraComponent* FinishedEffect)
{
    FinishedEffect->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnPhantomFinished);
    if (PhantomInstance.Get() == FinishedEffect) PhantomInstance.Reset();
}

void UChessMoveEffectsComponent::SpawnImpact(UNiagaraSystem* System, const AChesses* Victim, bool bContinueTrailPhantom)
{
    if (!AreChessEffectsEnabled() || !System || !IsValid(GetOwner()) || GetNetMode() == NM_DedicatedServer) return;
    const AChesses* Attacker = Cast<AChesses>(GetOwner());
    const FVector HitLocation = (Attacker ? Attacker->GetEffectImpactLocation() : GetOwner()->GetActorLocation())
        + FVector(0.f, 0.f, 0.15f);
    // 世界空间的独立实例不依赖攻击方/被吃方寿命。池只复用已完成实例，不会抢占前一次吃子。
    UNiagaraComponent* Impact = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, HitLocation,
        FRotator::ZeroRotator, FVector::OneVector, true, false, ENCPoolMethod::AutoRelease);
    if (!Impact) return;
    if (bChariotEffects && IsValid(Victim) && !bContinueTrailPhantom) ClaimPhantom(Impact);
    ApplyQualityParameters(Impact);
    if (bContinueTrailPhantom)
    {
        // 持续系统里原本可见的那一辆战车完成最后的短冲撞；吃子系统只负责碎甲和震尘。
        // 不在落点另造一辆从远方冲来的战车，也不把旧虚影先隐藏再重新淡入。
        Impact->SetVariableFloat(TEXT("User.PhantomOpacity"), 0.f);
    }
    Impact->SetVariableVec3(TEXT("User.HitLocation"), HitLocation);
    // 桌面斜视镜头可看到向上爆发的高度；移动端俯视正交镜头则需要棋盘平面上的扇形扩散。
    // 先去掉走子动画的竖直位移，防止移动端到达上方节点时也退化为正对镜头喷射。
    const FVector ImpactDirection = UsesMobileEffects()
        ? FVector(FireDirection.X, FireDirection.Y, 0.f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector)
        : FVector::UpVector;
    Impact->SetVariableVec3(TEXT("User.FireDirection"), ImpactDirection);
    const FLinearColor CaptureColor = IsValid(Victim) && Victim->GetColor() == EChessColor::BLACKCHESS
        ? FLinearColor(0.09f, 0.065f, 0.045f, 1.f) : FLinearColor(0.55f, 0.055f, 0.025f, 1.f);
    Impact->SetVariableLinearColor(TEXT("User.CaptureColor"), CaptureColor);
    if (bChariotEffects)
    {
        const FVector AttackDelta(HitLocation.X - MoveStartLocation.X, HitLocation.Y - MoveStartLocation.Y, 0.f);
        Impact->SetVariableVec3(TEXT("User.Direction"), AttackDelta.GetSafeNormal(SMALL_NUMBER, FireDirection));
        Impact->SetVariableFloat(TEXT("User.AttackDistance"), AttackDelta.Size());
        Impact->SetVariableLinearColor(TEXT("User.TrailColor"), TrailColor);
        Impact->SetTranslucentSortPriority(-1);
    }
    Impact->Activate(true);
}

void UChessMoveEffectsComponent::FinishMove(const AChesses* Victim)
{
    // 状态快照可能重复发送 Arrive，只有开始过且尚未完成的移动才拥有一次触发权。
    if (!bMoveInProgress) return;
    bMoveInProgress = false;
    if (IsValid(GetOwner()))
    {
        const FVector FinalDelta = GetOwner()->GetActorLocation() - PreviousLocation;
        const FVector PlanarDelta(FinalDelta.X, FinalDelta.Y, 0.f);
        if (!PlanarDelta.IsNearlyZero()) FireDirection = PlanarDelta.GetSafeNormal();
    }
    bool bContinueTrailPhantom = false;
    if (bChariotEffects && IsValid(Victim) && AreChessEffectsEnabled())
    {
        if (UNiagaraComponent* Trail = ActiveTrail.Get())
        {
            // 原粒子不重生、不改缩放、不从攻击起点重跑。停发后粒子年龄自然增长，
            // Niagara 用这段年龄驱动约 8 cm 的局部碾压，先完成位移，再在 0.42 秒内淡出。
            // 刷新当前网格顶面高度，避免最后抬高吃子的动画把虚影留在棋子下方。
            ApplyQualityParameters(Trail);
            Trail->SetVariableInt(TEXT("User.PhantomRush"), 1);
            Trail->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
            const FVector OwnerLocation = GetOwner()->GetActorLocation();
            Trail->SetWorldLocation(OwnerLocation + FVector(0.f, 0.f, FMath::Clamp(TrailHeightOffset, 0.1f, 5.f)));
            bContinueTrailPhantom = PhantomInstance.Get() == Trail;
        }
    }
    StopTrail(bContinueTrailPhantom);
    SpawnImpact(IsValid(Victim) ? (UsesMobileEffects() ? MobileCaptureEffect : CaptureEffect) : LandingEffect,
        Victim, bContinueTrailPhantom);
}

void UChessMoveEffectsComponent::CancelMove()
{
    bMoveInProgress = false;
    StopTrail();
}

void UChessMoveEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 阵亡/重开不再发射，但已脱离棋子的世界空间命中和拖尾都由 Niagara 自然完成。
    CancelMove();
    // 棋子组件离场后不再接收完成回调；Niagara 自身仍可自然消散并回池。
    if (UNiagaraComponent* Phantom = PhantomInstance.Get())
        Phantom->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnPhantomFinished);
    PhantomInstance.Reset();
    Super::EndPlay(EndPlayReason);
}
