#include "Ap5GameMode.h"
#include "Ap5Monster.h"
#include "Ap5Observer.h"
#include "Ap5PlayerCharacter.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
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

    // 両面・非ライティングの大きな箱の内側で、空の部分だけを単色にする。
    UMaterialInterface* BackgroundMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Generated/M_Background.M_Background"));
    AStaticMeshActor* Background = World->SpawnActor<AStaticMeshActor>();
    if (Background != nullptr && BackgroundMaterial != nullptr)
    {
        UStaticMeshComponent* BackgroundComponent = Background->GetStaticMeshComponent();
        BackgroundComponent->SetMobility(EComponentMobility::Movable);
        BackgroundComponent->SetStaticMesh(FloorMesh);
        BackgroundComponent->SetWorldScale3D(FVector(200.0));
        BackgroundComponent->SetMaterial(0, BackgroundMaterial);
        BackgroundComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BackgroundComponent->SetCastShadow(false);
    }
    else UE_LOG(LogAp5, Error, TEXT("背景を生成できません。BuildAndRun.ps1で材質生成を確認してください。"));

    // UEの長さ単位はcm。薄い立方体で床の衝突判定を確保する。
    AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
        FVector(0.0, 0.0, -10.0), FRotator::ZeroRotator);
    AAp5Monster* Monster = World->SpawnActor<AAp5Monster>();

    FActorSpawnParameters PlayerSpawn;
    PlayerSpawn.SpawnCollisionHandlingOverride=
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AAp5PlayerCharacter* Player=World->SpawnActor<AAp5PlayerCharacter>(
        FVector(-520.0,-260.0,90.0),FRotator(0.0,27.0,0.0),PlayerSpawn);

    ADirectionalLight* KeyLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-50.0, -30.0, 0.0));
    ADirectionalLight* FillLight = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-25.0, 140.0, 0.0));
    if (Floor == nullptr || Monster == nullptr || Player == nullptr ||
        KeyLight == nullptr || FillLight == nullptr)
    {
        UE_LOG(LogAp5, Error, TEXT("Cannot create sample actors."));
        return;
    }

    UStaticMeshComponent* FloorComponent = Floor->GetStaticMeshComponent();
    FloorComponent->SetMobility(EComponentMobility::Movable);
    FloorComponent->SetStaticMesh(FloorMesh);
    // 12m角ではTPS移動と戦闘検証には狭いため、50m角まで拡張する。
    FloorComponent->SetWorldScale3D(FVector(50.0,50.0,0.2));
    FloorComponent->SetCollisionProfileName(TEXT("BlockAll"));
    // 歩行支持はECC_WorldStaticへの下向きTraceで検出する。
    FloorComponent->SetCollisionObjectType(ECC_WorldStatic);

    // PlayerControllerは既存の破壊・射撃入力を持ち、PawnだけTPS Characterへ切り替える。
    if (APlayerController* Controller=World->GetFirstPlayerController())
    {
        Controller->Possess(Player);
        const FVector AimTarget=Monster->GetActorLocation()+FVector(0.0,0.0,180.0);
        Controller->SetControlRotation(
            (AimTarget-Player->GetActorLocation()).Rotation());
    }
    else
    {
        UE_LOG(LogAp5, Error, TEXT("TPS player controller was not found."));
    }

    UDirectionalLightComponent* KeyComponent = CastChecked<UDirectionalLightComponent>(KeyLight->GetLightComponent());
    KeyComponent->SetMobility(EComponentMobility::Movable);
    KeyComponent->SetIntensity(3.0f);
    // 主光源を明示し、補助光との同順位による警告を防ぐ。
    KeyComponent->SetForwardShadingPriority(1);
    UDirectionalLightComponent* FillComponent = CastChecked<UDirectionalLightComponent>(FillLight->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(0.8f);
    FillComponent->SetCastShadows(false);

    UE_LOG(LogAp5, Display, TEXT("AP5_SAMPLE_READY: TPS player and monster scene created."));
}
