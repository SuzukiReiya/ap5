#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Pieces.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UPhysicsHandleComponent;
class UPrimitiveComponent;

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
    int32 ApplyImpact(const FVector& Start, const FVector& Direction, float Radius);
    int32 ApplyProjectileHit(UPrimitiveComponent* HitComponent, const FVector& HitPoint,
        const FVector& Direction, float Radius, float ImpulseStrength);
    int32 ApplyProjectileBlast(const FVector& HitPoint, const FVector& Direction, float Radius);
    bool ApplyProjectileSweep(const FVector& Start, const FVector& End, float SweepRadius,
        float ImpactRadius, bool bExplosive, float ExplosionRadius, float ImpulseStrength);
    int32 ApplyBlast(const FVector& Start, const FVector& Direction, float Radius);
    int32 Cut(const FVector& PlanePoint, const FVector& PlaneNormal);
    void ResetShape();
    FString ApplyJoin(const FVector& Start, const FVector& Direction);
    void ClearJoinSelection() { SelectedJoinPiece = INDEX_NONE; }
    bool GetJoinSelectionLocation(FVector& Location) const;
    FString BeginGrab(const FVector& Start, const FVector& Direction);
    void UpdateGrab(const FVector& Start, const FVector& Direction);
    void EndGrab();
    bool GetGrabLocation(FVector& Location) const;
    bool ToggleArmMotionTest();
    bool IsArmMotionTestEnabled() const { return bArmMotionTest; }
    bool ToggleMaterialResistanceTest();
    bool IsMaterialResistanceTestEnabled() const { return bMaterialResistanceTest; }
    const FString& GetLastImpactMaterialText() const { return LastImpactMaterialText; }
    double LastEditMilliseconds = 0;
    int32 LastSeparatedPieces = 0;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UDynamicMeshComponent* CreatePiece();
    void RebuildMesh(int32 Index);
    void SyncPhysicsState();
    void RefreshPieces(const std::vector<int>& Changed);
    void UpdateArmMotion(float DeltaSeconds);
    void RecordProjectileSweepProfile(double Started, bool bBroadphaseRejected);
    double ImpactResistanceAt(const Ap5Volume::Point& LocalHit, FString& MaterialName) const;
    int32 SelectedJoinPiece = INDEX_NONE;
    int32 GrabbedPiece = INDEX_NONE;
    float GrabDistance = 0.0f;
    FVector GrabTarget = FVector::ZeroVector;
    bool bArmMotionTest = false;
    bool bMaterialResistanceTest = false;
    FString LastImpactMaterialText = TEXT("材質差OFF：標準 深さ8.0 cm");
    float ArmMotionTime = 0.0f;
    double SweepProfileTotalMilliseconds = 0.0;
    double SweepProfileMaxMilliseconds = 0.0;
    double SweepProfileLastLogSeconds = 0.0;
    int32 SweepProfileCalls = 0;
    int32 SweepProfileBroadphaseRejects = 0;
    std::vector<Ap5Volume::Ellipsoid> Shapes;
    Ap5Volume::Field InitialVolume;
    Ap5Volume::PieceCollection Pieces;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GolemMaterial;

    UPROPERTY()
    TObjectPtr<UPhysicsHandleComponent> FragmentHandle;

    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> PieceMeshes;
};
