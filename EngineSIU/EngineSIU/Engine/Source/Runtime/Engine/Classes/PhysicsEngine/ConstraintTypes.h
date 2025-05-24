#pragma once
#include "HAL/PlatformType.h"
#include "PhysicsCore/Chaos/ChaosEngineInterface.h"

struct FConstraintBaseParams
{
    FConstraintBaseParams();

    /* Soft constraint의 변수 */
    float Stiffness;
    float Damping;

    float Restitution;
    float ContactDistance;

    uint8 bSoftConstraint : 1;

};

/* 대부분의 물리 제약은 구면 또는 대칭 구간 가정
 * RigidBody Joint에선 전체 거리 제한이 동일한 경우 많음
 * PhysX의 D6Joint 구조에서 선형제한은 PxD6Motion::eLIMITED + 공통 Limit으로 처리됨
 */
struct FLinearConstraint : public FConstraintBaseParams
{
    FLinearConstraint();

    
    float Limit; /* 하나의 값으로 3개 축 각각에 동일하게 적용되는 거리 제한 값 */

    ELinearConstraintMotion XMotion;
    ELinearConstraintMotion YMotion;
    ELinearConstraintMotion ZMotion;
};

/* Swing1(Y축), Swing2(X축) 제한을 적용 (축은 회전 가능) */
struct FConeConstraint : public FConstraintBaseParams
{
    FConeConstraint();
    float Swing1LimitDegrees; 
    float Swing2LimitDegrees; 

    EAngularConstraintMotion Swing1Motion;
    EAngularConstraintMotion Swing2Motion;
};

/* Angular Roll Constraint
 * 의역: Twist(Z축 기준 회전) 제한 (축은 AngularRotationOffset로 회전 가능)
 */
struct FTwistConstraint : public FConstraintBaseParams
{
    FTwistConstraint();

    float TwistLimitDegrees; 
    EAngularConstraintMotion TwistMotion;
};
