#pragma once
#include "LightActor.h"
class ADirectionalLight : public ALight
{
    DECLARE_CLASS(ADirectionalLight, ALight)
    
public:
    ADirectionalLight();

public:
    void SetIntensity(float Intensity);

protected:
    UPROPERTY(
        VisibleAnywhere,
        UDirectionalLightComponent*, DirectionalLightComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UBillboardComponent*, BillboardComponent, = nullptr;
    )
};

