#pragma once
#include "LightActor.h"


class APointLight : public ALight
{
    DECLARE_CLASS(APointLight, ALight)

public:
    APointLight();

protected:
    UPROPERTY(
        VisibleAnywhere,
        UPointLightComponent*, PointLightComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UBillboardComponent*, BillboardComponent, = nullptr;
    )
};

