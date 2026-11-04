// Copyright 2026 Ultimate Player All Rights Reserved.

#include "ChessMoveEffectsComponent.h"
#include "Chesses.h"
#include "KnightEffectsSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
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
    PrimaryComponentTick.bStartWithTickEnabled = false;
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
    if (bKnightEffects)
    {
        // 低档减少数量和装饰层，保留完整冷色弧线的亮度；移动版所有档位均为 CPU。
        Effect->SetVariableFloat(TEXT("User.ParticleBudget"), bMobile ? MobileBudgets[Quality] : DesktopBudgets[Quality]);
        Effect->SetVariableFloat(TEXT("User.Intensity"), 1.f);
        Effect->SetVariableLinearColor(TEXT("User.DustColor"), DustColor);
        Effect->SetVariableLinearColor(TEXT("User.TrailColor"), TrailColor);
    }
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
    if (GroundStyle != EChessGroundStyle::None) ApplyGroundParameters(Effect);
}

void UChessMoveEffectsComponent::BeginMove()
{
    // 连续走子中断上一段尚未落地的将帅时，不能在新起点补播旧落印。
    bRoyalLandingPending = false;
    // 新移动不复位旧实例：旧拖尾停发、脱离棋子后继续消散，可与本次移动同时存在。
    StopTrail();
    // 同一枚棋子连续收到新走子时，上一段循环轮声先淡出；不积累循环音源。
    if (UAudioComponent* PreviousAudio = ActiveMoveAudio.Get()) PreviousAudio->FadeOut(0.06f, 0.f);
    ActiveMoveAudio.Reset();
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || GetNetMode() == NM_DedicatedServer) return;
    bMoveInProgress = true;
    bRoyalLandingPending = GroundStyle == EChessGroundStyle::Royal;
    GroundMoveSpeed = 0.f;
    PreviousLocation = Owner->GetActorLocation();
    MoveStartLocation = PreviousLocation;
    LastChariotUpdateFrame = MAX_uint64;
    ChariotFrameDelta = FVector::ZeroVector;
    ChariotVisibility = 0.95f;
    FireDirection = Owner->GetActorForwardVector();
    // 声音使用 SoundWave 自带的全局并发上限；菜单演示、回放及联机客户端也走这一入口。
    // 不受 Niagara 开关影响，专用服务器已在上方排除，不创建音频组件。
    if (MoveSound)
    {
        ActiveMoveAudio = UGameplayStatics::SpawnSoundAttached(MoveSound, Owner->GetRootComponent(),
            NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 0.7f,
            FMath::FRandRange(0.97f, 1.03f));
        // 循环接缝已经在音源中交叉淡化；首次开始时再作短淡入，避免任意起始采样造成点击声。
        if (bChariotEffects && ActiveMoveAudio.IsValid()) ActiveMoveAudio->FadeIn(0.025f, 0.7f);
    }
    if (!AreChessEffectsEnabled()) return;
    if (bKnightEffects)
    {
        BeginKnightTrail();
        BeginKnightMotionTrail();
        return;
    }
    if (GroundStyle != EChessGroundStyle::None)
    {
        EnsureGroundTrail();
        return;
    }
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
    if (bKnightEffects)
    {
        UpdateKnightMotionTrail(DeltaTime);
        // 小马蹄按进度交替采样真实运动与吃子抬升；已落下的步点及整段风痕保持世界空间。
        // 世界级租约校验防止旧实例回池后，原棋子继续改写另一枚马刚复用的组件。
        if (bMoveInProgress && GetWorld())
            if (auto* Budget = GetWorld()->GetSubsystem<UKnightEffectsSubsystem>())
                if (Budget->OwnsEffect(ActiveTrail.Get(), ActiveKnightLease)) UpdateKnightHoofLocation(ActiveTrail.Get());
        return;
    }
    AActor* Owner = GetOwner();
    UNiagaraComponent* Trail = ActiveTrail.Get();
    if (!IsValid(Owner) || !IsValid(Trail) || (!bMoveInProgress && !bRoyalLandingPending))
    {
        return;
    }
    const FVector Location = Owner->GetActorLocation();
    // 历史 Ribbon 点需要抵消完整 XYZ 位移；吃子时的竖向动画也应留在拖尾轨迹中。
    const FVector Delta = Location - PreviousLocation;
    FVector SampleDelta = Delta;
    if (bChariotEffects || GroundStyle != EChessGroundStyle::None)
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
        ? ((bChariotEffects || GroundStyle != EChessGroundStyle::None) ? SampleDelta.Size2D() : SampleDelta.Size()) / DeltaTime : 0.f;
    GroundMoveSpeed = Speed;
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
    if (GroundStyle != EChessGroundStyle::None)
    {
        // 主视觉置于真实网格顶面上方，并跟随完整 XYZ；历史拖尾抵消平移，停留在已走过的位置。
        Trail->SetWorldLocation(Location + FVector(0, 0, GetPieceTrailOffset()));
    }
    // 将帅的一格走子、吃子悬停及纯竖直落下共用同一祥云，不按瞬时平面速度切换纹样。
    // 其他棋子仍按真实位移停发，避免取消或暂停时继续积累路径粒子。
    Trail->SetVariableInt(TEXT("User.Emit"), bRoyalLandingPending || Speed > KINDA_SMALL_NUMBER ? 1 : 0);
    Trail->SetVariableFloat(TEXT("User.MoveSpeed"), Speed);
    Trail->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
    // 发射点跟着棋子走，历史采样点则抵消载体平移，形成悬在运动路径上的三维条带。
    Trail->SetVariableVec3(TEXT("User.LocalMoveDelta"), Trail->GetComponentTransform().InverseTransformVectorNoScale(SampleDelta));

    PreviousLocation = Location;
}

void UChessMoveEffectsComponent::StopTrail(bool bContinueWheelEmission)
{
    if (bKnightEffects)
    {
        StopKnightMotionTrail();
        if (UWorld* World = GetWorld())
            if (auto* Budget = World->GetSubsystem<UKnightEffectsSubsystem>())
                if (Budget->OwnsEffect(ActiveTrail.Get(), ActiveKnightLease))
                {
                    // 取消也要撤销本枚棋子的调度依赖，避免回池后的组件仍等待上一枚马的时间线。
                    ActiveTrail->RemoveTickPrerequisiteActor(GetOwner());
                    if (const AChesses* Chess = Cast<AChesses>(GetOwner()))
                        ActiveTrail->RemoveTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
                    Budget->ReleaseEffect(ActiveTrail.Get());
                }
        ActiveTrail.Reset();
        return;
    }
    if (UNiagaraComponent* Trail = ActiveTrail.Get())
    {
        Trail->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnGroundTrailFinished);
        Trail->SetVariableInt(TEXT("User.Emit"), 0);
        Trail->SetVariableInt(TEXT("User.AuraActive"), 0);
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
        + FVector(0.f, 0.f, GroundStyle != EChessGroundStyle::None ? GetGroundOffset() : 0.15f);
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
    // 上方主视觉与地面落尘分别定位，不能整体抬高 HitLocation，否则落尘也会悬在空中。
    float TargetTop = IsValid(Victim)
        ? FMath::Max(0.f, Victim->GetSurfaceZ() - HitLocation.Z) + 0.65f : 3.2f;
    if (GroundStyle == EChessGroundStyle::Advisor && Attacker)
    {
        // 士在命中瞬间位于被吃棋子的上方，只读受击者顶面会把剑光藏进攻击方网格内。
        // 以两枚棋子较高的真实顶面为基准，再为长 11 cm 的斜剑痕及其宽度留出净空；
        // Niagara 的剑光、闪光与玉屑共用此高度，地面护纹仍使用原 HitLocation。
        const float HighestSurface = FMath::Max(Attacker->GetSurfaceZ(),
            IsValid(Victim) ? Victim->GetSurfaceZ() : Attacker->GetSurfaceZ());
        const float SlashClearance = 5.5f * FMath::Max(1.f, FMath::Clamp(ImpactScale, 0.25f, 2.f));
        TargetTop = FMath::Max(0.f, HighestSurface - HitLocation.Z) + SlashClearance;
    }
    else if (GroundStyle == EChessGroundStyle::Royal && !IsValid(Victim) && Attacker)
    {
        // 将帅落印仍贴近落稳后的本枚棋子顶面，不使用士的剑光净空。
        TargetTop = FMath::Max(0.f, Attacker->GetSurfaceZ() - HitLocation.Z) + 0.7f;
    }
    Impact->SetVariableFloat(TEXT("User.TargetTop"), TargetTop);
    if (GroundStyle != EChessGroundStyle::None)
    {
        // 攻击坐标系始终取起点到目标格的水平朝向。吃子动画可能从目标上方下落，
        // 因此不能用最后一帧速度或相机方向决定左右，否则红兵、黑卒和横吃会反向挥刀。
        const FVector Direction = (HitLocation - MoveStartLocation).GetSafeNormal2D(SMALL_NUMBER,
            FireDirection.GetSafeNormal2D(SMALL_NUMBER, FVector::ForwardVector));
        const FVector AttackRight = FVector::CrossProduct(FVector::UpVector, Direction);
        Impact->SetVariableVec3(TEXT("User.Direction"), Direction);
        Impact->SetVariableVec3(TEXT("User.SlashDirection"), (Direction + FVector::UpVector * 0.55f).GetSafeNormal());
        // 兵的刀与剑气共用右上至左下的斜横切平面；士仍使用自己的斜向剑痕。
        // Niagara 根据 Direction 构造同一右向量，SwingDirection 同时用于火星和碎屑惯性。
        Impact->SetVariableVec3(TEXT("User.SwingDirection"), GroundStyle == EChessGroundStyle::Pawn
            ? (-AttackRight - FVector::UpVector * 0.45f).GetSafeNormal()
            : (Direction - FVector::UpVector * 0.8f).GetSafeNormal());
    }
    if (bKnightEffects)
    {
        const FVector Delta = HitLocation - MoveStartLocation;
        Impact->SetVariableVec3(TEXT("User.HoofDirection"), FVector(Delta.X, Delta.Y, 0.f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector));
        // 命中实例和移动实例共享世界预算及安全回池策略。阵营色只染碎块及少量浮尘。
        if (auto* Budget = GetWorld()->GetSubsystem<UKnightEffectsSubsystem>())
            Budget->RegisterEffect(Impact, false);
    }
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
    GroundMoveSpeed = 0.f;
    PlayArrivalSound(IsValid(Victim));
    if (GroundStyle == EChessGroundStyle::Royal)
    {
        // 普通移动在此已经落稳；吃子在此仅到达对方棋子上方，祥云继续跟随悬停和下落。
        // 吃子冲击保留独立表现，但九宫印只允许由真正落地的 FinishLanding 发出。
        if (IsValid(Victim)) SpawnImpact(UsesMobileEffects() ? MobileCaptureEffect : CaptureEffect, Victim);
        else FinishLanding();
        return;
    }
    if (bKnightEffects)
    {
        // 落地发射器只在真实到达时打开；不使用固定延时推测烽火/网络的到达时间。
        StopKnightMotionTrail();
        // 完成后立即交还参数写入权，已生成弧线和扬尘自然消散并自动回池。
        if (auto* Budget = GetWorld()->GetSubsystem<UKnightEffectsSubsystem>())
            if (Budget->OwnsEffect(ActiveTrail.Get(), ActiveKnightLease))
            {
                // 提交最后一步并交还控制；每个步点独立淡出，不被后续下落拖进棋子内部。
                UpdateKnightHoofLocation(ActiveTrail.Get());
                ActiveTrail->SetVariableFloat(TEXT("User.HoofProgress"), 1.f);
                ActiveTrail->SetVariableInt(TEXT("User.Landed"), 1);
                ActiveTrail->RemoveTickPrerequisiteActor(GetOwner());
                if (const AChesses* Chess = Cast<AChesses>(GetOwner()))
                    ActiveTrail->RemoveTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
            }
        ActiveTrail.Reset();
        if (IsValid(Victim)) SpawnImpact(UsesMobileEffects() ? MobileCaptureEffect : CaptureEffect, Victim);
        return;
    }
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

void UChessMoveEffectsComponent::FinishLanding()
{
    if (GroundStyle != EChessGroundStyle::Royal || !bRoyalLandingPending) return;
    // 先消费落印权，再创建独立短系统；即便同帧重复收到落地/网络通知，也只盖一次印。
    bRoyalLandingPending = false;
    StopTrail();
    SpawnImpact(UsesMobileEffects() ? MobileLandingEffect : LandingEffect, nullptr);
}

void UChessMoveEffectsComponent::CancelMove()
{
    bMoveInProgress = false;
    bRoyalLandingPending = false;
    GroundMoveSpeed = 0.f;
    if (UAudioComponent* Audio = ActiveMoveAudio.Get()) Audio->Stop();
    ActiveMoveAudio.Reset();
    StopTrail();
}

bool UChessMoveEffectsComponent::HasCrossedRiver() const
{
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    if (!Chess) return false;
    // 棋盘 X 为纵向行号。使用真实阵营与当前位置，覆盖红兵、黑卒及烽火/回放。
    return Chess->GetColor() == EChessColor::REDCHESS ? Chess->GetPosition().X >= 5 : Chess->GetPosition().X <= 4;
}

float UChessMoveEffectsComponent::GetGroundOffset() const
{
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    if (!Chess || !Chess->ChessMesh) return 0.12f;
    // 只取网格相对原点的底面偏移，不使用正在吃子悬停的绝对 Z；叠放时仍能对齐棋盘目标格。
    return Chess->ChessMesh->Bounds.Origin.Z - Chess->ChessMesh->Bounds.BoxExtent.Z
        - Chess->GetActorLocation().Z + 0.12f;
}

float UChessMoveEffectsComponent::GetPieceTrailOffset() const
{
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    // 棋子 Actor 原点在中部，不能用固定世界高度猜测顶面；兼容真实腾跃、缩放及吃子叠放。
    return Chess ? Chess->GetSurfaceZ() - Chess->GetActorLocation().Z + 0.7f : 2.f;
}

void UChessMoveEffectsComponent::ApplyGroundParameters(UNiagaraComponent* Effect) const
{
    const int32 Quality = GetEffectQuality();
    Effect->SetVariableFloat(TEXT("User.Intensity"), 1.f);
    Effect->SetVariableFloat(TEXT("User.ParticleBudget"), Quality == 0 || UsesMobileEffects() ? 0.5f : 1.f);
    // 参数保留各棋子的语义，编辑器里可独立修改王金、护卫青与相的朱砂红。
    Effect->SetVariableLinearColor(TEXT("User.RoyalColor"), RoyalColor);
    Effect->SetVariableLinearColor(TEXT("User.AdvisorColor"), AdvisorColor);
    Effect->SetVariableLinearColor(TEXT("User.EarthColor"), EarthColor);
    Effect->SetVariableInt(TEXT("User.CrossedRiver"), HasCrossedRiver() ? 1 : 0);
    // 小兵始终维持较小体量，将帅光柱也避免不受限放大而覆盖整张棋盘。
    Effect->SetVariableFloat(TEXT("User.ImpactScale"), FMath::Clamp(ImpactScale, 0.25f,
        GroundStyle == EChessGroundStyle::Pawn ? 1.25f : 2.f));
}

void UChessMoveEffectsComponent::EnsureGroundTrail()
{
    if (!AreChessEffectsEnabled() || !GetOwner() || GetNetMode() == NM_DedicatedServer) return;
    if (UNiagaraComponent* Existing = ActiveTrail.Get())
    {
        ApplyQualityParameters(Existing);
        // 移动符印跟随棋子的真实顶面，避免光环悬在已经离开的高度。
        if (const AChesses* Chess = Cast<AChesses>(GetOwner()))
            Existing->SetWorldLocation(Chess->GetActorLocation() + FVector(0, 0, GetPieceTrailOffset()));
        return;
    }
    UNiagaraSystem* System = UsesMobileEffects() ? MobileMoveTrailEffect : MoveTrailEffect;
    if (!System) return;
    AActor* Owner = GetOwner();
    UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(),
        NAME_None, FVector(0, 0, GetPieceTrailOffset()), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
        true, false, ENCPoolMethod::AutoRelease);
    if (!Trail) return;
    ActiveTrail = Trail;
    // 保留位置附着，使移动符印跟随棋子的完整 XYZ；旋转、缩放保持世界空间。
    // 局部拖尾用 LocalMoveDelta 抵消载体位移，已经生成的历史点不会被带着滑动。
    Trail->SetAbsolute(false, true, true);
    Trail->SetWorldScale3D(FVector::OneVector);
    Trail->SetWorldLocation(Owner->GetActorLocation() + FVector(0, 0, GetPieceTrailOffset()));
    Trail->OnSystemFinished.AddUniqueDynamic(this, &UChessMoveEffectsComponent::OnGroundTrailFinished);
    ApplyQualityParameters(Trail);
    Trail->SetVariableFloat(TEXT("User.MoveSpeed"), GroundMoveSpeed);
    Trail->SetVariableFloat(TEXT("User.MaxSpeed"), FMath::Max(1.f, MaxSpeed));
    // 将帅从起步首帧就显示祥云，避免第一帧速度为零造成“空白—符印—祥云”的闪换。
    Trail->SetVariableInt(TEXT("User.Emit"), bRoyalLandingPending ? 1 : 0);
    Trail->SetVariableInt(TEXT("User.AuraActive"), 1);
    Trail->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
    Trail->SetVariableVec3(TEXT("User.LocalMoveDelta"), FVector::ZeroVector);
    Trail->SetTickBehavior(ENiagaraTickBehavior::UsePrereqs);
    Trail->AddTickPrerequisiteActor(Owner);
    if (const AChesses* Chess = Cast<AChesses>(Owner)) Trail->AddTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
    Trail->Activate(true);
}

void UChessMoveEffectsComponent::OnGroundTrailFinished(UNiagaraComponent* FinishedEffect)
{
    FinishedEffect->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnGroundTrailFinished);
    if (ActiveTrail.Get() == FinishedEffect) ActiveTrail.Reset();
}

float UChessMoveEffectsComponent::GetKnightJumpHeight(const FVector& Start, const FVector& End) const
{
    // 控制点高于连线的距离是实际中点拱高的两倍。棋子位移与 Niagara 采样用同一定义。
    return FMath::Clamp(FVector::Dist2D(Start, End) * 0.7f, 10.f, 22.f);
}

void UChessMoveEffectsComponent::BeginKnightTrail()
{
    UNiagaraSystem* System = UsesMobileEffects() ? MobileMoveTrailEffect : MoveTrailEffect;
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    if (!System || !Chess || !GetWorld()) return;
    const FVector End = Chess->GetEffectImpactLocation();
    UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, MoveStartLocation,
        FRotator::ZeroRotator, FVector::OneVector, true, false, ENCPoolMethod::AutoRelease);
    if (!Trail) return;
    ActiveTrail = Trail;
    ApplyQualityParameters(Trail);
    Trail->SetVariableVec3(TEXT("User.StartLocation"), MoveStartLocation);
    Trail->SetVariableVec3(TEXT("User.EndLocation"), End);
    Trail->SetVariableFloat(TEXT("User.JumpHeight"), GetKnightJumpHeight(MoveStartLocation, End));
    Trail->SetVariableInt(TEXT("User.Landed"), 0);
    Trail->SetVariableVec3(TEXT("User.HoofDirection"), (End - MoveStartLocation).GetSafeNormal2D(SMALL_NUMBER, FVector::ForwardVector));
    UpdateKnightHoofLocation(Trail);
    // 与真实棋子的时间线/Actor 位移使用同一帧数据；不增加组件 Tick 或固定时长的动画定时器。
    Trail->SetTickBehavior(ENiagaraTickBehavior::UsePrereqs);
    Trail->AddTickPrerequisiteActor(GetOwner());
    Trail->AddTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
    // 默认固定边界用于资产预览；运行时根据真实起终点重算局部包围盒，覆盖整段跨越。
    FBox Bounds(ForceInit);
    Bounds += FVector::ZeroVector;
    Bounds += End - MoveStartLocation;
    Bounds = Bounds.ExpandBy(FVector(16.f, 16.f, 30.f));
    Trail->SetSystemFixedBounds(Bounds);
    if (auto* Budget = GetWorld()->GetSubsystem<UKnightEffectsSubsystem>()) ActiveKnightLease = Budget->RegisterEffect(Trail, true);
    Trail->Activate(true);
}

void UChessMoveEffectsComponent::BeginKnightMotionTrail()
{
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    UNiagaraSystem* System = UsesMobileEffects() ? MobileKnightMotionTrail : KnightMotionTrail;
    if (!Chess || !System) return;
    // 风股与流云跟随马的真实网格顶面，避免沿用固定高度后埋进棋身，看起来像平面贴片。
    // 组件跟随完整 XYZ；历史点在 Niagara 中抵消本帧平移，保留真实腾跃划过的轨迹。
    UNiagaraComponent* Ribbon = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Chess->GetRootComponent(),
        NAME_None, FVector(0, 0, GetPieceTrailOffset()), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
        true, false, ENCPoolMethod::AutoRelease);
    if (!Ribbon) return;
    ActiveKnightRibbon = Ribbon;
    Ribbon->SetAbsolute(false, true, true);
    Ribbon->SetWorldScale3D(FVector::OneVector);
    Ribbon->SetWorldLocation(Chess->GetActorLocation() + FVector(0, 0, GetPieceTrailOffset()));
    Ribbon->SetTickBehavior(ENiagaraTickBehavior::UsePrereqs);
    Ribbon->AddTickPrerequisiteActor(GetOwner());
    Ribbon->AddTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
    Ribbon->OnSystemFinished.AddUniqueDynamic(this, &UChessMoveEffectsComponent::OnKnightRibbonFinished);
    ApplyQualityParameters(Ribbon);
    Ribbon->SetVariableFloat(TEXT("User.MoveSpeed"), 0.f);
    Ribbon->SetVariableFloat(TEXT("User.MaxSpeed"), FMath::Max(1.f, MaxSpeed));
    Ribbon->SetVariableInt(TEXT("User.Emit"), 0);
    Ribbon->SetVariableVec3(TEXT("User.LocalMoveDelta"), FVector::ZeroVector);
    Ribbon->SetVariableVec3(TEXT("User.MoveDirection"), FireDirection);
    Ribbon->Activate(true);
}

void UChessMoveEffectsComponent::UpdateKnightMotionTrail(float DeltaTime)
{
    if (!bMoveInProgress || !GetOwner()) return;
    UNiagaraComponent* Ribbon = ActiveKnightRibbon.Get();
    if (!Ribbon) return;
    const FVector Location = GetOwner()->GetActorLocation();
    const FVector Delta = Location - PreviousLocation;
    if (LastChariotUpdateFrame != GFrameCounter)
    {
        LastChariotUpdateFrame = GFrameCounter;
        ChariotFrameDelta = FVector::ZeroVector;
    }
    else if (Delta.IsNearlyZero()) return;
    // 时间线与 Actor Tick 可能在同帧各提交一次；同帧累加后供 Niagara 一次采样，避免重复补偿。
    ChariotFrameDelta += Delta;
    const float Speed = DeltaTime > SMALL_NUMBER ? ChariotFrameDelta.Size() / DeltaTime : 0.f;
    const FVector Direction = ChariotFrameDelta.GetSafeNormal(SMALL_NUMBER, FireDirection);
    ApplyQualityParameters(Ribbon);
    Ribbon->SetVariableVec3(TEXT("User.LocalMoveDelta"), ChariotFrameDelta);
    Ribbon->SetVariableVec3(TEXT("User.MoveDirection"), Direction);
    Ribbon->SetVariableFloat(TEXT("User.MoveSpeed"), Speed);
    Ribbon->SetVariableInt(TEXT("User.Emit"), Speed > 0.1f ? 1 : 0);
    PreviousLocation = Location;
}

void UChessMoveEffectsComponent::StopKnightMotionTrail()
{
    if (UNiagaraComponent* Ribbon = ActiveKnightRibbon.Get())
    {
        Ribbon->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnKnightRibbonFinished);
        Ribbon->SetVariableInt(TEXT("User.Emit"), 0);
        Ribbon->SetVariableVec3(TEXT("User.LocalMoveDelta"), FVector::ZeroVector);
        Ribbon->RemoveTickPrerequisiteActor(GetOwner());
        if (const AChesses* Chess = Cast<AChesses>(GetOwner())) Ribbon->RemoveTickPrerequisiteComponent(Chess->TimeLine_ChessMove);
        Ribbon->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        Ribbon->Deactivate(); // Complete 保留存量点自然淡出；不重置、不立即清空、不设回收计时器。
    }
    ActiveKnightRibbon.Reset();
}

void UChessMoveEffectsComponent::OnKnightRibbonFinished(UNiagaraComponent* FinishedEffect)
{
    FinishedEffect->OnSystemFinished.RemoveDynamic(this, &UChessMoveEffectsComponent::OnKnightRibbonFinished);
    if (ActiveKnightRibbon.Get() == FinishedEffect) ActiveKnightRibbon.Reset();
}

void UChessMoveEffectsComponent::UpdateKnightHoofLocation(UNiagaraComponent* Effect) const
{
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    if (!IsValid(Effect) || !IsValid(Chess)) return;
    FVector Location = Chess->GetActorLocation();
    // 使用网格实际顶面，兼容普通走子、烽火和连续叠放吃子；步点略高于顶面，避免遮挡。
    // Niagara 用进度门槛依次锁定左右蹄铁的位置，不以固定秒数猜测不同玩法的步速。
    Location.Z = Chess->GetSurfaceZ() + 0.65f;
    Effect->SetVariableVec3(TEXT("User.HoofLocation"), Location);
    const FVector Path = Chess->GetEffectImpactLocation() - MoveStartLocation;
    const FVector Travel = Chess->GetActorLocation() - MoveStartLocation;
    const float DistanceSquared = Path.SizeSquared2D();
    const float Progress = DistanceSquared > SMALL_NUMBER
        ? (Travel.X * Path.X + Travel.Y * Path.Y) / DistanceSquared : 0.f;
    Effect->SetVariableFloat(TEXT("User.HoofProgress"), FMath::Clamp(Progress, 0.f, 1.f));
}

void UChessMoveEffectsComponent::PlayArrivalSound(bool bCapture)
{
    if (!IsValid(GetOwner()) || GetNetMode() == NM_DedicatedServer) return;
    if (bChariotEffects)
    {
        // 车轮声贯穿整段实际移动，落点柔和刹停；炮发射余响和马破风则让其自然播完。
        if (UAudioComponent* Audio = ActiveMoveAudio.Get()) Audio->FadeOut(0.07f, 0.f);
        ActiveMoveAudio.Reset();
    }
    USoundBase* Sound = bCapture ? CaptureSound : LandingSound;
    if (!Sound) return;
    const AChesses* Chess = Cast<AChesses>(GetOwner());
    const FVector Location = Chess ? Chess->GetEffectImpactLocation() : GetOwner()->GetActorLocation();
    // 到达锁存保证每次正式落子只播放一次；吃子用重音替代普通落子声，避免两声叠加。
    UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, bCapture ? 0.85f : 0.65f,
        FMath::FRandRange(0.97f, 1.03f));
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
