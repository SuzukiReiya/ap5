#pragma once

// UEに依存しない体積演算。負値が材料、正値が空間を表す。
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

namespace Ap5Volume
{
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

class Field
{
public:
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

    void Reset() { Values = Original; }

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
