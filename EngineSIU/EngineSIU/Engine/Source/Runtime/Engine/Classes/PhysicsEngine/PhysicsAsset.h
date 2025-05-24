#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UBodySetup;
class UPhysicsConstraintTemplate;
class USkeletalMesh;

class UPhysicsAsset : public UObject
{
    DECLARE_CLASS(UPhysicsAsset, UObject)
public:
    UPhysicsAsset() = default;

    const TArray<FName>& GetConstraintProfileNames() const;

    /* Graphics Bone 조작하는 Physics bone의 인덱스를 찾는 함수들 */
    int32 FindBodyIndex(FName bodyName) const;
    int32 FindConstraintIndex(FName ConstraintName) const;
    int32 FindConstraintIndex(FName Bone1Name, FName Bone2Name) const;

    USkeletalMesh* PreviewSkeletalMesh;

    TArray<FName> PhysicalAnimationProfiles;

    TArray<FName> ConstraintProfiles;

    FName CurrentPhysicalAnimationProfileName;

    FName CurrentConstraintProfileName;

    TArray<UBodySetup*> BodySetup;
    TArray<UPhysicsConstraintTemplate*> ConstraintSetup;

public:
    TMap<FName, int32>					BodySetupIndexMap;
};
