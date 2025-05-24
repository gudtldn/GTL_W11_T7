#pragma once
#include "Container/Array.h"

struct FKBoxElem;
struct FKSphereElem;
struct FKSphylElem;
struct FKConvexElem;

struct FKAggregateGeom
{
    TArray<FKSphereElem>   SphereElems;
    TArray<FKBoxElem>      BoxElems;
    TArray<FKSphylElem>    SphylElems;  // 캡슐
    TArray<FKConvexElem>   ConvexElems;

    int32 GetElementCount() const
    {
        return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num();
    }
};
