#include "ConstraintInstance.h"

#include "UserInterface/Console.h"

FConstraintProfileProperties::FConstraintProfileProperties()
    : bDisableCollision(true) // 관절 사이의 충돌 기본적으로 비활성화
{
}

FConstraintInstanceBase::FConstraintInstanceBase()
{
    Reset();
}

void FConstraintInstanceBase::Reset()
{
    ConstraintIndex = 0;
    PhysScene = nullptr;
}

FPhysScene* FConstraintInstanceBase::GetPhysScene() const
{
    return PhysScene;
}

const FPhysScene* FConstraintInstanceBase::GetPhysicsScene() const
{
    return PhysScene;
}

void FConstraintInstance::UpdateLinearLimit()
{
}

void FConstraintInstance::UpdateAngularLimit()
{
}

/*
 * 여기서 모든 PhysX 엔진의 Constraint 인스턴스 생성
 */
void FConstraintInstance::InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, float Scale, USkeletalMeshComponent* OwningComponent)
{

}


/** Constructor **/
FConstraintInstance::FConstraintInstance()
    : FConstraintInstanceBase()
    , LastKnownScale(1.f)
    , AngularRotationOffset(FRotator::ZeroRotator)
    , bScaleLinearLimits(true)
    , AverageMass(0.f)
{
    Pos1 = FVector(0.0f, 0.0f, 0.0f);
    PriAxis1 = FVector(1.0f, 0.0f, 0.0f);
    SecAxis1 = FVector(0.0f, 1.0f, 0.0f);

    Pos2 = FVector(0.0f, 0.0f, 0.0f);
    PriAxis2 = FVector(1.0f, 0.0f, 0.0f);
    SecAxis2 = FVector(0.0f, 1.0f, 0.0f);
}

void FConstraintInstance::CopyConstraintParamsFrom(const FConstraintInstance* FromInstance)
{
    if (FromInstance->PhysScene == nullptr)
    {
        UE_LOG(ELogLevel::Error, TEXT("FConstraintInst::CopyConstraintParamsFrom() : PhysScene is NULLPTR"));
    }
    *this = *FromInstance;
}

const FName& FConstraintInstance::GetChildBoneName() const
{
    return ConstraintBone1; 
}

const FName& FConstraintInstance::GetParentBoneName() const
{
    return ConstraintBone2;
}

float FConstraintInstance::GetLinearLimit() const
{
    return ProfileInstance.LinearLimit.Limit;
}




