#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Projectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS()
class AP5_API AAp5Projectile : public AActor
{
    GENERATED_BODY()

public:
    AAp5Projectile();
    virtual void Tick(float DeltaSeconds) override;
    void Launch(const FVector& Direction, float InImpactRadius, bool bInExplosive, float InExplosionRadius);

private:
    UFUNCTION()
    void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY()
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Visual;

    UPROPERTY()
    TObjectPtr<UProjectileMovementComponent> Movement;

    float ImpactRadius = 20.0f;
    float ExplosionRadius = 30.0f;
    bool bExplosive = false;
    bool bResolved = false;
    FVector FlightDirection = FVector::ForwardVector;
    FVector PreviousLocation = FVector::ZeroVector;
};
