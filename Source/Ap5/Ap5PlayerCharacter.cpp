#include "Ap5PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAp5PlayerCharacter::AAp5PlayerCharacter()
{
    PrimaryActorTick.bCanEverTick=false;

    GetCapsuleComponent()->InitCapsuleSize(42.0f,88.0f);
    bUseControllerRotationPitch=false;
    bUseControllerRotationYaw=false;
    bUseControllerRotationRoll=false;

    UCharacterMovementComponent* Movement=GetCharacterMovement();
    Movement->bOrientRotationToMovement=true;
    Movement->RotationRate=FRotator(0.0f,540.0f,0.0f);
    Movement->MaxWalkSpeed=WalkSpeed;
    Movement->JumpZVelocity=470.0f;
    Movement->AirControl=0.35f;

    CameraBoom=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetRelativeLocation(FVector(0.0f,0.0f,58.0f));
    CameraBoom->TargetArmLength=DefaultCameraDistance;
    CameraBoom->SocketOffset=FVector(0.0f,65.0f,0.0f);
    CameraBoom->bUsePawnControlRotation=true;
    CameraBoom->bDoCollisionTest=true;
    CameraBoom->ProbeSize=12.0f;
    CameraBoom->ProbeChannel=ECC_Camera;

    FollowCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom,USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation=false;
    FollowCamera->SetFieldOfView(60.0f);

    // Skeletal Mesh導入前の検証用表示。衝突はCharacterのCapsuleだけで受ける。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    BodyVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(RootComponent);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeLocation(FVector(0.0f,0.0f,0.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.36f,0.28f,0.72f));
    if (CubeFinder.Succeeded()) BodyVisual->SetStaticMesh(CubeFinder.Object);

    HeadVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(RootComponent);
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetRelativeLocation(FVector(0.0f,0.0f,62.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.24f));
    if (SphereFinder.Succeeded()) HeadVisual->SetStaticMesh(SphereFinder.Object);
}

void AAp5PlayerCharacter::SetSprintEnabled(bool bEnabled)
{
    GetCharacterMovement()->MaxWalkSpeed=bEnabled ? SprintSpeed : WalkSpeed;
}

void AAp5PlayerCharacter::AdjustCameraDistance(float DeltaCm)
{
    CameraBoom->TargetArmLength=FMath::Clamp(
        CameraBoom->TargetArmLength+DeltaCm,220.0f,450.0f);
}

void AAp5PlayerCharacter::ResetCameraDistance()
{
    CameraBoom->TargetArmLength=DefaultCameraDistance;
}
