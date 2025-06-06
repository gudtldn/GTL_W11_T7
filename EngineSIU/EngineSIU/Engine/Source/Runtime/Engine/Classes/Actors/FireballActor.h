#pragma once
#include"GameFramework/Actor.h"

class UProjectileMovementComponent;
class UPointLightComponent;
class USphereComp;

class AFireballActor : public AActor
{
    DECLARE_CLASS(AFireballActor, AActor)

public:
    AFireballActor();

    virtual void BeginPlay() override;

protected:
    UPROPERTY(
        VisibleAnywhere,
        UProjectileMovementComponent*, ProjectileMovementComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UPointLightComponent*, PointLightComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        USphereComp*, SphereComp, = nullptr;
    )
};
