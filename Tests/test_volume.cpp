// 実際の体積演算をUEなしで検証する。描画・入力の確認はWindows側で別途行う。
#include "../Source/Ap5/Ap5Volume.h"
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
}
