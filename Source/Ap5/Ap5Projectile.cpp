#include "Ap5Projectile.h"
#include "Ap5Monster.h"
#include "EngineUtils.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AAp5Projectile::AAp5Projectile()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent=Collision;
    Collision->InitSphereRadius(8.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block);
    Collision->SetNotifyRigidBodyCollision(true);
    Collision->OnComponentHit.AddDynamic(this,&AAp5Projectile::OnProjectileHit);

    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(0.16f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded()) Visual->SetStaticMesh(SphereFinder.Object);

    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->UpdatedComponent=Collision;
    Movement->InitialSpeed=1800.0f;
    Movement->MaxSpeed=1800.0f;
    Movement->ProjectileGravityScale=0.0f;
    Movement->bRotationFollowsVelocity=true;
    Movement->bShouldBounce=false;
    Movement->bForceSubStepping=true;
    Movement->MaxSimulationTimeStep=1.0f/120.0f;
    Movement->MaxSimulationIterations=8;
    InitialLifeSpan=5.0f;
}

void AAp5Projectile::Launch(const FVector& Direction, float InImpactRadius,
    bool bInExplosive, float InExplosionRadius)
{
    FlightDirection=Direction.GetSafeNormal();
    ImpactRadius=FMath::Max(1.0f,InImpactRadius);
    bExplosive=bInExplosive;
    ExplosionRadius=FMath::Max(1.0f,InExplosionRadius);
    PreviousLocation=GetActorLocation();
    Movement->Velocity=FlightDirection*Movement->InitialSpeed;
    UE_LOG(LogTemp,Display,
        TEXT("AP5_PROJECTILE_FIRE: speed_cm_s=1800 impact_radius_cm=%.0f explosive=%d explosion_radius_cm=%.0f"),
        ImpactRadius,bExplosive ? 1 : 0,ExplosionRadius);
}

void AAp5Projectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bResolved) return;
    const FVector Current=GetActorLocation();
    if (!Current.Equals(PreviousLocation,0.01f))
    {
        for (TActorIterator<AAp5Monster> It(GetWorld()); It; ++It)
        {
            if (It->ApplyProjectileSweep(
                PreviousLocation,Current,8.0f,ImpactRadius,bExplosive,ExplosionRadius,40000.0f))
            {
                bResolved=true;
                Destroy();
                return;
            }
        }
    }
    PreviousLocation=Current;
}

void AAp5Projectile::OnProjectileHit(UPrimitiveComponent* /*HitComponent*/, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector /*NormalImpulse*/, const FHitResult& Hit)
{
    if (bResolved) return;
    bResolved=true;
    if (bExplosive)
    {
        for (TActorIterator<AAp5Monster> It(GetWorld()); It; ++It)
            It->ApplyProjectileBlast(Hit.ImpactPoint,FlightDirection,ExplosionRadius);
    }
    else if (AAp5Monster* Monster=Cast<AAp5Monster>(OtherActor))
    {
        Monster->ApplyProjectileHit(OtherComp,Hit.ImpactPoint,FlightDirection,ImpactRadius,40000.0f);
    }
    UE_LOG(LogTemp,Display,TEXT("AP5_PROJECTILE_COLLISION: actor=%s explosive=%d"),
        OtherActor!=nullptr ? *OtherActor->GetName() : TEXT("None"),bExplosive ? 1 : 0);
    Destroy();
}
