#include "Ap5PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAp5PlayerCharacter::AAp5PlayerCharacter()
{
    PrimaryActorTick.bCanEverTick=false;

    GetCapsuleComponent()->InitCapsuleSize(42.0f,88.0f);
    bUseControllerRotationPitch=false;
    // TPS照準中は移動方向ではなく、常にクロスヘア（Controller Yaw）へ正面を向ける。
    bUseControllerRotationYaw=true;
    bUseControllerRotationRoll=false;

    UCharacterMovementComponent* Movement=GetCharacterMovement();
    Movement->bOrientRotationToMovement=false;
    Movement->RotationRate=FRotator(0.0f,720.0f,0.0f);
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
    // 6頭身寄りのデフォルメ体型として、頭・胴・骨盤・両腕・両脚を
    // 太めの楕円体で構成する。関節部は意図的に少し重ね、隙間を見せない。銃だけは直方体を使う.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    BodyVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorsoVisual"));
    BodyVisual->SetupAttachment(RootComponent);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeLocation(FVector(0.0f,0.0f,10.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.40f,0.32f,0.48f));
    if (SphereFinder.Succeeded()) BodyVisual->SetStaticMesh(SphereFinder.Object);

    PelvisVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PelvisVisual"));
    PelvisVisual->SetupAttachment(RootComponent);
    PelvisVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PelvisVisual->SetRelativeLocation(FVector(0.0f,0.0f,-19.0f));
    PelvisVisual->SetRelativeScale3D(FVector(0.32f,0.27f,0.18f));
    if (SphereFinder.Succeeded()) PelvisVisual->SetStaticMesh(SphereFinder.Object);

    HeadVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(RootComponent);
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetRelativeLocation(FVector(0.0f,0.0f,46.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.27f));
    if (SphereFinder.Succeeded()) HeadVisual->SetStaticMesh(SphereFinder.Object);

    LeftUpperArmVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftUpperArmVisual"));
    LeftUpperArmVisual->SetupAttachment(RootComponent);
    LeftUpperArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftUpperArmVisual->SetRelativeLocation(FVector(0.0f,-22.0f,12.0f));
    LeftUpperArmVisual->SetRelativeScale3D(FVector(0.14f,0.14f,0.30f));
    if (SphereFinder.Succeeded()) LeftUpperArmVisual->SetStaticMesh(SphereFinder.Object);

    LeftForearmVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftForearmVisual"));
    LeftForearmVisual->SetupAttachment(RootComponent);
    LeftForearmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftForearmVisual->SetRelativeLocation(FVector(0.0f,-22.0f,-12.0f));
    LeftForearmVisual->SetRelativeScale3D(FVector(0.13f,0.13f,0.26f));
    if (SphereFinder.Succeeded()) LeftForearmVisual->SetStaticMesh(SphereFinder.Object);

    LeftHandVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandVisual"));
    LeftHandVisual->SetupAttachment(RootComponent);
    LeftHandVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftHandVisual->SetRelativeLocation(FVector(0.0f,-22.0f,-28.0f));
    LeftHandVisual->SetRelativeScale3D(FVector(0.14f));
    if (SphereFinder.Succeeded()) LeftHandVisual->SetStaticMesh(SphereFinder.Object);

    LeftThighVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftThighVisual"));
    LeftThighVisual->SetupAttachment(RootComponent);
    LeftThighVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftThighVisual->SetRelativeLocation(FVector(0.0f,-10.0f,-39.0f));
    LeftThighVisual->SetRelativeScale3D(FVector(0.17f,0.17f,0.31f));
    if (SphereFinder.Succeeded()) LeftThighVisual->SetStaticMesh(SphereFinder.Object);

    LeftShinVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftShinVisual"));
    LeftShinVisual->SetupAttachment(RootComponent);
    LeftShinVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftShinVisual->SetRelativeLocation(FVector(0.0f,-10.0f,-66.0f));
    LeftShinVisual->SetRelativeScale3D(FVector(0.15f,0.15f,0.28f));
    if (SphereFinder.Succeeded()) LeftShinVisual->SetStaticMesh(SphereFinder.Object);

    LeftFootVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFootVisual"));
    LeftFootVisual->SetupAttachment(RootComponent);
    LeftFootVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftFootVisual->SetRelativeLocation(FVector(8.0f,-10.0f,-82.0f));
    LeftFootVisual->SetRelativeScale3D(FVector(0.28f,0.16f,0.10f));
    if (SphereFinder.Succeeded()) LeftFootVisual->SetStaticMesh(SphereFinder.Object);

    RightThighVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightThighVisual"));
    RightThighVisual->SetupAttachment(RootComponent);
    RightThighVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightThighVisual->SetRelativeLocation(FVector(0.0f,10.0f,-39.0f));
    RightThighVisual->SetRelativeScale3D(FVector(0.17f,0.17f,0.31f));
    if (SphereFinder.Succeeded()) RightThighVisual->SetStaticMesh(SphereFinder.Object);

    RightShinVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightShinVisual"));
    RightShinVisual->SetupAttachment(RootComponent);
    RightShinVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightShinVisual->SetRelativeLocation(FVector(0.0f,10.0f,-66.0f));
    RightShinVisual->SetRelativeScale3D(FVector(0.15f,0.15f,0.28f));
    if (SphereFinder.Succeeded()) RightShinVisual->SetStaticMesh(SphereFinder.Object);

    RightFootVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFootVisual"));
    RightFootVisual->SetupAttachment(RootComponent);
    RightFootVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightFootVisual->SetRelativeLocation(FVector(8.0f,10.0f,-82.0f));
    RightFootVisual->SetRelativeScale3D(FVector(0.28f,0.16f,0.10f));
    if (SphereFinder.Succeeded()) RightFootVisual->SetStaticMesh(SphereFinder.Object);

    // 射撃側は肩をPivotにしてPitchだけ追従させる。YawはCharacter全体がControllerへ追従する。
    WeaponPivot=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPivot"));
    WeaponPivot->SetupAttachment(RootComponent);
    WeaponPivot->SetRelativeLocation(FVector(0.0f,20.0f,22.0f));

    RightArmVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightUpperArmVisual"));
    RightArmVisual->SetupAttachment(WeaponPivot);
    RightArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightArmVisual->SetRelativeLocation(FVector(14.0f,0.0f,0.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.28f,0.14f,0.14f));
    if (SphereFinder.Succeeded()) RightArmVisual->SetStaticMesh(SphereFinder.Object);

    RightForearmVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightForearmVisual"));
    RightForearmVisual->SetupAttachment(WeaponPivot);
    RightForearmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightForearmVisual->SetRelativeLocation(FVector(38.0f,0.0f,0.0f));
    RightForearmVisual->SetRelativeScale3D(FVector(0.24f,0.13f,0.13f));
    if (SphereFinder.Succeeded()) RightForearmVisual->SetStaticMesh(SphereFinder.Object);

    RightHandVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandVisual"));
    RightHandVisual->SetupAttachment(WeaponPivot);
    RightHandVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightHandVisual->SetRelativeLocation(FVector(52.0f,0.0f,0.0f));
    RightHandVisual->SetRelativeScale3D(FVector(0.14f));
    if (SphereFinder.Succeeded()) RightHandVisual->SetStaticMesh(SphereFinder.Object);

    GunVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunVisual"));
    GunVisual->SetupAttachment(WeaponPivot);
    GunVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GunVisual->SetRelativeLocation(FVector(68.0f,0.0f,0.0f));
    GunVisual->SetRelativeScale3D(FVector(0.30f,0.065f,0.065f));
    if (CubeFinder.Succeeded()) GunVisual->SetStaticMesh(CubeFinder.Object);
}

FVector AAp5PlayerCharacter::GetProjectileMuzzleLocation() const
{
    if (!IsValid(GunVisual))
        return GetActorLocation()+GetActorForwardVector()*60.0f+FVector(0.0f,0.0f,25.0f);
    // GunVisualは長さ28cmなので、その前端より少し先を射出口にする。
    return GunVisual->GetComponentLocation()+GunVisual->GetForwardVector()*16.0f;
}

void AAp5PlayerCharacter::SetAimPitch(float PitchDegrees)
{
    if (!IsValid(WeaponPivot)) return;
    WeaponPivot->SetRelativeRotation(
        FRotator(FMath::Clamp(PitchDegrees,-65.0f,60.0f),0.0f,0.0f));
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
