#include "Ap5GameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAp5, Log, All);

AAp5GameMode::AAp5GameMode()
{
    DefaultPawnClass = nullptr;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BallFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    FloorMesh = FloorFinder.Object;
    BallMesh = BallFinder.Object;
}

void AAp5GameMode::StartPlay()
{
    Super::StartPlay();
    UWorld* World = GetWorld();
    if (World == nullptr || FloorMesh == nullptr || BallMesh == nullptr)
    {
        UE_LOG(LogAp5, Error, TEXT("Cannot create sample: world or basic shapes missing."));
        return;
    }

    // UE units are centimetres. A thin cube gives the flat floor reliable collision.
    AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
        FVector(0.0, 0.0, -10.0), FRotator::ZeroRotator);
    AStaticMeshActor* Ball = World->SpawnActor<AStaticMeshActor>(
        FVector(0.0, 0.0, 250.0), FRotator::ZeroRotator);
    ACameraActor* Camera = World->SpawnActor<ACameraActor>(
        FVector(700.0, -900.0, 600.0), FRotator::ZeroRotator);
    ADirectionalLight* KeyLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-50.0, -30.0, 0.0));
    ADirectionalLight* FillLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-25.0, 140.0, 0.0));
    if (Floor == nullptr || Ball == nullptr || Camera == nullptr ||
        KeyLight == nullptr || FillLight == nullptr)
    {
        UE_LOG(LogAp5, Error, TEXT("Cannot create sample actors."));
        return;
    }

    UStaticMeshComponent* FloorComponent = Floor->GetStaticMeshComponent();
    FloorComponent->SetMobility(EComponentMobility::Movable);
    FloorComponent->SetStaticMesh(FloorMesh);
    FloorComponent->SetWorldScale3D(FVector(12.0, 12.0, 0.2));
    FloorComponent->SetCollisionProfileName(TEXT("BlockAll"));

    UStaticMeshComponent* BallComponent = Ball->GetStaticMeshComponent();
    BallComponent->SetMobility(EComponentMobility::Movable);
    BallComponent->SetStaticMesh(BallMesh);
    BallComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
    BallComponent->SetSimulatePhysics(true);
    BallComponent->SetEnableGravity(true);
    BallComponent->SetLinearDamping(0.1f);

    UDirectionalLightComponent* KeyComponent = CastChecked<UDirectionalLightComponent>(KeyLight->GetLightComponent());
    KeyComponent->SetMobility(EComponentMobility::Movable);
    KeyComponent->SetIntensity(3.0f);
    UDirectionalLightComponent* FillComponent = CastChecked<UDirectionalLightComponent>(FillLight->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(0.8f);
    FillComponent->SetCastShadows(false);

    Camera->SetActorRotation((FVector(0.0, 0.0, 70.0) - Camera->GetActorLocation()).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(55.0f);
    APlayerController* Controller = World->GetFirstPlayerController();
    if (Controller != nullptr)
    {
        Controller->bAutoManageActiveCameraTarget = false;
        Controller->SetViewTarget(Camera);
    }
    else
    {
        UE_LOG(LogAp5, Error, TEXT("Player controller missing; camera not assigned."));
        return;
    }
    UE_LOG(LogAp5, Display, TEXT("AP5_SAMPLE_READY: floor, physics ball, lights and camera created."));
}
