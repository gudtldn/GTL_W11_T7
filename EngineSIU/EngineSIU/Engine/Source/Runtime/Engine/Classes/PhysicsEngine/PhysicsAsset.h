#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class USkeletalMesh;

class UPhysicsAsset : public UObject
{
    DECLARE_CLASS(UPhysicsAsset, UObject)
public:
    UPhysicsAsset() = default;

    USkeletalMesh* PreviewSkeletalMesh;

    TArray<FName> PhysicalAnimationProfiles;

    TArray<FName> ConstraintProfiles;

    FName CurrentPhysicalAnimationProfileName;

    FName CurrentConstraintProfileName;
};
