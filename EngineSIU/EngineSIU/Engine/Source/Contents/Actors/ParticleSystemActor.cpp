#include "ParticleSystemActor.h"

#include "Components/ParticleSystemComponent.h"

AParticleSystemActor::AParticleSystemActor()
{
    ParticleSystemComponent = AddComponent<UParticleSystemComponent>("ParticleSystemComponent_0");
    RootComponent = ParticleSystemComponent;

    ParticleSystemComponent->InitTestSpriteParticles(); // 이 함수에서 SpriteVertexData 채워줌

}

UParticleSystemComponent* AParticleSystemActor::GetParticleSystemComponent() const
{
    return ParticleSystemComponent;
}
