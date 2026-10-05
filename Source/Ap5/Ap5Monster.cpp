#include "Ap5Monster.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/Material.h"
#include "HAL/PlatformTime.h"

AAp5Monster::AAp5Monster()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MonsterRoot"));
}

void AAp5Monster::BeginPlay()
{
    Super::BeginPlay();
    // 単位はcm。正面は-X方向。部位は重ねて配置し、まだ融合しない。
    AddPart(TEXT("Torso"), FVector(0, 0, 190), FVector(55, 72, 85));
    AddPart(TEXT("Pelvis"), FVector(0, 0, 117), FVector(45, 54, 35));
    AddPart(TEXT("Head"), FVector(-3, 0, 295), FVector(43, 45, 42));
    AddPart(TEXT("Brow"), FVector(-37, 0, 307), FVector(12, 43, 12));
    AddPart(TEXT("Nose"), FVector(-47, 0, 288), FVector(15, 12, 17));
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        const FString Prefix = Side < 0 ? TEXT("Left") : TEXT("Right");
        AddPart(FName(*(Prefix + TEXT("Shoulder"))), FVector(0, Side * 82, 233), FVector(36, 37, 38));
        AddPart(FName(*(Prefix + TEXT("UpperArm"))), FVector(0, Side * 106, 193), FVector(27, 28, 48), FRotator(0, 0, Side * 18));
        AddPart(FName(*(Prefix + TEXT("Forearm"))), FVector(-6, Side * 122, 130), FVector(30, 31, 43));
        AddPart(FName(*(Prefix + TEXT("Hand"))), FVector(-12, Side * 124, 87), FVector(31, 29, 27));
        AddPart(FName(*(Prefix + TEXT("Thigh"))), FVector(0, Side * 34, 86), FVector(30, 29, 43));
        AddPart(FName(*(Prefix + TEXT("Shin"))), FVector(0, Side * 38, 40), FVector(24, 25, 32));
        AddPart(FName(*(Prefix + TEXT("Foot"))), FVector(-17, Side * 38, 15), FVector(43, 28, 15));
    }
    UE_LOG(LogTemp, Display, TEXT("AP5_MONSTER_READY: parts=%d volume_spacing_cm=5"), Parts.Num());
}

void AAp5Monster::AddPart(FName Name, const FVector& Center,
    const FVector& Radii, const FRotator& Rotation)
{
    UDynamicMeshComponent* Part = NewObject<UDynamicMeshComponent>(this, Name);
    AddInstanceComponent(Part);
    Part->SetupAttachment(RootComponent);
    Part->SetRelativeLocation(Center);
    Part->SetRelativeRotation(Rotation);
    // 加工は体積を直接参照する。物理用の衝突形状は次の段階。
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
    Part->RegisterComponent();
    Parts.Add(Part);
    Ap5Volume::Field Volume;
    Volume.Initialize(Ap5Volume::Point(Radii.X, Radii.Y, Radii.Z));
    Volumes.push_back(std::move(Volume));
    RebuildPart(Parts.Num() - 1);
}

void AAp5Monster::RebuildPart(int32 PartIndex)
{
    const std::vector<Ap5Volume::Triangle> Surface = Volumes[PartIndex].Surface();
    UE::Geometry::FDynamicMesh3 Mesh;
    Mesh.EnableAttributes();
    for (const Ap5Volume::Triangle& Face : Surface)
    {
        int32 Vertices[3];
        int32 Normals[3];
        for (int32 K = 0; K < 3; ++K)
        {
            const Ap5Volume::Point& P = Face.Vertices[K];
            const Ap5Volume::Point& N = Face.Normals[K];
            Vertices[K] = Mesh.AppendVertex(FVector3d(P.X, P.Y, P.Z));
            Normals[K] = Mesh.Attributes()->PrimaryNormals()->AppendElement(
                FVector3f(static_cast<float>(N.X), static_cast<float>(N.Y), static_cast<float>(N.Z)));
        }
        const int32 Triangle = Mesh.AppendTriangle(Vertices[0], Vertices[1], Vertices[2]);
        check(Triangle >= 0);
        Mesh.Attributes()->PrimaryNormals()->SetTriangle(Triangle,
            UE::Geometry::FIndex3i(Normals[0], Normals[1], Normals[2]));
    }
    Parts[PartIndex]->SetMesh(MoveTemp(Mesh));
}

int32 AAp5Monster::ApplyBrush(const FVector& Start, const FVector& Direction, float Radius, bool bRepair)
{
    const double Started = FPlatformTime::Seconds();
    int32 ChangedParts = 0;
    for (int32 I = 0; I < Parts.Num(); ++I)
    {
        const FTransform Transform = Parts[I]->GetComponentTransform();
        const FVector LocalStart = Transform.InverseTransformPosition(Start);
        const FVector LocalDirection = Transform.InverseTransformVectorNoScale(Direction);
        const int32 Changed = Volumes[I].Brush(
            Ap5Volume::Point(LocalStart.X, LocalStart.Y, LocalStart.Z),
            Ap5Volume::Point(LocalDirection.X, LocalDirection.Y, LocalDirection.Z), Radius, bRepair);
        if (Changed > 0)
        {
            RebuildPart(I);
            ++ChangedParts;
        }
    }
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_EDIT: mode=%s radius_cm=%.0f parts=%d cpu_ms=%.2f"),
        bRepair ? TEXT("repair") : TEXT("drill"), Radius, ChangedParts, LastEditMilliseconds);
    return ChangedParts;
}

void AAp5Monster::ResetShape()
{
    for (int32 I = 0; I < Parts.Num(); ++I)
    {
        Volumes[I].Reset();
        RebuildPart(I);
    }
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_RESET"));
}
