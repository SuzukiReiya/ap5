#include "Ap5Monster.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/Material.h"

namespace
{
    // 極の頂点を共有し、継ぎ目にも重複頂点を作らない閉じた楕円体。
    constexpr int32 LongitudeSteps = 24;
    constexpr int32 LatitudeSteps = 12;

    int32 AppendPoint(UE::Geometry::FDynamicMesh3& Mesh,
        const FVector& Unit, const FVector& Radii)
    {
        const int32 Vertex = Mesh.AppendVertex(FVector3d(Unit * Radii));
        const FVector Normal = FVector(Unit.X / Radii.X, Unit.Y / Radii.Y,
            Unit.Z / Radii.Z).GetSafeNormal();
        Mesh.Attributes()->PrimaryNormals()->AppendElement(FVector3f(Normal));
        return Vertex;
    }

    void AppendFace(UE::Geometry::FDynamicMesh3& Mesh, int32 A, int32 B, int32 C)
    {
        const int32 Triangle = Mesh.AppendTriangle(A, B, C);
        check(Triangle >= 0);
        Mesh.Attributes()->PrimaryNormals()->SetTriangle(
            Triangle, UE::Geometry::FIndex3i(A, B, C));
    }
}

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
    UE_LOG(LogTemp, Display, TEXT("AP5_MONSTER_READY: parts=%d triangles_per_part=528"), Parts.Num());
}

void AAp5Monster::AddPart(FName Name, const FVector& Center,
    const FVector& Radii, const FRotator& Rotation)
{
    UE::Geometry::FDynamicMesh3 Mesh;
    Mesh.EnableAttributes();
    const int32 North = AppendPoint(Mesh, FVector(0, 0, 1), Radii);
    for (int32 Latitude = 1; Latitude < LatitudeSteps; ++Latitude)
    {
        const double Theta = PI * Latitude / LatitudeSteps;
        for (int32 Longitude = 0; Longitude < LongitudeSteps; ++Longitude)
        {
            const double Phi = 2.0 * PI * Longitude / LongitudeSteps;
            AppendPoint(Mesh, FVector(FMath::Sin(Theta) * FMath::Cos(Phi),
                FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta)), Radii);
        }
    }
    const int32 South = AppendPoint(Mesh, FVector(0, 0, -1), Radii);
    for (int32 Longitude = 0; Longitude < LongitudeSteps; ++Longitude)
    {
        const int32 Next = (Longitude + 1) % LongitudeSteps;
        AppendFace(Mesh, North, 1 + Longitude, 1 + Next);
        for (int32 Ring = 0; Ring < LatitudeSteps - 2; ++Ring)
        {
            const int32 A = 1 + Ring * LongitudeSteps + Longitude;
            const int32 B = A + LongitudeSteps;
            const int32 C = 1 + (Ring + 1) * LongitudeSteps + Next;
            const int32 D = 1 + Ring * LongitudeSteps + Next;
            AppendFace(Mesh, A, B, C);
            AppendFace(Mesh, A, C, D);
        }
        const int32 Last = 1 + (LatitudeSteps - 2) * LongitudeSteps;
        AppendFace(Mesh, Last + Longitude, South, Last + Next);
    }
    UDynamicMeshComponent* Part = NewObject<UDynamicMeshComponent>(this, Name);
    AddInstanceComponent(Part);
    Part->SetupAttachment(RootComponent);
    Part->SetRelativeLocation(Center);
    Part->SetRelativeRotation(Rotation);
    // 今回は観察のみ。破壊後の当たり判定・物理は次の検証で追加する。
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
    Part->SetMesh(MoveTemp(Mesh));
    Part->RegisterComponent();
    Parts.Add(Part);
}
