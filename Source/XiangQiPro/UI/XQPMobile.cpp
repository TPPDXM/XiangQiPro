#include "XQPMobile.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"

FVector2D XQPMobile::ViewportSize(const UObject* Context)
{
    return Context && Context->GetWorld() ? UWidgetLayoutLibrary::GetViewportSize(Context) : FVector2D::ZeroVector;
}

bool XQPMobile::IsPortrait(const UObject* Context)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return true;
#else
    // Dedicated mobile levels also select their UI before the first viewport size is available.
    if (const UWorld* World = Context ? Context->GetWorld() : nullptr)
        if (World->GetMapName().EndsWith(TEXT("MobileMain")) || World->GetMapName().EndsWith(TEXT("MobileBattle"))) return true;
    const FVector2D Size = ViewportSize(Context);
    return Size.X > 0.f && Size.Y > Size.X;
#endif
}

FMargin XQPMobile::SafeInsets(const UObject* Context)
{
    FVector4 Padding(0, 0, 0, 0), Spill(0, 0, 0, 0);
    FVector2D Scale = FVector2D::UnitVector;
    UWidgetBlueprintLibrary::GetSafeZonePadding(Context, Padding, Scale, Spill);
    return FMargin(Padding.X, Padding.Y, Padding.Z, Padding.W);
}

FBox2D XQPMobile::BoardRect(const UObject* Context)
{
    const FVector2D Size = ViewportSize(Context);
    const FMargin Safe = SafeInsets(Context);
    const float Width = FMath::Max(1.f, Size.X - Safe.Left - Safe.Right);
    const float Height = FMath::Max(1.f, Size.Y - Safe.Top - Safe.Bottom);
    const float DPI = UWidgetLayoutLibrary::GetViewportScale(Context);
    // Keep this clearance in sync with the compact portrait HUD assets.
    const auto* GameInstance = Context && Context->GetWorld() ? Context->GetWorld()->GetGameInstance<UXQPGameInstance>() : nullptr;
    const bool ProBattle = GameInstance && (GameInstance->GetGameMode() == EXQPGameMode::ProBattleAI || GameInstance->GetGameMode() == EXQPGameMode::ProBattleLAN);
    const float Top = FMath::Min((ProBattle ? 142.f : 100.f) * DPI, Height * .25f);
    const float Bottom = Height - FMath::Min((ProBattle ? 212.f : 106.f) * DPI, Height * .35f);
    const float Side = FMath::Min(8.f * DPI, Width * .025f);
    return FBox2D(FVector2D(Safe.Left + Side, Safe.Top + Top),
        FVector2D(Size.X - Safe.Right - Side, Safe.Top + FMath::Max(Top + 1.f, Bottom)));
}

UClass* XQPMobile::ResolveWidgetClass(const UObject* Context, UClass* WidgetClass)
{
    if (!WidgetClass || !IsPortrait(Context) || !WidgetClass->GetPathName().StartsWith(TEXT("/Game/UMG/")) ||
        WidgetClass->GetPathName().StartsWith(TEXT("/Game/UMG/Mobile/"))) return WidgetClass;
    FString Name = WidgetClass->GetName();
    Name.RemoveFromEnd(TEXT("_C"));
    const FString Path = FString::Printf(TEXT("/Game/UMG/Mobile/%s_Mobile.%s_Mobile_C"), *Name, *Name);
    if (UClass* MobileClass = LoadClass<UUserWidget>(nullptr, *Path)) return MobileClass;
    UE_LOG(LogTemp, Warning, TEXT("Mobile UI asset is missing: %s"), *Path);
    return WidgetClass;
}
