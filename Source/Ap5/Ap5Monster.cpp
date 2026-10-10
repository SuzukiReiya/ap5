#include "Ap5Monster.h"
#include "Ap5FrameProfiler.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/Material.h"
#include "HAL/PlatformTime.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
Ap5Volume::Point VolumePoint(const FVector& P) { return Ap5Volume::Point(P.X,P.Y,P.Z); }
FVector EnginePoint(const Ap5Volume::Point& P) { return FVector(P.X,P.Y,P.Z); }
}

AAp5Monster::AAp5Monster()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MonsterRoot"));
    FragmentHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("FragmentHandle"));
    FragmentHandle->SetLinearStiffness(1800.0f);
    FragmentHandle->SetLinearDamping(220.0f);
    FragmentHandle->SetInterpolationSpeed(18.0f);
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
    FAp5ProfileScope Profile(EAp5ProfileProcess::ComponentCreate);
    UDynamicMeshComponent* Piece = NewObject<UDynamicMeshComponent>(this);
    AddInstanceComponent(Piece);
    Piece->SetupAttachment(RootComponent);
    Piece->SetMobility(EComponentMobility::Movable);
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 連射時に単純衝突の再Cookでゲームスレッドを止めない。
    Piece->bUseAsyncCooking = true;
    Piece->SetComplexAsSimpleCollisionEnabled(false, false);
    Piece->CollisionType = CTF_UseSimpleAsComplex;
    Piece->SetDeferredCollisionUpdatesEnabled(true, false);
    Piece->SetLinearDamping(0.15f);
    Piece->SetAngularDamping(0.5f);
    Piece->SetMaterial(0, GolemMaterial != nullptr
        ? GolemMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface));
    Piece->RegisterComponent();
    return Piece;
}

void AAp5Monster::RebuildMesh(int32 Index)
{
    Ap5Volume::Piece& State = Pieces.Items[Index];
    UDynamicMeshComponent* Component = PieceMeshes[Index];
    Component->SetSimulatePhysics(false);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    std::vector<Ap5Volume::Triangle> Surface;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::SurfaceBuild);
        Surface = State.RebuildSurface();
    }

    UE::Geometry::FDynamicMesh3 Mesh;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::DynamicMeshBuild);
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
    }

    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::SetMesh);
        Component->SetMesh(MoveTemp(Mesh));
    }

    const FQuat Rotation = FRotationMatrix::MakeFromXY(EnginePoint(State.AxisX), EnginePoint(State.AxisY)).ToQuat();
    const FTransform LocalTransform(Rotation, EnginePoint(State.ToWorld(Ap5Volume::Point())));
    Component->SetWorldTransform(LocalTransform * GetActorTransform(), false, nullptr, ETeleportType::TeleportPhysics);

    FKAggregateGeom Collision;
    std::vector<Ap5Volume::CollisionBox> Boxes;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::CollisionBoxes);
        Boxes = State.Volume.CollisionBoxes();
        for (const Ap5Volume::CollisionBox& Box : Boxes)
        {
            FKBoxElem Element;
            Element.Center = EnginePoint(Box.Center);
            Element.X = static_cast<float>(Box.Size.X);
            Element.Y = static_cast<float>(Box.Size.Y);
            Element.Z = static_cast<float>(Box.Size.Z);
            Collision.BoxElems.Add(Element);
        }
    }

    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::CollisionUpdate);
        Component->SetSimpleCollisionShapes(Collision, false);
        Component->UpdateCollision(false);
    }

    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::PhysicsBodySetup);
        if (!Boxes.empty())
        {
            Component->SetCollisionObjectType(State.Fixed
                ? (State.Driven ? ECC_WorldDynamic : ECC_WorldStatic) : ECC_PhysicsBody);
            Component->SetCollisionResponseToAllChannels(ECR_Block);
            Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Component->SetUseCCD(true, NAME_None);
            Component->SetSimulatePhysics(!State.Fixed);
            if (!State.Fixed)
            {
                const Ap5Volume::Point Center = VolumePoint(
                    GetActorTransform().InverseTransformPosition(Component->GetCenterOfMass()));
                Component->SetPhysicsLinearVelocity(
                    GetActorTransform().TransformVectorNoScale(EnginePoint(State.VelocityAt(Center))));
                Component->SetPhysicsAngularVelocityInRadians(
                    GetActorTransform().TransformVectorNoScale(EnginePoint(State.AngularVelocity)));
                Component->WakeAllRigidBodies();
            }
        }
    }
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
    SyncPhysicsState();
    const FVector LocalStart = GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(Direction);
    const int32 Before = static_cast<int32>(Pieces.Items.size());
    std::vector<int> Changed;
    int32 Samples=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Samples=Pieces.Brush(
            Ap5Volume::Point(LocalStart.X, LocalStart.Y, LocalStart.Z),
            Ap5Volume::Point(LocalDirection.X, LocalDirection.Y, LocalDirection.Z), Radius, bRepair, Changed);
    }
    LastSeparatedPieces = static_cast<int32>(Pieces.Items.size()) - Before;
    if (!Changed.empty()) RefreshPieces(Changed);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_EDIT: mode=%s radius_cm=%.0f samples=%d pieces=%d separated=%d cpu_ms=%.2f"),
        bRepair ? TEXT("repair") : TEXT("drill"), Radius, Samples, static_cast<int32>(Changed.size()), LastSeparatedPieces, LastEditMilliseconds);
    return Samples;
}

double AAp5Monster::ImpactResistanceAt(const Ap5Volume::Point& LocalHit, FString& MaterialName) const
{
    if (!bMaterialResistanceTest)
    {
        MaterialName=TEXT("標準");
        return 1.0;
    }
    // 検証用の空間材質マップ。破片化・回転後も元の局所座標で同じ材質を維持する。
    if (LocalHit.Z>=250.0)
    {
        MaterialName=TEXT("頭部硬質");
        return 4.0;
    }
    if (LocalHit.Z>=145.0 && LocalHit.Z<250.0 && FMath::Abs(LocalHit.Y)<=60.0)
    {
        MaterialName=TEXT("胴体硬質");
        return 2.0;
    }
    MaterialName=TEXT("標準");
    return 1.0;
}

bool AAp5Monster::ToggleMaterialResistanceTest()
{
    bMaterialResistanceTest=!bMaterialResistanceTest;
    LastImpactMaterialText=bMaterialResistanceTest
        ? TEXT("材質差ON：腕・脚=標準 / 胴体=x2 / 頭=x4")
        : TEXT("材質差OFF：全身標準 深さ8.0 cm");
    UE_LOG(LogTemp, Display, TEXT("AP5_MATERIAL_TEST: enabled=%d"),bMaterialResistanceTest ? 1 : 0);
    return bMaterialResistanceTest;
}

int32 AAp5Monster::ApplyImpact(const FVector& Start, const FVector& Direction, float Radius)
{
    const double Started = FPlatformTime::Seconds();
    SyncPhysicsState();
    const FVector LocalStart = GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
    if (LocalDirection.IsNearlyZero()) return 0;

    double Resistance=1.0;
    double Depth=8.0;
    FString MaterialName=TEXT("標準");
    double HitDistance=0;
    const int32 Target=Pieces.Pick(VolumePoint(LocalStart),VolumePoint(LocalDirection),HitDistance);
    if (Target>=0)
    {
        const Ap5Volume::Point Hit=Pieces.Items[Target].ToLocal(
            VolumePoint(LocalStart)+VolumePoint(LocalDirection)*HitDistance);
        Resistance=ImpactResistanceAt(Hit,MaterialName);
        Depth=8.0/Resistance;
    }
    LastImpactMaterialText=FString::Printf(TEXT("%s x%.1f / 深さ %.1f cm"),
        *MaterialName,Resistance,Depth);

    const int32 Before = static_cast<int32>(Pieces.Items.size());
    std::vector<int> Changed;
    int32 Samples=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Samples=Pieces.Impact(
            VolumePoint(LocalStart),VolumePoint(LocalDirection),Radius,Depth,Changed);
    }
    LastSeparatedPieces = static_cast<int32>(Pieces.Items.size()) - Before;
    if (!Changed.empty()) RefreshPieces(Changed);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display,
        TEXT("AP5_IMPACT: radius_cm=%.0f depth_cm=%.1f resistance=%.1f material=%s samples=%d pieces=%d separated=%d cpu_ms=%.2f"),
        Radius,Depth,Resistance,*MaterialName,Samples,static_cast<int32>(Changed.size()),
        LastSeparatedPieces,LastEditMilliseconds);
    return Samples;
}

int32 AAp5Monster::ApplyProjectileHit(UPrimitiveComponent* HitComponent, const FVector& HitPoint,
    const FVector& Direction, float Radius, float ImpulseStrength)
{
    const double Started=FPlatformTime::Seconds();
    if (HitComponent==nullptr || Direction.IsNearlyZero()) return 0;
    SyncPhysicsState();

    int32 Target=INDEX_NONE;
    for (int32 I=0;I<PieceMeshes.Num();++I)
    {
        if (PieceMeshes[I]==HitComponent) { Target=I; break; }
    }
    if (Target==INDEX_NONE || Target>=static_cast<int32>(Pieces.Items.size())) return 0;

    const FVector LocalHit=GetActorTransform().InverseTransformPosition(HitPoint);
    const FVector LocalDirection=GetActorTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
    FString MaterialName;
    const Ap5Volume::Point PieceHit=Pieces.Items[Target].ToLocal(VolumePoint(LocalHit));
    const double Resistance=ImpactResistanceAt(PieceHit,MaterialName);
    const double Depth=8.0/Resistance;
    LastImpactMaterialText=FString::Printf(TEXT("%s x%.1f / 深さ %.1f cm"),
        *MaterialName,Resistance,Depth);

    const int32 Before=static_cast<int32>(Pieces.Items.size());
    std::vector<int> Changed;
    int32 Samples=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Samples=Pieces.ImpactAt(Target,VolumePoint(LocalHit),VolumePoint(LocalDirection),
            Radius,Depth,Changed);
    }
    LastSeparatedPieces=static_cast<int32>(Pieces.Items.size())-Before;
    if (!Changed.empty()) RefreshPieces(Changed);

    // 加工で分離した場合も、命中点の直後にある自由破片へ運動量を渡す。
    double Nearest=0;
    const Ap5Volume::Point ProbeStart=VolumePoint(LocalHit)-VolumePoint(LocalDirection)*10.0;
    const int32 ImpulseTarget=Pieces.Pick(ProbeStart,VolumePoint(LocalDirection),Nearest);
    if (ImpulseTarget>=0 && PieceMeshes.IsValidIndex(ImpulseTarget)
        && PieceMeshes[ImpulseTarget]->IsSimulatingPhysics())
    {
        PieceMeshes[ImpulseTarget]->AddImpulseAtLocation(
            Direction.GetSafeNormal()*ImpulseStrength,HitPoint,NAME_None);
    }

    LastEditMilliseconds=(FPlatformTime::Seconds()-Started)*1000;
    return Samples;
}

int32 AAp5Monster::ApplyProjectileBlast(const FVector& HitPoint, const FVector& Direction, float Radius)
{
    const double Started=FPlatformTime::Seconds();
    if (Radius<=0 || Direction.IsNearlyZero()) return 0;
    SyncPhysicsState();
    const FVector LocalHit=GetActorTransform().InverseTransformPosition(HitPoint);
    const FVector LocalDirection=GetActorTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
    const FVector Center=LocalHit+LocalDirection*Radius*0.25f;
    const int32 Before=static_cast<int32>(Pieces.Items.size());
    std::vector<int> Changed;
    int32 Samples=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Samples=Pieces.Blast(VolumePoint(Center),Radius,550.0,Changed);
    }
    LastSeparatedPieces=static_cast<int32>(Pieces.Items.size())-Before;
    if (!Changed.empty()) RefreshPieces(Changed);
    LastEditMilliseconds=(FPlatformTime::Seconds()-Started)*1000;
    return Samples;
}

bool AAp5Monster::ApplyProjectileSweep(const FVector& Start, const FVector& End, float SweepRadius,
    float ImpactRadius, bool bExplosive, float ExplosionRadius, float ImpulseStrength)
{
    if ((End-Start).IsNearlyZero()) return false;

    bool bNearAnyPiece=false;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::ProjectileSweep);
        FBox SegmentBounds(ForceInit);
        SegmentBounds+=Start;
        SegmentBounds+=End;
        SegmentBounds=SegmentBounds.ExpandBy(SweepRadius);
        for (UDynamicMeshComponent* Component : PieceMeshes)
        {
            if (IsValid(Component) && SegmentBounds.Intersect(Component->Bounds.GetBox()))
            {
                bNearAnyPiece=true;
                break;
            }
        }
    }
    if (!bNearAnyPiece) return false;

    SyncPhysicsState();

    int32 Target=INDEX_NONE;
    Ap5Volume::Point Hit;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::ProjectileSweep);
        const FVector LocalStart=GetActorTransform().InverseTransformPosition(Start);
        const FVector LocalEnd=GetActorTransform().InverseTransformPosition(End);
        double Nearest=0;
        Target=Pieces.PickSwept(
            VolumePoint(LocalStart),VolumePoint(LocalEnd),SweepRadius,Nearest,Hit);
    }
    if (Target<0 || !PieceMeshes.IsValidIndex(Target)) return false;

    const FVector WorldHit=GetActorTransform().TransformPosition(EnginePoint(Hit));
    const FVector Direction=(End-Start).GetSafeNormal();
    if (bExplosive)
        ApplyProjectileBlast(WorldHit,Direction,ExplosionRadius);
    else
        ApplyProjectileHit(PieceMeshes[Target],WorldHit,Direction,ImpactRadius,ImpulseStrength);
    return true;
}

int32 AAp5Monster::ApplyBlast(const FVector& Start, const FVector& Direction, float Radius)
{
    const double Started=FPlatformTime::Seconds();
    SyncPhysicsState();
    const FVector LocalStart=GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalDirection=GetActorTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
    if (LocalDirection.IsNearlyZero() || Radius<=0) return 0;
    double HitDistance=0;
    const int32 Target=Pieces.Pick(VolumePoint(LocalStart),VolumePoint(LocalDirection),HitDistance);
    if (Target<0) return 0;
    const FVector Center=LocalStart+LocalDirection*static_cast<float>(HitDistance+Radius*0.35f);
    const int32 Before=static_cast<int32>(Pieces.Items.size());
    std::vector<int> Changed;
    int32 Samples=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Samples=Pieces.Blast(VolumePoint(Center),Radius,550.0,Changed);
    }
    LastSeparatedPieces=static_cast<int32>(Pieces.Items.size())-Before;
    if (!Changed.empty()) RefreshPieces(Changed);
    LastEditMilliseconds=(FPlatformTime::Seconds()-Started)*1000;
    UE_LOG(LogTemp, Display,
        TEXT("AP5_BLAST: radius_cm=%.0f samples=%d changed=%d separated=%d total=%d speed_cm_s=550 cpu_ms=%.2f"),
        Radius,Samples,static_cast<int32>(Changed.size()),LastSeparatedPieces,
        static_cast<int32>(Pieces.Items.size()),LastEditMilliseconds);
    return Samples;
}

int32 AAp5Monster::Cut(const FVector& PlanePoint, const FVector& PlaneNormal)
{
    const double Started = FPlatformTime::Seconds();
    SyncPhysicsState();
    const FVector P = GetActorTransform().InverseTransformPosition(PlanePoint);
    const FVector N = GetActorTransform().InverseTransformVectorNoScale(PlaneNormal);
    std::vector<int> Changed;
    int32 Added=0;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Added=Pieces.Cut(Ap5Volume::Point(P.X, P.Y, P.Z), Ap5Volume::Point(N.X, N.Y, N.Z), Changed);
    }
    if (Added > 0) RefreshPieces(Changed);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_CUT: added=%d changed=%d total=%d cpu_ms=%.2f"),
        Added, static_cast<int32>(Changed.size()), static_cast<int32>(Pieces.Items.size()), LastEditMilliseconds);
    return Added;
}

FString AAp5Monster::BeginGrab(const FVector& Start, const FVector& Direction)
{
    EndGrab();
    SyncPhysicsState();
    const FVector Axis=Direction.GetSafeNormal();
    if (Axis.IsNearlyZero()) return TEXT("把持できません：視線方向が無効です。");
    const FVector LocalStart=GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalDirection=GetActorTransform().InverseTransformVectorNoScale(Axis);
    double Distance=0;
    const int32 Target=Pieces.Pick(VolumePoint(LocalStart),VolumePoint(LocalDirection),Distance);
    if (Target<0) return TEXT("把持対象なし：分離して落ちた破片をクリックしてください。");
    if (!PieceMeshes.IsValidIndex(Target) || Pieces.Items[Target].Fixed
        || !PieceMeshes[Target]->IsSimulatingPhysics())
        return TEXT("固定されている部分は把持できません。切り離した破片を選んでください。");

    GrabbedPiece=Target;
    GrabDistance=static_cast<float>(Distance);
    GrabTarget=Start+Axis*GrabDistance;
    FragmentHandle->GrabComponentAtLocation(PieceMeshes[Target],NAME_None,GrabTarget);
    PieceMeshes[Target]->WakeAllRigidBodies();
    UE_LOG(LogTemp, Display, TEXT("AP5_GRAB_BEGIN: index=%d distance_cm=%.1f"),Target,GrabDistance);
    return TEXT("破片を把持しました。左ボタンを押したまま動かし、勢いを付けて離すと投げられます。");
}

void AAp5Monster::UpdateGrab(const FVector& Start, const FVector& Direction)
{
    if (GrabbedPiece==INDEX_NONE || FragmentHandle==nullptr) return;
    const FVector Axis=Direction.GetSafeNormal();
    if (Axis.IsNearlyZero()) return;
    GrabTarget=Start+Axis*GrabDistance;
    FragmentHandle->SetTargetLocation(GrabTarget);
}

void AAp5Monster::EndGrab()
{
    if (GrabbedPiece==INDEX_NONE) return;
    const int32 Released=GrabbedPiece;
    if (FragmentHandle!=nullptr) FragmentHandle->ReleaseComponent();
    GrabbedPiece=INDEX_NONE;
    GrabDistance=0.0f;
    UE_LOG(LogTemp, Display, TEXT("AP5_GRAB_END: index=%d"),Released);
}

bool AAp5Monster::GetGrabLocation(FVector& Location) const
{
    if (GrabbedPiece==INDEX_NONE) return false;
    Location=GrabTarget;
    return true;
}

bool AAp5Monster::GetJoinSelectionLocation(FVector& Location) const
{
    if (!PieceMeshes.IsValidIndex(SelectedJoinPiece)) return false;
    Location = PieceMeshes[SelectedJoinPiece]->Bounds.Origin;
    return true;
}

FString AAp5Monster::ApplyJoin(const FVector& Start, const FVector& Direction)
{
    SyncPhysicsState();
    double Distance = 0;
    const int32 Target = Pieces.Pick(
        VolumePoint(GetActorTransform().InverseTransformPosition(Start)),
        VolumePoint(GetActorTransform().InverseTransformVectorNoScale(Direction)), Distance);
    if (Target < 0) return TEXT("対象なし。破片または接合先をクリックしてください。");
    if (SelectedJoinPiece == INDEX_NONE)
    {
        if (Pieces.Items[Target].Fixed) return TEXT("まず、分離して落ちた破片を選んでください。");
        SelectedJoinPiece = Target;
        return TEXT("破片を選択しました。次に接合先をクリック。同じ破片または8で選択解除。");
    }
    if (Target == SelectedJoinPiece)
    {
        ClearJoinSelection();
        return TEXT("選択を解除しました。戻す破片を選んでください。");
    }
    const double Started = FPlatformTime::Seconds();
    const int32 Source = SelectedJoinPiece;
    int32 Result=INDEX_NONE;
    {
        FAp5ProfileScope Profile(EAp5ProfileProcess::VolumeEdit);
        Result=Pieces.Join(Source, Target);
    }
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    if (Result < 0)
    {
        UE_LOG(LogTemp, Display, TEXT("AP5_JOIN_REJECTED: source=%d target=%d reason=%d"), Source, Target, Result);
        if (Result == -2) return TEXT("共通の分離元がないため接合できません。別の接合先を選んでください。");
        if (Result == -3) return TEXT("元の位置でもつながりません。間の破片を先に戻すか、別の接合先を選んでください。");
        return TEXT("接合対象が無効です。8で選択を解除してください。");
    }
    // データの番号とコンポーネントの番号をそろえ、吸収した破片の物理ボディを除去する。
    PieceMeshes[Source]->DestroyComponent();
    PieceMeshes.RemoveAt(Source);
    ClearJoinSelection();
    RebuildMesh(Result);
    SetActorTickEnabled(true);
    LastEditMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    UE_LOG(LogTemp, Display, TEXT("AP5_JOIN: source=%d target=%d result=%d total=%d cpu_ms=%.2f"),
        Source, Target, Result, static_cast<int32>(Pieces.Items.size()), LastEditMilliseconds);
    return FString::Printf(TEXT("接合しました。接合先の姿勢に合わせて統合 / CPU処理 %.1f ms。再加工できます。"),
        LastEditMilliseconds);
}

void AAp5Monster::SyncPhysicsState()
{
    FAp5ProfileScope Profile(EAp5ProfileProcess::PhysicsSync);
    for (int32 I = 0; I < PieceMeshes.Num(); ++I)
    {
        UDynamicMeshComponent* Component = PieceMeshes[I];
        Ap5Volume::Piece& State = Pieces.Items[I];
        const FTransform Relative = Component->GetComponentTransform().GetRelativeTransform(GetActorTransform());
        State.Translation = VolumePoint(Relative.GetLocation());
        State.AxisX = VolumePoint(Relative.TransformVectorNoScale(FVector::ForwardVector));
        State.AxisY = VolumePoint(Relative.TransformVectorNoScale(FVector::RightVector));
        State.AxisZ = VolumePoint(Relative.TransformVectorNoScale(FVector::UpVector));
        State.Motion.OffsetZ = 0;
        if (Component->IsSimulatingPhysics())
        {
            State.OriginVelocity = VolumePoint(GetActorTransform().InverseTransformVectorNoScale(
                Component->GetPhysicsLinearVelocityAtPoint(Component->GetComponentLocation())));
            State.AngularVelocity = VolumePoint(GetActorTransform().InverseTransformVectorNoScale(
                Component->GetPhysicsAngularVelocityInRadians()));
            const bool bSleeping = !Component->RigidBodyIsAwake();
            if (bSleeping && !State.Motion.Landed)
                UE_LOG(LogTemp, Display, TEXT("AP5_FRAGMENT_SLEEP: index=%d"), I);
            State.Motion.Landed = bSleeping;
        }
    }
}

void AAp5Monster::UpdateArmMotion(float DeltaSeconds)
{
    if (!bArmMotionTest) return;
    ArmMotionTime+=FMath::Max(0.0f,DeltaSeconds);
    const float Frequency=2.0f*PI/3.0f;
    const float Phase=ArmMotionTime*Frequency;
    const float AngleRadians=FMath::DegreesToRadians(35.0f*FMath::Sin(Phase));
    const float AngularSpeed=FMath::DegreesToRadians(35.0f)*Frequency*FMath::Cos(Phase);
    const FQuat Rotation(FVector::RightVector,AngleRadians);
    for (int32 I=0;I<static_cast<int32>(Pieces.Items.size());++I)
    {
        Ap5Volume::Piece& State=Pieces.Items[I];
        if (!State.Driven || !State.Fixed || !PieceMeshes.IsValidIndex(I)) continue;
        const FVector Pivot=EnginePoint(State.JointPivot);
        const FVector RotatedPivot=Rotation.RotateVector(Pivot);
        const FVector Translation=Pivot-RotatedPivot;
        const FVector Omega=FVector::RightVector*AngularSpeed;
        const FVector OriginVelocity=-FVector::CrossProduct(Omega,RotatedPivot);
        State.AxisX=VolumePoint(Rotation.RotateVector(FVector::ForwardVector));
        State.AxisY=VolumePoint(Rotation.RotateVector(FVector::RightVector));
        State.AxisZ=VolumePoint(Rotation.RotateVector(FVector::UpVector));
        State.Translation=VolumePoint(Translation);
        State.OriginVelocity=VolumePoint(OriginVelocity);
        State.AngularVelocity=VolumePoint(Omega);
        const FTransform LocalTransform(Rotation,Translation);
        PieceMeshes[I]->SetWorldTransform(
            LocalTransform*GetActorTransform(),false,nullptr,ETeleportType::TeleportPhysics);
    }
}

void AAp5Monster::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateArmMotion(DeltaSeconds);
    SyncPhysicsState();
}

bool AAp5Monster::ToggleArmMotionTest()
{
    bArmMotionTest=!bArmMotionTest;
    ArmMotionTime=0.0f;
    ResetShape();
    UE_LOG(LogTemp, Display, TEXT("AP5_ARM_MOTION: enabled=%d"), bArmMotionTest ? 1 : 0);
    return bArmMotionTest;
}

void AAp5Monster::ResetShape()
{
    EndGrab();
    ClearJoinSelection();
    for (UDynamicMeshComponent* Mesh : PieceMeshes) Mesh->DestroyComponent();
    PieceMeshes.Empty();
    bool ArticulatedReady=true;
    if (bArmMotionTest)
    {
        // +Y側の右腕を肩の外縁で切りしろ無しに分け、肩中心付近を関節拘束点にする。
        ArticulatedReady=Pieces.ResetArticulated(
            InitialVolume,Ap5Volume::Point(0,72,233),Ap5Volume::Point(0,1,0),
            Ap5Volume::Point(0,72,233),Ap5Volume::Point(0,82,233));
    }
    else
    {
        Pieces.Reset(InitialVolume);
    }
    if (!ArticulatedReady)
    {
        bArmMotionTest=false;
        Pieces.Reset(InitialVolume);
        UE_LOG(LogTemp, Error, TEXT("AP5_ARM_MOTION_SETUP_FAILED"));
    }
    for (int32 I=0;I<static_cast<int32>(Pieces.Items.size());++I)
    {
        PieceMeshes.Add(CreatePiece());
        RebuildMesh(I);
    }
    ArmMotionTime=0.0f;
    UpdateArmMotion(0.0f);
    SetActorTickEnabled(bArmMotionTest);
    UE_LOG(LogTemp, Display, TEXT("AP5_VOLUME_RESET: articulated=%d pieces=%d"),
        bArmMotionTest ? 1 : 0, static_cast<int32>(Pieces.Items.size()));
}
