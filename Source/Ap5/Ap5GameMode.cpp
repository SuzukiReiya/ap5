#include "Ap5GameMode.h"
#include "Ap5Monster.h"
#include "Ap5Observer.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAp5, Log, All);

AAp5GameMode::AAp5GameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = AAp5Observer::StaticClass();
    HUDClass = AAp5ObserverHUD::StaticClass();
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    FloorMesh = FloorFinder.Object;
}

void AAp5GameMode::StartPlay()
{
    Super::StartPlay();
    UWorld* World = GetWorld();
    if (World == nullptr || FloorMesh == nullptr)
    {
        UE_LOG(LogAp5, Error, TEXT("Cannot create sample: world or basic shapes missing."));
        return;
    }

    // UEの長さ単位はcm。薄い立方体で床の衝突判定を確保する。
    AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
        FVector(0.0, 0.0, -10.0), FRotator::ZeroRotator);
    AAp5Monster* Monster = World->SpawnActor<AAp5Monster>();
    ADirectionalLight* KeyLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-50.0, -30.0, 0.0));
    ADirectionalLight* FillLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-25.0, 140.0, 0.0));
    if (Floor == nullptr || Monster == nullptr ||
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

    UDirectionalLightComponent* KeyComponent = CastChecked<UDirectionalLightComponent>(KeyLight->GetLightComponent());
    KeyComponent->SetMobility(EComponentMobility::Movable);
    KeyComponent->SetIntensity(3.0f);
    // 主光源を明示し、補助光との同順位による警告を防ぐ。
    KeyComponent->SetForwardShadingPriority(1);
    UDirectionalLightComponent* FillComponent = CastChecked<UDirectionalLightComponent>(FillLight->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(0.8f);
    FillComponent->SetCastShadows(false);

    UE_LOG(LogAp5, Display, TEXT("AP5_SAMPLE_READY: monster observation scene created."));
}
