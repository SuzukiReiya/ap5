#include "Ap5Projectile.h"
#include "Ap5Monster.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AAp5Projectile::AAp5Projectile()
{
    PrimaryActorTick.bCanEverTick=false;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent=Collision;
    Collision->InitSphereRadius(4.0f);
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
    Visual->SetRelativeScale3D(FVector(0.08f));
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
    InitialLifeSpan=5.0f;
}

void AAp5Projectile::Launch(const FVector& Direction, float InImpactRadius)
{
    FlightDirection=Direction.GetSafeNormal();
    ImpactRadius=FMath::Max(1.0f,InImpactRadius);
    Movement->Velocity=FlightDirection*Movement->InitialSpeed;
    UE_LOG(LogTemp,Display,TEXT("AP5_PROJECTILE_FIRE: speed_cm_s=1800 radius_cm=%.0f"),
        ImpactRadius);
}

void AAp5Projectile::OnProjectileHit(UPrimitiveComponent* /*HitComponent*/, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector /*NormalImpulse*/, const FHitResult& Hit)
{
    if (AAp5Monster* Monster=Cast<AAp5Monster>(OtherActor))
    {
        Monster->ApplyProjectileHit(OtherComp,Hit.ImpactPoint,FlightDirection,ImpactRadius,40000.0f);
    }
    UE_LOG(LogTemp,Display,TEXT("AP5_PROJECTILE_COLLISION: actor=%s"),
        OtherActor!=nullptr ? *OtherActor->GetName() : TEXT("None"));
    Destroy();
}
