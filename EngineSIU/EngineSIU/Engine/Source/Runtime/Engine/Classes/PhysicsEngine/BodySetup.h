#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UBodySetup : public UObject
{
    DECLARE_CLASS(UBodySetup, UObject)
public:
    UBodySetup() = default;

    /* From UBodySetupCore */
    FName BoneName; 

    struct FKAggregateGeom AggGeom;
};
