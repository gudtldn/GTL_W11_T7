#pragma once
#include "LightActor.h"


class ASpotLight : public ALight
{
    DECLARE_CLASS(ASpotLight, ALight)

public:
    ASpotLight();

protected:
    UPROPERTY(
        VisibleAnywhere,
        USpotLightComponent*, SpotLightComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UBillboardComponent*, BillboardComponent, = nullptr;
    )
};

