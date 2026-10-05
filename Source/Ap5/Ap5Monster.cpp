#include "Ap5Monster.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/Material.h"
#include "HAL/PlatformTime.h"
#include "UObject/ConstructorHelpers.h"

AAp5Monster::AAp5Monster()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MonsterRoot"));
    // 白い基本形状用マテリアルで、外皮と穴の内壁の陰影を見やすくする。
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    GolemMaterial = MaterialFinder.Object;
}

void AAp5Monster::BeginPlay()
{
    Super::BeginPlay();
    // 単位はcm。正面は-X方向。19部位の体積を一つの体として合成する。
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
    InitialVolume.InitializeUnion(Shapes);
    ResetShape();
    UE_LOG(LogTemp, Display, TEXT("AP5_MONSTER_READY: shapes=%d volume_spacing_cm=5"), static_cast<int32>(Shapes.size()));
}

void AAp5Monster::AddPart(FName /*Name*/, const FVector& Center,
    const FVector& Radii, const FRotator& Rotation)
{
    Ap5Volume::Ellipsoid Shape;
    Shape.Center = Ap5Volume::Point(Center.X, Center.Y, Center.Z);
    Shape.Radii = Ap5Volume::Point(Radii.X, Radii.Y, Radii.Z);
    const FVector X = Rotation.RotateVector(FVector(1, 0, 0));
    const FVector Y = Rotation.RotateVector(FVector(0, 1, 0));
    const FVector Z = Rotation.RotateVector(FVector(0, 0, 1));
    Shape.AxisX = Ap5Volume::Point(X.X, X.Y, X.Z);
    Shape.AxisY = Ap5Volume::Point(Y.X, Y.Y, Y.Z);
    Shape.AxisZ = Ap5Volume::Point(Z.X, Z.Y, Z.Z);
    Shapes.push_back(Shape);
}

UDynamicMeshComponent* AAp5Monster::CreatePiece()
{
    UDynamicMeshComponent* Piece = NewObject<UDynamicMeshComponent>(this);
    AddInstanceComponent(Piece);
    Piece->SetupAttachment(RootComponent);
    Piece->SetMobility(EComponentMobility::Movable);
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Piece->SetMaterial(0, GolemMaterial != nullptr
        ? GolemMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface));
    Piece->RegisterComponent();
    return Piece;
}

void AAp5Monster::RebuildMesh(int32 Index)
{
    const std::vector<Ap5Volume::Triangle> Surface = Pieces.Items[Index].RebuildSurface();
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
    PieceMeshes[Index]->SetMesh(MoveTemp(Mesh));
    PieceMeshes[Index]->SetRelativeLocation(FVector(0, 0, Pieces.Items[Index].Motion.OffsetZ));
}

void AAp5Monster::RefreshPieces(const std::vector<int>& Changed)
{
    while (PieceMeshes.Num() < static_cast<int32>(Pieces.Items.size())) PieceMeshes.Add(CreatePiece());
    for (int32 Index : Changed) RebuildMesh(Index);
    SetActorTickEnabled(true);
}

int32 AAp5Monster::ApplyBrush(const FVector& Start, const FVector& Direction, float Radius, bool bRepair)
{
    const double Started = FPlatformTime::Seconds();
    const FVector LocalStart = GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(Direction);
    std::vector<int> Changed;
    const int32 Samples = Pieces.Brush(
        Ap5Volume::Point(LocalStart.X, LocalStart.Y, LocalStart.Z),
        Ap5Volume::Point(LocalDirection.X, LocalDirection.Y, LocalDirection.Z), Radius, bRepair, Changed);
    if (!Changed.empty()) RefreshPieces(Changed);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_EDIT: mode=%s radius_cm=%.0f samples=%d pieces=%d cpu_ms=%.2f"),
        bRepair ? TEXT("repair") : TEXT("drill"), Radius, Samples, static_cast<int32>(Changed.size()), LastEditMilliseconds);
    return Samples;
}

int32 AAp5Monster::Cut(const FVector& PlanePoint, const FVector& PlaneNormal)
{
    const double Started = FPlatformTime::Seconds();
    const FVector P = GetActorTransform().InverseTransformPosition(PlanePoint);
    const FVector N = GetActorTransform().InverseTransformVectorNoScale(PlaneNormal);
    std::vector<int> Changed;
    const int32 Added = Pieces.Cut(Ap5Volume::Point(P.X, P.Y, P.Z), Ap5Volume::Point(N.X, N.Y, N.Z), Changed);
    if (Added > 0) RefreshPieces(Changed);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_CUT: added=%d changed=%d total=%d cpu_ms=%.2f"),
        Added, static_cast<int32>(Changed.size()), static_cast<int32>(Pieces.Items.size()), LastEditMilliseconds);
    return Added;
}

void AAp5Monster::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    bool bAnyFalling = false;
    for (size_t I = 0; I < Pieces.Items.size(); ++I)
    {
        Ap5Volume::Piece& Piece = Pieces.Items[I];
        Ap5Volume::FallState& State = Piece.Motion;
        if (Piece.Fixed || State.Landed) continue;
        // 本体と破片の区別ではなく、各塊の固定状態と落下状態だけを見る。
        if (State.Advance(DeltaSeconds, -GetActorLocation().Z))
        {
            UE_LOG(LogTemp, Display, TEXT("AP5_FRAGMENT_LANDED: index=%d"), static_cast<int32>(I));
        }
        else bAnyFalling = true;
        PieceMeshes[static_cast<int32>(I)]->SetRelativeLocation(FVector(0, 0, State.OffsetZ));
    }
    if (!bAnyFalling) SetActorTickEnabled(false);
}

void AAp5Monster::ResetShape()
{
    for (UDynamicMeshComponent* Mesh : PieceMeshes) Mesh->DestroyComponent();
    PieceMeshes.Empty();
    Pieces.Reset(InitialVolume);
    PieceMeshes.Add(CreatePiece());
    RebuildMesh(0);
    SetActorTickEnabled(false);
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_RESET"));
}
