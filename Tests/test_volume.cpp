// 実際の体積演算をUEなしで検証する。描画・入力の確認はWindows側で別途行う。
#include "../Source/Ap5/Ap5Volume.h"
#include "../Source/Ap5/Ap5Pieces.h"
#include <cassert>
#include <iostream>
#include <map>
#include <tuple>

using Ap5Volume::Point;
using Ap5Volume::Field;
using Ap5Volume::Triangle;
typedef std::tuple<long long,long long,long long> VertexKey;
typedef std::pair<VertexKey,VertexKey> EdgeKey;

VertexKey Key(const Point& P)
{
    return VertexKey(std::llround(P.X*1e7),std::llround(P.Y*1e7),std::llround(P.Z*1e7));
}

void CheckSurface(const Field& Volume)
{
    const std::vector<Triangle> Surface = Volume.Surface();
    assert(!Surface.empty());
    std::map<EdgeKey,int> Edges;
    for (const Triangle& T : Surface)
    {
        const Point Cross = (T.Vertices[1]-T.Vertices[0]).Cross(T.Vertices[2]-T.Vertices[0]);
        assert(Cross.Length()>1e-10);
        for (int K=0; K<3; ++K)
        {
            assert(std::isfinite(T.Normals[K].X));
            assert(std::abs(T.Normals[K].Length()-1)<1e-8);
            VertexKey A=Key(T.Vertices[K]), B=Key(T.Vertices[(K+1)%3]);
            if (B<A) std::swap(A,B);
            ++Edges[EdgeKey(A,B)];
        }
    }
    // 描画用の頂点は面ごとに独立。座標で共有辺を数え、外皮と穴の内壁に隙間がないか確認する。
    for (const std::pair<const EdgeKey,int>& Edge : Edges) assert(Edge.second==2);
}

int PieceAt(const Ap5Volume::PieceCollection& Scene,const Point& Local)
{
    for (size_t I=0;I<Scene.Items.size();++I)
        if (Scene.Items[I].Volume.Sample(Local)<0) return static_cast<int>(I);
    return -1;
}

int main()
{
    Field Volume;
    Volume.Initialize(Point(55,72,85));
    assert(Volume.Sample(Point(0,0,0))<0);
    CheckSurface(Volume);
    // 以前の裏面表示を再発させないよう、外向き法線とUE用頂点順の関係を検証する。
    for (const Triangle& T : Volume.Surface())
    {
        const Point Cross = (T.Vertices[1]-T.Vertices[0]).Cross(T.Vertices[2]-T.Vertices[0]);
        assert(Cross.Dot(T.Normals[0]+T.Normals[1]+T.Normals[2])<0);
    }
    const Point Start(-200,0,0), Axis(1,0,0);
    assert(Volume.Brush(Start,Axis,20,false)>0);
    // 手前から奥まで貫通し、離れた材料は残る。
    for (int X=-50; X<=50; X+=5) assert(Volume.Sample(Point(X,0,0))>0);
    assert(Volume.Sample(Point(0,40,0))<0);
    CheckSurface(Volume);
    // 二つ目の穴を作り、一つ目だけ修復しても二つ目は残る。
    Volume.Brush(Point(-200,42,0),Axis,12,false);
    Volume.Brush(Start,Axis,20,true);
    assert(Volume.Sample(Point(0,0,0))<0);
    assert(Volume.Sample(Point(0,42,0))>0);
    assert(Volume.Sample(Point(70,0,0))>0);
    CheckSurface(Volume);
    // 穴が空いた状態で斜めから加工しても閉じた表面を維持する。
    Volume.Brush(Point(-200,-130,-40),Point(1,0.65,0.2),12,false);
    CheckSurface(Volume);
    Volume.Reset();
    assert(Volume.Sample(Point(0,42,0))<0);
    CheckSurface(Volume);
    assert(Volume.Brush(Point(-200,400,0),Axis,20,false)==0);
    assert(Volume.Brush(Start,Point(),20,false)==0);
    Volume.Brush(Start,Axis,200,false);
    assert(Volume.Surface().empty());
    Volume.Brush(Start,Axis,200,true);
    assert(Volume.Sample(Point(0,0,0))<0);
    CheckSurface(Volume);
    std::cout << "体積演算：貫通・局所修復・外形制限・斜め加工・閉曲面・リセットの検証成功\n";
    // 平面が外れた場合は無変更。水平・垂直・斜めの切断で両側が閉じる。
    const int OriginalCount=Volume.MaterialCount();
    assert(Volume.Cut(Point(500,0,0),Point(1,0,0)).empty());
    assert(Volume.Cut(Point(),Point()).empty());
    assert(Volume.MaterialCount()==OriginalCount);
    const Point Directions[3]={Point(1,0,0),Point(0,0,1),Point(1,0.4,0.7)};
    for (const Point& N : Directions)
    {
        const std::vector<Field> Pieces=Volume.Cut(Point(0,0,7),N);
        assert(Pieces.size()==2);
        int Total=0;
        for (const Field& Piece : Pieces)
        {
            CheckSurface(Piece);
            assert(Piece.Components().size()==1);
            Total+=Piece.MaterialCount();
        }
        // 切りしろが格子間に収まる場合、負値の格子点数は変わらない。
        assert(Total<=OriginalCount);
    }
    Volume.Brush(Point(-200,0,0),Point(1,0,0),20,false);
    const std::vector<Field> HoledPieces=Volume.Cut(Point(0,0,3),Point(1,0.4,0.7));
    assert(HoledPieces.size()==2);
    for (const Field& Piece : HoledPieces) CheckSurface(Piece);

    // 三つの重なった部位を一本の腕とみなし、中央を切ると先端がまとまって分離する。
    std::vector<Ap5Volume::Ellipsoid> Shapes;
    for (int I=0;I<3;++I)
    {
        Ap5Volume::Ellipsoid E;
        E.Center=Point(0,I*35,0); E.Radii=Point(25,30,25); Shapes.push_back(E);
    }
    Field Arm; Arm.InitializeUnion(Shapes);
    assert(Arm.Components().size()==1);
    CheckSurface(Arm);
    const std::vector<Field> ArmPieces=Arm.Cut(Point(0,20,0),Point(0,1,0));
    assert(ArmPieces.size()==2);
    bool TipConnected=false;
    for (const Field& Piece : ArmPieces)
    {
        CheckSurface(Piece);
        if (Piece.Sample(Point(0,35,0))<0 && Piece.Sample(Point(0,70,0))<0) TipConnected=true;
    }
    assert(TipConnected);
    // 同じ側にある離れた物体を一つの破片として扱わない。
    Shapes[2].Center=Point(0,150,0);
    Arm.InitializeUnion(Shapes);
    assert(Arm.Components().size()==2);
    std::cout << "切断：水平・垂直・斜め・穴のある断面・連結判定の検証成功\n";
    Ap5Volume::FallState Falling;
    Falling.MinimumZ=100;
    assert(!Falling.Advance(0,0));
    assert(!Falling.Advance(0.1,0));
    assert(std::abs(Falling.OffsetZ+4.9)<1e-8);
    assert(Falling.Advance(2,0));
    assert(Falling.Landed && Falling.OffsetZ==-100 && Falling.VelocityZ==0);
    assert(!Falling.Advance(2,0) && Falling.OffsetZ==-100);
    std::cout << "落下：重力による移動・長いフレームでの床貫通防止・着地後の停止を確認\n";

    // ゲームで使う共通コレクションを直接検証。移動済みの破片も現在位置で加工する。
    Ap5Volume::Ellipsoid TestShape;
    TestShape.Center=Point(0,0,160); TestShape.Radii=Point(30,30,60);
    Field Initial; Initial.InitializeUnion({TestShape});
    Ap5Volume::PieceCollection Scene;
    Scene.Reset(Initial,{Point(0,-15,120)});
    std::vector<int> Changed;
    assert(Scene.Cut(Point(0,0,0),Point(0,1,0),Changed)==1);
    for (Ap5Volume::Piece& P : Scene.Items) P.RebuildSurface();
    assert(Scene.Items.size()==2 && Scene.Items[0].Fixed && !Scene.Items[1].Fixed);
    assert(Scene.Items[1].Motion.Advance(10,0));
    const double LandedOffset=Scene.Items[1].Motion.OffsetZ;
    const int BodyCount=Scene.Items[0].Volume.MaterialCount();
    // 本体は高さ100cm以上に残り、着地した破片だけを高さ30cmで切る。
    assert(Scene.Cut(Point(0,0,30),Point(0,0,1),Changed)==1);
    assert(Scene.Items.size()==3 && Changed.size()==2);
    assert(Scene.Items[0].Volume.MaterialCount()==BodyCount);
    for (size_t I=1;I<Scene.Items.size();++I)
    {
        assert(!Scene.Items[I].Fixed && Scene.Items[I].Motion.OffsetZ==LandedOffset);
        Scene.Items[I].RebuildSurface();
        assert(!Scene.Items[I].Motion.Landed);
        CheckSurface(Scene.Items[I].Volume);
    }
    // 上限で拒否した切断が一部だけ適用されることを防ぐ。
    const int BeforeLimit=Scene.Items[1].Volume.MaterialCount();
    assert(Scene.Cut(Point(0,0,45),Point(0,0,1),Changed,3)==-1);
    assert(Changed.empty() && Scene.Items.size()==3 && Scene.Items[1].Volume.MaterialCount()==BeforeLimit);
    for (size_t I=1;I<Scene.Items.size();++I) Scene.Items[I].Motion.Advance(10,0);
    const Point Target(0,15,50);
    const Point LocalTarget=Scene.Items[1].ToLocal(Target);
    assert(Scene.Items[1].Volume.Sample(LocalTarget)<0);
    Scene.Brush(Point(-200,15,50),Point(1,0,0),8,false,Changed);
    assert(Scene.Items[1].Volume.Sample(LocalTarget)>0);
    Scene.Brush(Point(-200,15,50),Point(1,0,0),12,true,Changed);
    assert(Scene.Items[1].Volume.Sample(LocalTarget)<0);
    // 修復しても、切断で別の塊になった側へ材料は戻らない。
    assert(Scene.Items[1].Volume.Sample(Point(0,-15,160))>0);

    // 空中の再切断でも、位置と落下速度を全ての子に引き継ぐ。
    Scene.Reset(Initial,{Point(0,-15,120)});
    Scene.Cut(Point(),Point(0,1,0),Changed);
    for (Ap5Volume::Piece& P : Scene.Items) P.RebuildSurface();
    assert(!Scene.Items[1].Motion.Advance(0.1,0));
    const double AirOffset=Scene.Items[1].Motion.OffsetZ, AirVelocity=Scene.Items[1].Motion.VelocityZ;
    assert(Scene.Cut(Point(0,0,160+AirOffset),Point(0,0,1),Changed)>0);
    int MovingChildren=0;
    for (Ap5Volume::Piece& P : Scene.Items)
    {
        if (P.Motion.OffsetZ==AirOffset)
        {
            assert(!P.Fixed && P.Motion.VelocityZ==AirVelocity);
            ++MovingChildren;
        }
    }
    assert(MovingChildren==2);
    Scene.Reset(Initial,{Point(0,-15,120)});
    assert(Scene.Items.size()==1 && Scene.Items[0].Fixed && Scene.Items[0].Motion.OffsetZ==0);
    std::cout << "共通加工：着地後・落下中の再切断、位置と速度の継承、破片の穴あけ・修復、上限とリセットを確認\n";
    // 一発では表面だけをへこませ、同じ射線の繰り返しで貫通する。
    Field TargetVolume; TargetVolume.Initialize(Point(40,43,47));
    Scene.Reset(TargetVolume);
    const Point ShotStart(-200,1,2), ShotDirection(1,0,0);
    double BeforeHit=0, AfterHit=0;
    assert(Scene.Items[0].Volume.Trace(ShotStart,ShotDirection,4000,BeforeHit));
    assert(Scene.Impact(ShotStart,ShotDirection,20,8,Changed)>0);
    assert(Changed.size()==1 && Changed[0]==0);
    assert(Scene.Items[0].Volume.Trace(ShotStart,ShotDirection,4000,AfterHit));
    assert(AfterHit>BeforeHit+3 && AfterHit<BeforeHit+12);
    assert(Scene.Items[0].Volume.Sample(Point(0,1,2))<0);
    assert(Scene.Items[0].Volume.Sample(Point(35,1,2))<0);
    CheckSurface(Scene.Items[0].Volume);
    int Shots=1;
    while (Scene.Items[0].Volume.Trace(ShotStart,ShotDirection,4000,BeforeHit) && Shots<40)
    {
        assert(Scene.Impact(ShotStart,ShotDirection,20,8,Changed)>0);
        ++Shots;
        if (Scene.Items[0].Volume.Trace(ShotStart,ShotDirection,4000,AfterHit))
            assert(AfterHit>BeforeHit);
    }
    assert(Shots>1 && Shots<40);
    CheckSurface(Scene.Items[0].Volume);
    assert(Scene.Impact(ShotStart,ShotDirection,20,8,Changed)==0 && Changed.empty());
    assert(Scene.Impact(Point(-200,100,0),ShotDirection,20,8,Changed)==0);
    // 分離前の弾痕は、加工前の外形まで修復できる。
    Scene.Reset(TargetVolume);
    Scene.Impact(ShotStart,ShotDirection,20,8,Changed);
    Scene.Brush(ShotStart,ShotDirection,25,true,Changed);
    assert(Scene.Items[0].Volume.Trace(ShotStart,ShotDirection,4000,AfterHit));
    assert(TargetVolume.Trace(ShotStart,ShotDirection,4000,BeforeHit));
    assert(std::abs(AfterHit-BeforeHit)<0.01);

    // 後ろの塊を先に登録しても最も手前だけに命中する。落下後の座標にも対応。
    Scene.Reset(TargetVolume);
    Ap5Volume::Piece Front;
    Ap5Volume::Ellipsoid FrontShape;
    FrontShape.Center=Point(-120,0,100); FrontShape.Radii=Point(30,35,40);
    Front.Volume.InitializeUnion({FrontShape});
    Front.Motion.OffsetZ=-100;
    Scene.Items.push_back(Front);
    const int BackCount=Scene.Items[0].Volume.MaterialCount();
    assert(Scene.Impact(ShotStart,ShotDirection,20,8,Changed)>0);
    assert(Changed.size()==1 && Changed[0]==1);
    assert(Scene.Items[0].Volume.MaterialCount()==BackCount);
    for (int I=0;I<40 && Scene.Items[0].Volume.MaterialCount()==BackCount;++I)
        assert(Scene.Impact(ShotStart,ShotDirection,20,8,Changed)>0 && !Changed.empty());
    assert(Changed[0]==0);
    assert(Scene.Items[0].Volume.MaterialCount()<BackCount);
    // 逆方向と不正な射線。
    assert(TargetVolume.Trace(Point(200,1,2),Point(-1,0,0),4000,BeforeHit));
    assert(!TargetVolume.Trace(ShotStart,Point(),4000,BeforeHit));
    assert(!TargetVolume.Trace(ShotStart,ShotDirection,10,BeforeHit));
    std::cout << "弾痕：浅いくぼみ・反復貫通・閉曲面・修復・手前優先・移動済み破片を確認\n";

    // 両側の太い塊を細い首でつなぐ。首が残る穴では分離せず、削り切ると分離。
    Ap5Volume::Ellipsoid Left, Neck, Right;
    Left.Center=Point(0,-40,100); Left.Radii=Point(28,32,28);
    Right=Left; Right.Center.Y=40;
    Neck.Center=Point(0,0,100); Neck.Radii=Point(10,35,10);
    Field Connected; Connected.InitializeUnion({Left,Neck,Right});
    assert(Connected.Components().size()==1);
    Scene.Reset(Connected,{Point(0,-40,80)});
    const Point NeckStart(-200,0,100);
    Scene.Brush(NeckStart,ShotDirection,4,false,Changed);
    assert(Scene.Items.size()==1);
    Scene.Brush(NeckStart,ShotDirection,14,false,Changed);
    assert(Scene.Items.size()==2 && Changed.size()==2);
    assert(Scene.Items[0].Fixed != Scene.Items[1].Fixed);
    for (Ap5Volume::Piece& P : Scene.Items)
    {
        assert(P.Volume.Components().size()==1);
        P.RebuildSurface();
        CheckSurface(P.Volume);
    }
    const size_t DetachedIndex=Scene.Items[0].Fixed ? 1 : 0;
    assert(Scene.Items[DetachedIndex].Motion.Advance(10,0));
    // 修復で分離時の隙間が埋まって再接続されることはない。
    Scene.Brush(NeckStart,ShotDirection,30,true,Changed);
    assert(Scene.Items[0].Volume.Sample(Point(0,0,100))>0);

    // 弾痕の反復でも同じ首を分離し、空中の親の位置・速度を両方に継承する。
    Scene.Reset(Connected);
    Scene.Items[0].Fixed=false;
    Scene.Items[0].Motion.OffsetZ=-12;
    Scene.Items[0].Motion.VelocityZ=-30;
    const Point MovingNeckStart(-200,0,88);
    int NeckShots=0;
    while (Scene.Items.size()==1 && NeckShots<20)
    {
        assert(Scene.Impact(MovingNeckStart,ShotDirection,20,8,Changed)>0);
        ++NeckShots;
    }
    assert(NeckShots>1 && NeckShots<20 && Scene.Items.size()==2);
    for (Ap5Volume::Piece& P : Scene.Items)
    {
        assert(!P.Fixed && P.Motion.OffsetZ==-12 && P.Motion.VelocityZ==-30);
        P.RebuildSurface(); CheckSurface(P.Volume);
    }

    // 分離上限超過は穴あけ・弾痕とも元の形状を保持して拒否。
    Scene.Reset(Connected);
    const int ConnectedCount=Connected.MaterialCount();
    assert(Scene.Brush(NeckStart,ShotDirection,14,false,Changed,1)==-1);
    assert(Changed.empty() && Scene.Items.size()==1);
    assert(Scene.Items[0].Volume.MaterialCount()==ConnectedCount);
    int Rejected=0;
    for (int I=0;I<20;++I)
    {
        const int BeforeCount=Scene.Items[0].Volume.MaterialCount();
        double BeforeDistance=0, AfterDistance=0;
        Scene.Items[0].Volume.Trace(NeckStart,ShotDirection,4000,BeforeDistance);
        const int Result=Scene.Impact(NeckStart,ShotDirection,20,8,Changed,1);
        if (Result<0)
        {
            assert(Changed.empty() && Scene.Items.size()==1);
            assert(Scene.Items[0].Volume.MaterialCount()==BeforeCount);
            Scene.Items[0].Volume.Trace(NeckStart,ShotDirection,4000,AfterDistance);
            assert(BeforeDistance==AfterDistance);
            ++Rejected;
            break;
        }
    }
    assert(Rejected==1);
    // 複数対象のうち後半で上限に達しても、手前の加工を確定しない。
    Scene.Reset(TargetVolume);
    Ap5Volume::Piece NeckPiece; NeckPiece.Volume=Connected;
    NeckPiece.Motion.OffsetZ=-100;
    Scene.Items.push_back(NeckPiece);
    const int SphereCount=TargetVolume.MaterialCount();
    assert(Scene.Brush(Point(-200,0,0),ShotDirection,14,false,Changed,2)==-1);
    assert(Changed.empty() && Scene.Items.size()==2);
    assert(Scene.Items[0].Volume.MaterialCount()==SphereCount);
    assert(Scene.Items[1].Volume.MaterialCount()==ConnectedCount);
    // 連結成分上限と、材料が空になった場合を区別する。
    std::vector<Ap5Volume::Ellipsoid> ManyShapes;
    for (int I=0;I<33;++I)
    {
        Ap5Volume::Ellipsoid E; E.Center=Point(I*15,0,0); E.Radii=Point(4,4,4);
        ManyShapes.push_back(E);
    }
    Field Many; Many.InitializeUnion(ManyShapes);
    bool Exceeded=false;
    assert(Many.Components(&Exceeded).empty() && Exceeded);
    // 全材料消去を「成分上限」と混同しない。
    Scene.Reset(Connected);
    assert(Scene.Brush(NeckStart,ShotDirection,200,false,Changed)>0);
    assert(Scene.Items.size()==1 && Scene.Items[0].Volume.MaterialCount()==0);
    Scene.Brush(NeckStart,ShotDirection,200,true,Changed);
    assert(Scene.Items[0].Volume.MaterialCount()==ConnectedCount);
    std::cout << "自動分離：接続維持・穴あけと弾痕での分離・落下・状態継承・修復制限・上限拒否を確認\n";

    // 回転・平行移動した物体への加工は、元の物体へ同じ局所操作をした結果と一致する。
    for (int Mode=0;Mode<4;++Mode)
    {
        Ap5Volume::PieceCollection Reference, Rotated;
        Reference.Reset(Connected); Rotated.Reset(Connected);
        Ap5Volume::Piece& Pose=Rotated.Items[0];
        Pose.Fixed=false;
        Pose.Translation=Point(200,-80,50);
        Pose.AxisX=Point(0,0,1); Pose.AxisY=Point(1,0,0); Pose.AxisZ=Point(0,1,0);
        Pose.OriginVelocity=Point(10,20,-30); Pose.AngularVelocity=Point(0,0,2);
        const Point WorldStart=Pose.ToWorld(NeckStart), WorldDirection=Pose.ToWorldVector(ShotDirection);
        const Point WorldPlane=Pose.ToWorld(Point(0,0,100)), WorldNormal=Pose.ToWorldVector(Point(0,1,0));
        assert((Pose.ToLocal(WorldStart)-NeckStart).Length()<1e-9);
        assert((Pose.VelocityAt(Pose.ToWorld(Point())+Point(3,0,0))-Point(10,26,-30)).Length()<1e-9);
        std::vector<int> ReferenceChanged;
        if (Mode==0)
        {
            Reference.Brush(NeckStart,ShotDirection,14,false,ReferenceChanged);
            Rotated.Brush(WorldStart,WorldDirection,14,false,Changed);
        }
        else if (Mode==1)
        {
            Reference.Impact(NeckStart,ShotDirection,20,8,ReferenceChanged);
            Rotated.Impact(WorldStart,WorldDirection,20,8,Changed);
        }
        else if (Mode==2)
        {
            Reference.Cut(Point(0,0,100),Point(0,1,0),ReferenceChanged);
            Rotated.Cut(WorldPlane,WorldNormal,Changed);
        }
        else
        {
            Reference.Impact(NeckStart,ShotDirection,20,8,ReferenceChanged);
            Rotated.Impact(WorldStart,WorldDirection,20,8,Changed);
            Reference.Brush(NeckStart,ShotDirection,25,true,ReferenceChanged);
            Rotated.Brush(WorldStart,WorldDirection,25,true,Changed);
        }
        assert(Reference.Items.size()==Rotated.Items.size());
        for (size_t I=0;I<Rotated.Items.size();++I)
        {
            const Ap5Volume::Piece& P=Rotated.Items[I];
            assert(P.Volume.MaterialCount()==Reference.Items[I].Volume.MaterialCount());
            assert((P.Translation-Point(200,-80,50)).Length()<1e-9);
            assert((P.AxisX-Point(0,0,1)).Length()<1e-9);
            assert((P.AngularVelocity-Point(0,0,2)).Length()<1e-9 && !P.Fixed);
            assert((P.OriginVelocity-Point(10,20,-30)).Length()<1e-9);
        }
    }
    // 衝突箱は材料内部のセルをまとめたもの。貫通穴の中心をふさがない。
    Field CollisionVolume; CollisionVolume.Initialize(Point(40,43,47));
    const size_t BeforeBoxes=CollisionVolume.CollisionBoxes().size();
    assert(BeforeBoxes>0);
    CollisionVolume.Brush(Point(-200,0,0),Point(1,0,0),15,false);
    const std::vector<Ap5Volume::CollisionBox> Boxes=CollisionVolume.CollisionBoxes();
    assert(!Boxes.empty());
    double BoxVolume=0;
    for (const Ap5Volume::CollisionBox& B : Boxes)
    {
        BoxVolume+=B.Size.X*B.Size.Y*B.Size.Z;
        assert(std::abs(B.Center.Y)>B.Size.Y*0.5 || std::abs(B.Center.Z)>B.Size.Z*0.5);
        for (int Z=-1;Z<=1;Z+=2) for (int Y=-1;Y<=1;Y+=2) for (int X=-1;X<=1;X+=2)
            assert(CollisionVolume.Sample(B.Center+Point(X*B.Size.X,Y*B.Size.Y,Z*B.Size.Z)*0.5)<0);
    }
    assert(BoxVolume>0);
    CollisionVolume.Brush(Point(-200,0,0),Point(1,0,0),200,false);
    assert(CollisionVolume.CollisionBoxes().empty());
    Field Thin; Thin.Initialize(Point(3,20,20),5);
    const std::vector<Ap5Volume::CollisionBox> ThinBoxes=Thin.CollisionBoxes();
    assert(ThinBoxes.size()==1 && Thin.Sample(ThinBoxes[0].Center)<0);
    std::cout << "物理用データ：回転後の加工・姿勢と速度の継承・穴を保持する衝突箱・薄片と空形状を確認\n";

    // 大きい上半身も、足元の支持点から離れたら固定を解除する。
    Scene.Reset(Initial);
    assert(Scene.Cut(Point(0,0,125),Point(0,0,1),Changed)==1);
    int Supported=0, Free=0;
    for (const Ap5Volume::Piece& P : Scene.Items)
    {
        if (P.Volume.Sample(Point(0,0,180))<0)
        {
            assert(!P.Fixed); ++Free;
        }
        if (P.Volume.Sample(Point(0,0,110))<0)
        {
            assert(P.Fixed); ++Supported;
        }
    }
    assert(Supported==1 && Free==1);
    assert(!Scene.Items[0].Fixed); // 最大の塊を固定する旧挙動を再発させない。
    // 左右それぞれに足元を残す縦分割では、両側とも支持される。
    Scene.Reset(Connected);
    assert(Scene.Cut(Point(0,0,100),Point(0,1,0),Changed)==1);
    assert(Scene.Items.size()==2 && Scene.Items[0].Fixed && Scene.Items[1].Fixed);
    // 分離せずに支持点だけを削り落とした場合も固定解除する。
    Scene.Reset(Initial);
    const std::vector<Point> Feet=Initial.LowestMaterialPoints();
    assert(!Feet.empty());
    const Point SupportRay(-200,0,Feet[0].Z);
    Scene.Brush(SupportRay,Point(1,0,0),25,false,Changed);
    assert(Scene.Items.size()==1 && !Scene.Items[0].Fixed);
    Scene.Brush(SupportRay,Point(1,0,0),40,true,Changed);
    assert(!Scene.Items[0].Fixed); // 修復や着地で固定へ戻さない。
    Scene.Reset(Initial);
    assert(Scene.Items.size()==1 && Scene.Items[0].Fixed);

    // 自動支持では、支持点が残っていても重心投影が大きく外れれば固定しない。
    Ap5Volume::Ellipsoid StableTorso, StableLeftFoot, StableRightFoot, StableLeftLeg, StableRightLeg;
    StableTorso.Center=Point(0,0,85); StableTorso.Radii=Point(28,42,45);
    StableLeftFoot.Center=Point(0,-38,15); StableLeftFoot.Radii=Point(24,25,15);
    StableRightFoot=StableLeftFoot; StableRightFoot.Center.Y=38;
    StableLeftLeg.Center=Point(0,-28,45); StableLeftLeg.Radii=Point(22,24,35);
    StableRightLeg=StableLeftLeg; StableRightLeg.Center.Y=28;
    Field TwoFootBody; TwoFootBody.InitializeUnion(
        {StableTorso,StableLeftFoot,StableRightFoot,StableLeftLeg,StableRightLeg});
    Scene.Reset(TwoFootBody);
    assert(Scene.Items.size()==1 && Scene.Items[0].Fixed);

    // 片足側だけを残した同程度の上体では、重心が支持範囲を外れて自由化する。
    Ap5Volume::Ellipsoid LeanTorso=StableTorso;
    LeanTorso.Center.Y=28;
    Field OneFootBody; OneFootBody.InitializeUnion({LeanTorso,StableLeftFoot,StableLeftLeg});
    Scene.Reset(OneFootBody);
    assert(Scene.Items.size()==1 && !Scene.Items[0].Fixed);

    // 明示支持点は従来のアンカー用途を維持する。
    Scene.Reset(OneFootBody,{Point(0,-38,15)});
    assert(Scene.Items.size()==1 && Scene.Items[0].Fixed);
    std::cout << "支持判定：支持点消失・両足支持・重心投影による片足不安定・明示アンカー互換を確認\n";

    // 接合は全リセットではない。切断前と切断後に開けた離れた穴を残す。
    Field Damaged=Initial;
    Damaged.Brush(Point(-200,-12,185),Point(1,0,0),7,false);
    Scene.Reset(Damaged);
    assert(Scene.Cut(Point(0,0,145),Point(0,0,1),Changed)==1);
    int Upper=PieceAt(Scene,Point(0,0,170)), Lower=PieceAt(Scene,Point(0,0,120));
    assert(Upper>=0 && Lower>=0 && Upper!=Lower && !Scene.Items[Upper].Fixed);
    Scene.Items[Upper].Volume.Brush(Point(-200,12,195),Point(1,0,0),7,false);
    Scene.Items[Upper].Translation=Point(150,70,-30);
    Scene.Items[Upper].AxisX=Point(0,0,1); Scene.Items[Upper].AxisY=Point(1,0,0); Scene.Items[Upper].AxisZ=Point(0,1,0);
    const Point PickStart=Scene.Items[Upper].ToWorld(Point(-200,0,170));
    const Point PickDirection=Scene.Items[Upper].ToWorldVector(Point(1,0,0));
    double PickDistance=0;
    assert(Scene.Pick(PickStart,PickDirection,PickDistance)==Upper);
    std::weak_ptr<const Ap5Volume::Separation> History=Scene.Items[Upper].Origin;
    assert(Scene.Join(Upper,Lower)==0 && Scene.Items.size()==1 && Scene.Items[0].Fixed);
    assert(Scene.Items[0].Translation.Length()==0 && Scene.Items[0].AxisX.X==1);
    assert(Scene.Items[0].Volume.Sample(Point(0,-12,185))>0);
    assert(Scene.Items[0].Volume.Sample(Point(0,12,195))>0);
    assert(Scene.Items[0].Volume.Sample(Point(0,0,145))<0);
    assert(Scene.Items[0].Volume.Components().size()==1);
    assert(History.expired() && !Scene.Items[0].Origin);
    CheckSurface(Scene.Items[0].Volume);
    // 再接合後にも再切断・再接合でき、履歴を積み上げ続けない。
    for (int I=0;I<3;++I)
    {
        assert(Scene.Cut(Point(0,0,170),Point(0,0,1),Changed)==1);
        Upper=PieceAt(Scene,Point(0,0,205)); Lower=PieceAt(Scene,Point(0,0,120));
        assert(Scene.Join(Upper,Lower)==0);
        assert(!Scene.Items[0].Origin);
    }
    Scene.Brush(Point(-200,0,160),Point(1,0,0),7,false,Changed);
    assert(Scene.Items[0].Volume.Sample(Point(0,0,160))>0);
    Scene.Brush(Point(-200,0,160),Point(1,0,0),10,true,Changed);
    assert(Scene.Items[0].Volume.Sample(Point(0,0,160))<0);

    // 間の破片が欠けている場合は接合を拒否し、選択した二つ以外を復元しない。
    Scene.Reset(Initial);
    Scene.Cut(Point(0,0,140),Point(0,0,1),Changed);
    Scene.Cut(Point(0,0,180),Point(0,0,1),Changed);
    assert(Scene.Items.size()==3);
    Upper=PieceAt(Scene,Point(0,0,200)); Lower=PieceAt(Scene,Point(0,0,120));
    int Middle=PieceAt(Scene,Point(0,0,160));
    const int UpperCount=Scene.Items[Upper].Volume.MaterialCount();
    assert(Scene.Join(Upper,Lower)==-3 && Scene.Items.size()==3);
    assert(Scene.Items[Upper].Volume.MaterialCount()==UpperCount);
    // 自由な接合先へ合わせる場合は、接合先の姿勢と速度を保持する。
    Scene.Items[Middle].Translation=Point(40,50,-60);
    Scene.Items[Middle].OriginVelocity=Point(1,2,3);
    Scene.Items[Middle].AngularVelocity=Point(0,0,2);
    const int Combined=Scene.Join(Upper,Middle);
    assert(Combined>=0 && Scene.Items.size()==2 && !Scene.Items[Combined].Fixed);
    assert((Scene.Items[Combined].Translation-Point(40,50,-60)).Length()<1e-9);
    assert((Scene.Items[Combined].OriginVelocity-Point(1,2,3)).Length()<1e-9);
    assert(Scene.Items[Combined].AngularVelocity.Z==2);
    Lower=PieceAt(Scene,Point(0,0,120));
    assert(Scene.Join(Combined,Lower)==0 && Scene.Items[0].Fixed);
    assert(Scene.Items.size()==1 && !Scene.Items[0].Origin);
    // 不正な選択・固定された接合元・別の分離元は変更せず拒否。
    assert(Scene.Join(0,0)==-1 && Scene.Join(-1,0)==-1);
    Ap5Volume::Piece Unrelated; Unrelated.Volume=Initial;
    Scene.Items.push_back(Unrelated);
    assert(Scene.Join(0,1)==-1);
    assert(Scene.Join(1,0)==-2 && Scene.Items.size()==2);
    Scene.Reset(Initial);
    assert(Scene.Items.size()==1 && !Scene.Items[0].Origin);
    std::cout << "接合：姿勢合わせ・穴の保持・隙間補完・再加工・多段分離・不正な接合拒否・履歴解放を確認\n";

    // 関節検証用の分割は切りしろを作らず、駆動中の加工と切断後の自由化を両立する。
    Ap5Volume::Ellipsoid JointBody, JointShoulder, JointArm, JointHand;
    JointBody.Center=Point(0,0,120); JointBody.Radii=Point(40,50,80);
    JointShoulder.Center=Point(0,55,145); JointShoulder.Radii=Point(28,30,30);
    JointArm.Center=Point(0,90,120); JointArm.Radii=Point(24,40,45);
    JointHand.Center=Point(0,120,85); JointHand.Radii=Point(25,25,30);
    Field JointModel; JointModel.InitializeUnion({JointBody,JointShoulder,JointArm,JointHand});
    const Point JointPlane(0,50,145), JointNormal(0,1,0);
    const Point JointPivot(0,50,145), JointAnchor(0,65,145);
    assert(Scene.ResetArticulated(JointModel,JointPlane,JointNormal,JointPivot,JointAnchor));
    assert(Scene.Items.size()==2);
    int Driven=-1, StaticBody=-1;
    for (size_t I=0;I<Scene.Items.size();++I)
    {
        CheckSurface(Scene.Items[I].Volume);
        if (Scene.Items[I].Driven) Driven=static_cast<int>(I);
        else if (Scene.Items[I].Fixed) StaticBody=static_cast<int>(I);
    }
    assert(Driven>=0 && StaticBody>=0 && Driven!=StaticBody);
    Ap5Volume::Piece& MovingArm=Scene.Items[Driven];
    MovingArm.AxisX=Point(0,0,-1);
    MovingArm.AxisY=Point(0,1,0);
    MovingArm.AxisZ=Point(1,0,0);
    const Point RotatedPivot=MovingArm.ToWorldVector(JointPivot);
    MovingArm.Translation=JointPivot-RotatedPivot;
    MovingArm.OriginVelocity=Point(15,0,0);
    MovingArm.AngularVelocity=Point(0,1,0);
    const Point LocalShot(-200,115,100), LocalShotAxis(1,0,0), ArmLocalTarget(0,115,100);
    const Point MovingShot=MovingArm.ToWorld(LocalShot);
    const Point MovingShotAxis=MovingArm.ToWorldVector(LocalShotAxis);
    assert(Scene.Brush(MovingShot,MovingShotAxis,8,false,Changed)>0);
    assert(Scene.Items.size()==2 && Scene.Items[Driven].Volume.Sample(ArmLocalTarget)>0);
    const Point MovingCutPoint=Scene.Items[Driven].ToWorld(Point(0,95,120));
    const Point MovingCutNormal=Scene.Items[Driven].ToWorldVector(Point(0,1,0));
    assert(Scene.Cut(MovingCutPoint,MovingCutNormal,Changed)==1);
    assert(Scene.Items.size()==3);
    int DrivenAfter=-1, Detached=-1;
    for (size_t I=0;I<Scene.Items.size();++I)
    {
        if (Scene.Items[I].Driven)
        {
            assert(Scene.Items[I].Fixed);
            DrivenAfter=static_cast<int>(I);
        }
        else if (!Scene.Items[I].Fixed && Scene.Items[I].Origin)
        {
            Detached=static_cast<int>(I);
        }
    }
    assert(DrivenAfter>=0 && Detached>=0);
    assert((Scene.Items[Detached].AxisX-Point(0,0,-1)).Length()<1e-9);
    assert((Scene.Items[Detached].OriginVelocity-Point(15,0,0)).Length()<1e-9);
    assert((Scene.Items[Detached].AngularVelocity-Point(0,1,0)).Length()<1e-9);
    const int Rejoined=Scene.Join(Detached,DrivenAfter);
    assert(Rejoined>=0 && Scene.Items.size()==2);
    assert(Scene.Items[Rejoined].Driven && Scene.Items[Rejoined].Fixed);
    CheckSurface(Scene.Items[Rejoined].Volume);
    std::cout << "関節動作：切りしろ無し分割・駆動中加工・切断時の拘束継承・速度継承・再接合を確認\n";

    // 爆発は球状の欠損を作り、切れた自由破片だけへ外向き速度を加える。
    Field BlastField; BlastField.Initialize(Point(45,45,45));
    const int BlastBefore=BlastField.MaterialCount();
    assert(BlastField.Blast(Point(35,0,0),20)>0);
    assert(BlastField.MaterialCount()<BlastBefore);
    assert(BlastField.Sample(Point(35,0,0))>0);
    assert(BlastField.Sample(Point(-20,0,0))<0);
    CheckSurface(BlastField);

    Scene.Reset(Connected,{Point(0,-40,80)});
    const int BlastSamples=Scene.Blast(Point(0,0,100),14,500,Changed);
    assert(BlastSamples>0);
    assert(Scene.Items.size()==2 && Changed.size()==2);
    int BlastFixed=-1, BlastFree=-1;
    for (size_t I=0;I<Scene.Items.size();++I)
    {
        CheckSurface(Scene.Items[I].Volume);
        if (Scene.Items[I].Fixed) BlastFixed=static_cast<int>(I);
        else BlastFree=static_cast<int>(I);
    }
    assert(BlastFixed>=0 && BlastFree>=0);
    assert(Scene.Items[BlastFixed].OriginVelocity.Length()<1e-9);
    assert(Scene.Items[BlastFree].OriginVelocity.Length()>400);
    assert(Scene.Items[BlastFree].OriginVelocity.Y>0);
    // 上限超過時は形状も速度も変更しない。
    Scene.Reset(Connected,{Point(0,-40,80)});
    const int BlastOriginalCount=Scene.Items[0].Volume.MaterialCount();
    assert(Scene.Blast(Point(0,0,100),14,500,Changed,1)==-1);
    assert(Changed.empty() && Scene.Items.size()==1);
    assert(Scene.Items[0].Volume.MaterialCount()==BlastOriginalCount);
    assert(Scene.Items[0].OriginVelocity.Length()<1e-9);
    std::cout << "爆発：球状破壊・自動分離・固定側維持・自由破片への爆風・上限時ロールバックを確認\n";

}
