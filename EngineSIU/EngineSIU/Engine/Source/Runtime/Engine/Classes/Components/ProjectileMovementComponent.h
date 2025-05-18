#pragma once
#include"Components/SceneComponent.h"

class UProjectileMovementComponent : public USceneComponent
{
    DECLARE_CLASS(UProjectileMovementComponent, USceneComponent)
public:
    UProjectileMovementComponent();
    virtual ~UProjectileMovementComponent() override = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    void SetVelocity(FVector NewVelocity) { Velocity = NewVelocity; }

    FVector GetVelocity() const { return Velocity; }

    void SetInitialSpeed(float NewInitialSpeed) { InitialSpeed = NewInitialSpeed; }

    float GetInitialSpeed() const { return InitialSpeed; }

    void SetMaxSpeed(float NewMaxSpeed) { MaxSpeed = NewMaxSpeed; }

    float GetMaxSpeed() const { return MaxSpeed; }

    void SetGravity(float NewGravity) { Gravity = NewGravity; }

    float GetGravity() const { return Gravity; }

    void SetLifetime(float NewLifetime) { ProjectileLifetime = NewLifetime; }

    float GetLifetime() const { return ProjectileLifetime; }

    virtual void BeginPlay() override;
    
    virtual void TickComponent(float DeltaTime) override;
    
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

private:
    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        float, ProjectileLifetime // 생명주기
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        float, AccumulatedTime
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        float, InitialSpeed
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        float, MaxSpeed
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        float, Gravity
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        FVector, Velocity
    )
};

