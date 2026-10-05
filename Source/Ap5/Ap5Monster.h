#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Volume.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

// 全身の体積から外皮・内壁・切断面を生成する検証用モンスター。
UCLASS()
class AP5_API AAp5Monster : public AActor
{
    GENERATED_BODY()

public:
    AAp5Monster();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    int32 ApplyBrush(const FVector& Start, const FVector& Direction, float Radius, bool bRepair);
    int32 Cut(const FVector& PlanePoint, const FVector& PlaneNormal);
    void ResetShape();
    double LastEditMilliseconds = 0;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UDynamicMeshComponent* CreatePiece();
    double RebuildMesh(UDynamicMeshComponent* Component, const Ap5Volume::Field& Volume);
    std::vector<Ap5Volume::Ellipsoid> Shapes;
    Ap5Volume::Field InitialVolume;
    Ap5Volume::Field BodyVolume;
    TArray<Ap5Volume::FallState> Falling;
    bool bHasCut = false;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GolemMaterial;

    UPROPERTY()
    TObjectPtr<UDynamicMeshComponent> BodyMesh;
    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> Fragments;
};
