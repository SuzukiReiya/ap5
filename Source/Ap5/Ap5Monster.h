#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Pieces.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UPhysicsHandleComponent;
class UPhysicsConstraintComponent;
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
    bool ToggleLegMotionTest();
    bool IsLegMotionTestEnabled() const { return bLegMotionTest; }
    bool ToggleMaterialResistanceTest();
    bool IsMaterialResistanceTestEnabled() const { return bMaterialResistanceTest; }
    const FString& GetLastImpactMaterialText() const { return LastImpactMaterialText; }
    double LastEditMilliseconds = 0;
    int32 LastSeparatedPieces = 0;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UDynamicMeshComponent* CreatePiece();
    UDynamicMeshComponent* CreateVisualChunk(UDynamicMeshComponent* Parent);
    void RebuildMesh(int32 Index);
    void UpdateVisualChunks(int32 Index, const Ap5Volume::IndexedSurface& Surface);
    void DestroyVisualChunks(int32 Index);
    void SyncPhysicsState();
    void RefreshPieces(const std::vector<int>& Changed);
    void UpdateArmMotion(float DeltaSeconds);
    void UpdateLegMotion(float DeltaSeconds);
    bool IsDrivenFootGrounded(int32 DriveId) const;
    void CollapseWalkingBody(int32 FailedSupportDriveId);
    void DestroyCollapseConstraints();
    void CreateCollapseConstraint(int32 ParentIndex,int32 ChildIndex,
        const FVector& JointWorld,const Ap5Volume::Point& ParentAnchor,
        const Ap5Volume::Point& ChildAnchor,float SwingLimitDegrees,float TwistLimitDegrees);
    void ValidateCollapseConstraints();
    double ImpactResistanceAt(const Ap5Volume::Point& LocalHit, FString& MaterialName) const;
    int32 SelectedJoinPiece = INDEX_NONE;
    int32 GrabbedPiece = INDEX_NONE;
    float GrabDistance = 0.0f;
    FVector GrabTarget = FVector::ZeroVector;
    bool bArmMotionTest = false;
    bool bLegMotionTest = false;
    bool bMaterialResistanceTest = false;
    FString LastImpactMaterialText = TEXT("材質差OFF：標準 深さ8.0 cm");
    float ArmMotionTime = 0.0f;
    float LegMotionTime = 0.0f;
    float LegWalkDistance = 0.0f;
    bool bLegWalkBlocked = false;
    int32 ActiveSupportDriveId = INDEX_NONE;
    float LegSupportLossSeconds = 0.0f;
    FTransform LegMotionOriginTransform = FTransform::Identity;
    std::vector<Ap5Volume::Ellipsoid> Shapes;
    std::vector<Ap5Volume::Ellipsoid> BodyShapes;
    std::vector<Ap5Volume::Ellipsoid> LeftThighShapes;
    std::vector<Ap5Volume::Ellipsoid> LeftLowerLegShapes;
    std::vector<Ap5Volume::Ellipsoid> RightThighShapes;
    std::vector<Ap5Volume::Ellipsoid> RightLowerLegShapes;
    Ap5Volume::Field InitialVolume;
    Ap5Volume::PieceCollection Pieces;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GolemMaterial;

    UPROPERTY()
    TObjectPtr<UPhysicsHandleComponent> FragmentHandle;

    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> PieceMeshes;

    UPROPERTY()
    TArray<TObjectPtr<UPhysicsConstraintComponent>> CollapseConstraints;

    struct FCollapseConstraintState
    {
        UPhysicsConstraintComponent* Component = nullptr;
        UDynamicMeshComponent* Parent = nullptr;
        UDynamicMeshComponent* Child = nullptr;
        int32 ParentStructureId = INDEX_NONE;
        int32 ChildStructureId = INDEX_NONE;
        int32 ParentInitialMaterialSamples = 0;
        int32 ChildInitialMaterialSamples = 0;
        Ap5Volume::Point ParentAnchor;
        Ap5Volume::Point ChildAnchor;
    };
    TArray<FCollapseConstraintState> CollapseConstraintStates;

    struct FVisualChunkState
    {
        UDynamicMeshComponent* Component = nullptr;
        uint64 Signature = 0;
    };
    TArray<TMap<FIntVector,FVisualChunkState>> PieceVisualChunks;
};
