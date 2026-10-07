#pragma once

// UEに依存しない体積演算。負値が材料、正値が空間を表す。
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

namespace Ap5Volume
{
// 水平床に対する落下だけを扱う。速度・距離はcm基準。
struct FallState
{
    double MinimumZ=0, OffsetZ=0, VelocityZ=0;
    bool Landed=false;
    bool Advance(double DeltaSeconds, double FloorZ)
    {
        if (Landed || DeltaSeconds<=0) return false;
        OffsetZ+=VelocityZ*DeltaSeconds-490*DeltaSeconds*DeltaSeconds;
        VelocityZ-=980*DeltaSeconds;
        if (OffsetZ+MinimumZ<=FloorZ)
        {
            OffsetZ=FloorZ-MinimumZ;
            VelocityZ=0;
            Landed=true;
            return true;
        }
        return false;
    }
};

struct Point
{
    double X, Y, Z;
    Point(double InX = 0, double InY = 0, double InZ = 0) : X(InX), Y(InY), Z(InZ) {}
    Point operator+(const Point& B) const { return Point(X+B.X, Y+B.Y, Z+B.Z); }
    Point operator-(const Point& B) const { return Point(X-B.X, Y-B.Y, Z-B.Z); }
    Point operator*(double S) const { return Point(X*S, Y*S, Z*S); }
    double Dot(const Point& B) const { return X*B.X + Y*B.Y + Z*B.Z; }
    Point Cross(const Point& B) const { return Point(Y*B.Z-Z*B.Y, Z*B.X-X*B.Z, X*B.Y-Y*B.X); }
    double Length() const { return std::sqrt(Dot(*this)); }
    Point Unit() const { const double L = Length(); return L > 1e-12 ? *this*(1/L) : Point(0,0,1); }
};

struct Triangle
{
    Point Vertices[3];
    Point Normals[3];
};

struct CollisionBox
{
    Point Center, Size;
};

struct Ellipsoid
{
    Point Center, Radii;
    Point AxisX = Point(1,0,0), AxisY = Point(0,1,0), AxisZ = Point(0,0,1);
    double Evaluate(const Point& P) const
    {
        const Point D = P-Center;
        return (Point(D.Dot(AxisX)/Radii.X,D.Dot(AxisY)/Radii.Y,D.Dot(AxisZ)/Radii.Z).Length()-1)
            *std::min(Radii.X,std::min(Radii.Y,Radii.Z));
    }
};

class Field
{
public:
    // 部位を同じ格子上で合成し、肩から手までのつながりも判定できるようにする。
    void InitializeUnion(const std::vector<Ellipsoid>& Shapes, double InSpacing=5)
    {
        if (Shapes.empty()) { *this=Field(); return; }
        Point Low(1e9,1e9,1e9), High(-1e9,-1e9,-1e9);
        for (const Ellipsoid& E : Shapes)
        {
            const Point Extent(
                std::abs(E.AxisX.X)*E.Radii.X+std::abs(E.AxisY.X)*E.Radii.Y+std::abs(E.AxisZ.X)*E.Radii.Z,
                std::abs(E.AxisX.Y)*E.Radii.X+std::abs(E.AxisY.Y)*E.Radii.Y+std::abs(E.AxisZ.Y)*E.Radii.Z,
                std::abs(E.AxisX.Z)*E.Radii.X+std::abs(E.AxisY.Z)*E.Radii.Y+std::abs(E.AxisZ.Z)*E.Radii.Z);
            Low=Point(std::min(Low.X,E.Center.X-Extent.X),std::min(Low.Y,E.Center.Y-Extent.Y),std::min(Low.Z,E.Center.Z-Extent.Z));
            High=Point(std::max(High.X,E.Center.X+Extent.X),std::max(High.Y,E.Center.Y+Extent.Y),std::max(High.Z,E.Center.Z+Extent.Z));
        }
        Spacing=InSpacing;
        Origin=Low-Point(Spacing,Spacing,Spacing);
        NX=static_cast<int>(std::ceil((High.X-Low.X)/Spacing))+3;
        NY=static_cast<int>(std::ceil((High.Y-Low.Y)/Spacing))+3;
        NZ=static_cast<int>(std::ceil((High.Z-Low.Z)/Spacing))+3;
        Values.resize(NX*NY*NZ);
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            double Value=1e9;
            for (const Ellipsoid& E : Shapes) Value=std::min(Value,E.Evaluate(Position(X,Y,Z)));
            Values[Index(X,Y,Z)]=Value;
        }
        Original=Values;
    }

    int MaterialCount() const
    {
        int Count=0;
        for (double V : Values) if (V<0) ++Count;
        return Count;
    }

    Point MaterialCentroid() const
    {
        Point Sum;
        int Count=0;
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            if (Values[Index(X,Y,Z)]>=0) continue;
            Sum=Sum+Position(X,Y,Z);
            ++Count;
        }
        return Count>0 ? Sum*(1.0/Count) : Point();
    }

    // 初期形状の最下段の材料を支持点にする。今回のモデルでは足裏付近に相当する。
    std::vector<Point> LowestMaterialPoints() const
    {
        std::vector<Point> Points;
        for (int Z=0;Z<NZ;++Z)
        {
            for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
                if (Values[Index(X,Y,Z)]<0) Points.push_back(Position(X,Y,Z));
            if (!Points.empty()) break;
        }
        return Points;
    }

    // 全頂点が材料の内側にあるセルだけを箱にし、空洞を埋めずに衝突形状を近似する。
    // X、Y、Zの順で隣接セルをまとめ、物理形状数を減らす。
    std::vector<CollisionBox> CollisionBoxes() const
    {
        std::vector<CollisionBox> Boxes;
        std::vector<unsigned char> Solid(Values.size(),0);
        for (int Z=0;Z<NZ-1;++Z) for (int Y=0;Y<NY-1;++Y) for (int X=0;X<NX-1;++X)
        {
            bool Inside=true;
            for (int DZ=0;DZ<2;++DZ) for (int DY=0;DY<2;++DY) for (int DX=0;DX<2;++DX)
                if (Values[Index(X+DX,Y+DY,Z+DZ)]>=0) Inside=false;
            if (Inside) Solid[Index(X,Y,Z)]=1;
        }
        for (int Z=0;Z<NZ-1;++Z) for (int Y=0;Y<NY-1;++Y) for (int X=0;X<NX-1;++X)
        {
            if (!Solid[Index(X,Y,Z)]) continue;
            int EndX=X+1, EndY=Y+1, EndZ=Z+1;
            while (EndX<NX-1 && Solid[Index(EndX,Y,Z)]) ++EndX;
            while (EndY<NY-1)
            {
                bool Full=true;
                for (int XX=X;XX<EndX;++XX) if (!Solid[Index(XX,EndY,Z)]) Full=false;
                if (!Full) break;
                ++EndY;
            }
            while (EndZ<NZ-1)
            {
                bool Full=true;
                for (int YY=Y;YY<EndY;++YY) for (int XX=X;XX<EndX;++XX)
                    if (!Solid[Index(XX,YY,EndZ)]) Full=false;
                if (!Full) break;
                ++EndZ;
            }
            CollisionBox Box;
            Box.Size=Point(EndX-X,EndY-Y,EndZ-Z)*Spacing;
            Box.Center=Position(X,Y,Z)+Box.Size*0.5;
            Boxes.push_back(Box);
            for (int ZZ=Z;ZZ<EndZ;++ZZ) for (int YY=Y;YY<EndY;++YY) for (int XX=X;XX<EndX;++XX)
                Solid[Index(XX,YY,ZZ)]=0;
        }
        // 完全なセルがない薄片は、材料が最も厚い格子点の小箱で最低限支える。
        if (Boxes.empty())
        {
            int Deepest=-1;
            for (int I=0;I<static_cast<int>(Values.size());++I)
                if (Values[I]<0 && (Deepest<0 || Values[I]<Values[Deepest])) Deepest=I;
            if (Deepest>=0)
            {
                CollisionBox Box;
                Box.Center=Position(Deepest%NX,(Deepest/NX)%NY,Deepest/(NX*NY));
                double Half=Spacing*0.25;
                for (int Trial=0;Trial<20;++Trial)
                {
                    bool Inside=true;
                    for (int Z=-1;Z<=1;Z+=2) for (int Y=-1;Y<=1;Y+=2) for (int X=-1;X<=1;X+=2)
                        if (Sample(Box.Center+Point(X,Y,Z)*Half)>=0) Inside=false;
                    if (Inside) break;
                    Half*=0.5;
                }
                Box.Size=Point(2*Half,2*Half,2*Half);
                Boxes.push_back(Box);
            }
        }
        return Boxes;
    }

    // 表面生成に使う四面体の辺に沿って探索する。斜めの細いつながりも維持する。
    std::vector<Field> Components(bool* ExceededLimit = nullptr, bool SplitOnly = false) const
    {
        if (ExceededLimit != nullptr) *ExceededLimit=false;
        std::vector<int> Labels(Values.size(),-1), Queue;
        int ComponentCount=0;
        const int Steps[7][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{1,0,1},{0,1,1},{1,1,1}};
        for (int Seed=0;Seed<static_cast<int>(Values.size());++Seed)
        {
            if (Values[Seed]>=0 || Labels[Seed]>=0) continue;
            Queue.clear(); Queue.push_back(Seed); Labels[Seed]=ComponentCount;
            for (size_t Head=0;Head<Queue.size();++Head)
            {
                const int I=Queue[Head], X=I%NX, Y=(I/NX)%NY, Z=I/(NX*NY);
                for (int S=0;S<7;++S) for (int Sign=-1;Sign<=1;Sign+=2)
                {
                    const int XX=X+Steps[S][0]*Sign, YY=Y+Steps[S][1]*Sign, ZZ=Z+Steps[S][2]*Sign;
                    if (XX<0 || XX>=NX || YY<0 || YY>=NY || ZZ<0 || ZZ>=NZ) continue;
                    const int J=Index(XX,YY,ZZ);
                    if (Values[J]<0 && Labels[J]<0) { Labels[J]=ComponentCount; Queue.push_back(J); }
                }
            }
            ++ComponentCount;
            // 極端に細かい加工で全格子のコピーが無制限に増えるのを防ぐ。
            if (ComponentCount>32)
            {
                if (ExceededLimit != nullptr) *ExceededLimit=true;
                return {};
            }
        }
        if (SplitOnly && ComponentCount<2) return {};
        std::vector<Field> Result;
        for (int C=0;C<ComponentCount;++C)
        {
            Field Piece=*this;
            for (size_t I=0;I<Values.size();++I)
                if (Values[I]<0 && Labels[I]!=C) Piece.Values[I]=-Values[I];
            // 分離時の形を修復の上限にして、別の破片の領域を再生させない。
            Piece.Original=Piece.Values;
            Result.push_back(std::move(Piece));
        }
        return Result;
    }

    // 薄い切りしろを持つ平面で両側を閉じ、さらに連結成分ごとに分離する。
    // 平面が材料を両側に分けないときは空配列を返し、元の体積は変えない。
    std::vector<Field> Cut(const Point& PlanePoint, const Point& PlaneNormal) const
    {
        if (PlaneNormal.Length()<1e-12) return {};
        const Point N=PlaneNormal.Unit();
        const double HalfGap=1.5;
        Field Sides[2]={*this,*this};
        int Counts[2]={0,0};
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            const int I=Index(X,Y,Z);
            const double D=(Position(X,Y,Z)-PlanePoint).Dot(N);
            Sides[0].Values[I]=std::max(Values[I],D+HalfGap);
            Sides[1].Values[I]=std::max(Values[I],-D+HalfGap);
            for (int S=0;S<2;++S) if (Sides[S].Values[I]<0) ++Counts[S];
        }
        if (Counts[0]==0 || Counts[1]==0) return {};
        std::vector<Field> Result;
        for (int S=0;S<2;++S)
        {
            std::vector<Field> Pieces=Sides[S].Components();
            if (Pieces.empty()) return {};
            for (Field& Piece : Pieces) Result.push_back(std::move(Piece));
        }
        return Result;
    }

    // 関節検証用。材料を平面の両側へ分けるが、切断のような切りしろは作らない。
    // 両側がそれぞれ一つの連結成分にならない場合は失敗として空配列を返す。
    std::vector<Field> Partition(const Point& PlanePoint, const Point& PlaneNormal) const
    {
        if (PlaneNormal.Length()<1e-12) return {};
        const Point N=PlaneNormal.Unit();
        Field Sides[2]={*this,*this};
        int Counts[2]={0,0};
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            const int I=Index(X,Y,Z);
            const double D=(Position(X,Y,Z)-PlanePoint).Dot(N);
            Sides[0].Values[I]=std::max(Values[I],D);
            Sides[1].Values[I]=std::max(Values[I],-D);
            for (int S=0;S<2;++S) if (Sides[S].Values[I]<0) ++Counts[S];
        }
        if (Counts[0]==0 || Counts[1]==0) return {};
        std::vector<Field> Result;
        for (int S=0;S<2;++S)
        {
            bool Exceeded=false;
            std::vector<Field> Parts=Sides[S].Components(&Exceeded);
            if (Exceeded || Parts.size()!=1) return {};
            Result.push_back(std::move(Parts[0]));
        }
        return Result;
    }

    void Initialize(const Point& Radii, double InSpacing = 5)
    {
        Spacing = InSpacing;
        NX = static_cast<int>(std::ceil(2*Radii.X/Spacing)) + 3;
        NY = static_cast<int>(std::ceil(2*Radii.Y/Spacing)) + 3;
        NZ = static_cast<int>(std::ceil(2*Radii.Z/Spacing)) + 3;
        Origin = Point(-(NX-1)*Spacing/2, -(NY-1)*Spacing/2, -(NZ-1)*Spacing/2);
        Original.resize(NX*NY*NZ);
        const double Scale = std::min(Radii.X, std::min(Radii.Y, Radii.Z));
        for (int Z=0; Z<NZ; ++Z) for (int Y=0; Y<NY; ++Y) for (int X=0; X<NX; ++X)
        {
            const Point P = Position(X,Y,Z);
            Original[Index(X,Y,Z)] = (Point(P.X/Radii.X,P.Y/Radii.Y,P.Z/Radii.Z).Length()-1)*Scale;
        }
        Values = Original;
    }

    // 同じ生成時座標の二つの塊を接合する。補うのは分離直前に存在し、
    // 現在の両方の材料に隣接する格子点だけ。広い欠損は再生しない。
    bool JoinAtSeam(const Field& Other,const Field& BeforeSplit,Field& Result) const
    {
        if (!SameGrid(Other) || !SameGrid(BeforeSplit) || MaterialCount()==0 || Other.MaterialCount()==0) return false;
        Field Merged=*this;
        for (size_t I=0;I<Values.size();++I) Merged.Values[I]=std::min(Values[I],Other.Values[I]);
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            const int I=Index(X,Y,Z);
            if (Merged.Values[I]<0 || BeforeSplit.Values[I]>=0) continue;
            bool NearThis=false, NearOther=false;
            for (int DZ=-1;DZ<=1;++DZ) for (int DY=-1;DY<=1;++DY) for (int DX=-1;DX<=1;++DX)
            {
                const int XX=X+DX, YY=Y+DY, ZZ=Z+DZ;
                if (XX<0 || XX>=NX || YY<0 || YY>=NY || ZZ<0 || ZZ>=NZ) continue;
                const int J=Index(XX,YY,ZZ);
                NearThis=NearThis || Values[J]<0;
                NearOther=NearOther || Other.Values[J]<0;
            }
            if (NearThis && NearOther) Merged.Values[I]=BeforeSplit.Values[I];
        }
        bool Exceeded=false;
        const std::vector<Field> Parts=Merged.Components(&Exceeded);
        if (Exceeded || Parts.size()!=1) return false;
        Merged.Original=Merged.Values;
        Result=std::move(Merged);
        return true;
    }

    void Reset() { Values = Original; }

    // 格子内の材料に最初に当たる距離。距離場は厳密な距離ではないため定間隔で探索する。
    bool Trace(const Point& Start,const Point& Direction,double MaximumDistance,double& Distance) const
    {
        if (Values.empty() || Direction.Length()<1e-12 || MaximumDistance<=0) return false;
        const Point Axis=Direction.Unit();
        const double S[3]={Start.X,Start.Y,Start.Z}, D[3]={Axis.X,Axis.Y,Axis.Z};
        const double Low[3]={Origin.X,Origin.Y,Origin.Z};
        const double High[3]={Origin.X+(NX-1)*Spacing,Origin.Y+(NY-1)*Spacing,Origin.Z+(NZ-1)*Spacing};
        double Enter=0, Exit=MaximumDistance;
        for (int K=0;K<3;++K)
        {
            if (std::abs(D[K])<1e-12)
            {
                if (S[K]<Low[K] || S[K]>High[K]) return false;
                continue;
            }
            double A=(Low[K]-S[K])/D[K], B=(High[K]-S[K])/D[K];
            if (B<A) std::swap(A,B);
            Enter=std::max(Enter,A); Exit=std::min(Exit,B);
            if (Enter>Exit) return false;
        }
        double Previous=Enter;
        for (double T=Enter; ; T=std::min(Exit,T+Spacing*0.2))
        {
            if (Sample(Start+Axis*T)<-1e-7)
            {
                double A=Previous, B=T;
                for (int I=0;I<16;++I)
                {
                    const double Middle=(A+B)*0.5;
                    if (Sample(Start+Axis*Middle)<0) B=Middle; else A=Middle;
                }
                Distance=B;
                return true;
            }
            if (T>=Exit) break;
            Previous=T;
        }
        return false;
    }

    // 球状の空間を材料から差し引く。爆発・局所欠損の基礎演算。
    int Blast(const Point& Center,double Radius)
    {
        if (Radius<=0) return 0;
        // 半径・爆心が格子と完全に対称でも等値面が格子点や辺へ厳密一致しないよう微小にずらす。
        // cm単位で1e-5以下なので見た目・寸法への影響は無視できる。
        const double EffectiveRadius=Radius+1e-6;
        const Point EffectiveCenter=Center+Point(1e-5,2e-5,3e-5);
        int Changed=0;
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            const int I=Index(X,Y,Z);
            const double Next=std::max(Values[I],EffectiveRadius-(Position(X,Y,Z)-EffectiveCenter).Length());
            if (Next-Values[I]>1e-9) { Values[I]=Next; ++Changed; }
        }
        return Changed;
    }

    // 浅い球面状のくぼみ。深さは射線方向、半径は平らな表面での入口の目安。
    int Dent(const Point& Hit,const Point& Direction,double Radius,double Depth)
    {
        if (Radius<=0 || Depth<=0 || Direction.Length()<1e-12) return 0;
        const double SphereRadius=(Radius*Radius+Depth*Depth)/(2*Depth);
        const Point Center=Hit-Direction.Unit()*(SphereRadius-Depth);
        int Changed=0;
        for (int Z=0;Z<NZ;++Z) for (int Y=0;Y<NY;++Y) for (int X=0;X<NX;++X)
        {
            const int I=Index(X,Y,Z);
            const double Next=std::max(Values[I],SphereRadius-(Position(X,Y,Z)-Center).Length());
            if (Next-Values[I]>1e-9) { Values[I]=Next; ++Changed; }
        }
        return Changed;
    }

    // 視線方向の半無限円柱。修復も現在の体積に対する加算で、履歴の巻き戻しではない。
    int Brush(const Point& Start, const Point& Direction, double Radius, bool Repair)
    {
        if (Direction.Length() < 1e-12 || Radius <= 0) return 0;
        const Point Axis = Direction.Unit();
        int Changed = 0;
        for (int Z=0; Z<NZ; ++Z) for (int Y=0; Y<NY; ++Y) for (int X=0; X<NX; ++X)
        {
            const int I = Index(X,Y,Z);
            const Point Delta = Position(X,Y,Z)-Start;
            const double Along = Delta.Dot(Axis);
            const double Cylinder = std::max((Delta-Axis*Along).Length()-Radius, -Along);
            // 元の体の外へ膨張させない。別の穴はブラシが届かなければ残る。
            const double Next = Repair
                ? std::max(Original[I], std::min(Values[I], Cylinder))
                : std::max(Values[I], -Cylinder);
            if (std::abs(Next-Values[I]) > 1e-9) { Values[I] = Next; ++Changed; }
        }
        return Changed;
    }

    double Sample(const Point& P) const
    {
        const double GX = std::max(0.0, std::min(static_cast<double>(NX-1), (P.X-Origin.X)/Spacing));
        const double GY = std::max(0.0, std::min(static_cast<double>(NY-1), (P.Y-Origin.Y)/Spacing));
        const double GZ = std::max(0.0, std::min(static_cast<double>(NZ-1), (P.Z-Origin.Z)/Spacing));
        const int X = std::min(NX-2, static_cast<int>(GX));
        const int Y = std::min(NY-2, static_cast<int>(GY));
        const int Z = std::min(NZ-2, static_cast<int>(GZ));
        double Result = 0;
        for (int DZ=0; DZ<2; ++DZ) for (int DY=0; DY<2; ++DY) for (int DX=0; DX<2; ++DX)
            Result += Values[Index(X+DX,Y+DY,Z+DZ)]
                * (DX ? GX-X : 1-(GX-X)) * (DY ? GY-Y : 1-(GY-Y)) * (DZ ? GZ-Z : 1-(GZ-Z));
        return Result;
    }

    std::vector<Triangle> Surface() const
    {
        std::vector<Triangle> Result;
        // 隣接セルと面の対角線が一致する6四面体分割。
        const int Corners[8][3] = {{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
        const int Tetrahedra[6][4] = {{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6},{0,5,1,6}};
        for (int Z=0; Z<NZ-1; ++Z) for (int Y=0; Y<NY-1; ++Y) for (int X=0; X<NX-1; ++X)
        {
            Point P[8]; double V[8]; int Negative = 0;
            for (int C=0; C<8; ++C)
            {
                P[C] = Position(X+Corners[C][0],Y+Corners[C][1],Z+Corners[C][2]);
                V[C] = Values[Index(X+Corners[C][0],Y+Corners[C][1],Z+Corners[C][2])];
                if (V[C] < 0) ++Negative;
            }
            if (Negative == 0 || Negative == 8) continue;
            for (int T=0; T<6; ++T)
            {
                int Inside[4], Outside[4], NI=0, NO=0;
                Point Inner, Outer;
                for (int K=0; K<4; ++K)
                {
                    const int C = Tetrahedra[T][K];
                    if (V[C]<0) { Inside[NI++]=C; Inner=Inner+P[C]; }
                    else { Outside[NO++]=C; Outer=Outer+P[C]; }
                }
                if (NI==0 || NO==0) continue;
                const Point Outward = Outer*(1.0/NO)-Inner*(1.0/NI);
                if (NI==1 || NO==1)
                {
                    const int A = NI==1 ? Inside[0] : Outside[0];
                    const int* Others = NI==1 ? Outside : Inside;
                    Emit(Result, Intersect(P,V,A,Others[0]), Intersect(P,V,A,Others[1]),
                        Intersect(P,V,A,Others[2]), Outward);
                }
                else
                {
                    const Point A = Intersect(P,V,Inside[0],Outside[0]);
                    const Point B = Intersect(P,V,Inside[0],Outside[1]);
                    const Point C = Intersect(P,V,Inside[1],Outside[1]);
                    const Point D = Intersect(P,V,Inside[1],Outside[0]);
                    Emit(Result,A,B,C,Outward);
                    Emit(Result,A,C,D,Outward);
                }
            }
        }
        return Result;
    }

private:
    bool SameGrid(const Field& Other) const
    {
        return NX==Other.NX && NY==Other.NY && NZ==Other.NZ
            && Spacing==Other.Spacing && (Origin-Other.Origin).Length()<1e-9;
    }

    int NX=0, NY=0, NZ=0;
    double Spacing=5;
    Point Origin;
    std::vector<double> Original, Values;
    int Index(int X,int Y,int Z) const { return X+NX*(Y+NY*Z); }
    Point Position(int X,int Y,int Z) const { return Origin+Point(X*Spacing,Y*Spacing,Z*Spacing); }
    static Point Intersect(const Point* P,const double* V,int A,int B)
    {
        return P[A]+(P[B]-P[A])*(V[A]/(V[A]-V[B]));
    }
    Point Normal(const Point& P) const
    {
        const double H = Spacing*0.25;
        return Point(Sample(P+Point(H,0,0))-Sample(P-Point(H,0,0)),
            Sample(P+Point(0,H,0))-Sample(P-Point(0,H,0)),
            Sample(P+Point(0,0,H))-Sample(P-Point(0,0,H))).Unit();
    }
    void Emit(std::vector<Triangle>& Result, Point A, Point B, Point C, const Point& Outward) const
    {
        const Point Cross = (B-A).Cross(C-A);
        if (Cross.Length()<1e-10) return;
        // UEの表面の頂点順に合わせる。法線は材料から空間へ向ける。
        if (Cross.Dot(Outward)>0) std::swap(B,C);
        Triangle Face = {{A,B,C},{Normal(A),Normal(B),Normal(C)}};
        Result.push_back(Face);
    }
};
}
