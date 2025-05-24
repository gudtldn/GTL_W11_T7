#pragma once
#include "Container/Array.h"

struct FKAggregateGeom
{
    TArray<FKSphereElem>   SphereElems;
    TArray<FKBoxElem>      BoxElems;
    TArray<FKSphylElem>    SphylElems;  // 캡슐
    TArray<FKConvexElem>   ConvexElems;
};
