#pragma once
#include "LightActor.h"


class AAmbientLight : public ALight
{
    DECLARE_CLASS(AAmbientLight, ALight)

public:
    AAmbientLight();

protected:
    UPROPERTY(
        VisibleAnywhere,
        UAmbientLightComponent*, AmbientLightComponent, = nullptr;
    )

    UPROPERTY(
        VisibleAnywhere,
        UBillboardComponent*, BillboardComponent, = nullptr;
    )
};
