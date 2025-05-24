#include  "PhysicsConstraintTemplate.h"

void UPhysicsConstraintTemplate::Serialize(FArchive& Ar)
{
    FConstraintProfileProperties CurrentProfile = DefaultInstance.ProfileInstance;

    DefaultInstance.ProfileInstance = CurrentProfile;


}
