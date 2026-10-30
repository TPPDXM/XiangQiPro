// Copyright 2026 Ultimate Player All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobileMenuCamera.generated.h"

/** Portrait menu composition, independent of the desktop entrance and mouse camera. */
UCLASS()
class XIANGQIPRO_API AMobileMenuCamera : public AActor
{
    GENERATED_BODY()
public:
    AMobileMenuCamera();
    virtual void Tick(float DeltaSeconds) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Board;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UMenuChessDemoComponent> Demo;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UAudioComponent> Music;
    UPROPERTY() TObjectPtr<class UTextBlock> Caption;
    FVector2D LastViewport = FVector2D::ZeroVector;
    FMargin LastInsets;
    FVector2D LensShift = FVector2D::ZeroVector;
    void FitBoard();
};
