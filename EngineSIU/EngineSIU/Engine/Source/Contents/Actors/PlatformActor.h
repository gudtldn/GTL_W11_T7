#pragma once
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"


class APlatformActor : public AActor
{
    DECLARE_CLASS(APlatformActor, AActor)

public:
    APlatformActor();
    virtual ~APlatformActor() override = default;

protected:
    UPROPERTY(
        VisibleAnywhere,
        UBoxComponent*, BoxComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UStaticMeshComponent*, MeshComponent, = nullptr;
    )
};
