#pragma once
#include "ConstraintTypes.h"
#include "PhysScene.h"
#include "HAL/PlatformType.h"
#include "Math/Vector.h"
#include "UObject/NameTypes.h"

struct FConstraintProfileProperties
{
    FConstraintProfileProperties();

    /* R^3 상에서 거리 벡터 크기 제한 = 탈골 방지
     * 두 rigid body 사이의 기준점들이 서로 너무 멀어지지 않도록 제한
     */
    FLinearConstraint LinearLimit;

    FConeConstraint ConeLimit;

    FTwistConstraint TwistLimit;

    
    uint8 bDisableCollision : 1;        /* Constraint로 연결된 Bodies Collision 비활성화 여부 */
    uint8 bParentDominates : 1;         /* Child Motion에 Parent Body가 영향받을지 여부 */

    // [Projection] 기타 회전 오차, 선형 위치 오차 보정 - 작은 오차는 물리 보정, 큰 오차는 순간 이동으로 보정함
    // [Drive (스프링) ] 선형 / 회전 목표로 끌어당기는 제약
};

struct FConstraintInstanceBase
{
    FConstraintInstanceBase();
    void Reset();
    FPhysScene* GetPhysScene() const;
    const FPhysScene* GetPhysicsScene() const;

    /* Skeletal Mesh Component 배열 내의 인덱스 */
    int32 ConstraintIndex;

    FPhysScene* PhysScene;
};


struct FConstraintInstance : public FConstraintInstanceBase
{
public:
    FConstraintInstance();

    const FName& GetChildBoneName() const;
    const FName& GetParentBoneName() const;

    float GetLinearLimit() const;


    FName JointName;
    FName ConstraintBone1;
    FName ConstraintBone2;

    void UpdateLinearLimit();
    void UpdateAngularLimit();
    
private:
    float LastKnownScale; // 초기화 당시 컴포넌트의 Scale

public:

    FConstraintProfileProperties ProfileInstance;

    /* Reference Frame 기준 Body1 Constraint 위치 */
    FVector Pos1;

    /* Reference Frame 기준 Body1 Primary (twist) Axis*/
    FVector PriAxis1;

    /* Reference Frame 기준 Body1 Secondary Axis, 위와 직교함 */
    FVector SecAxis1;

    /* 이하 동일, Body2의 경우 주로 Skeletal Mesh의 Parent Body */
    FVector Pos2;
    FVector PriAxis2;
    FVector SecAxis2;

    /* Bone의 Local Z가 아닌 방향으로 twist를 제한하거나 */
    FRotator AngularRotationOffset;
    uint32 bScaleLinearLimits : 1;

    float AverageMass;
};
