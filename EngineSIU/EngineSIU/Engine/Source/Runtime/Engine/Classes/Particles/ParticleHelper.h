#pragma once
#include "Math/Vector.h"
#include "Math/Color.h"

class UParticleModuleRequired;
struct FBaseParticle
{
    // 24 bytes
    FVector		OldLocation;			// Last frame's location, used for collision
    FVector		Location;				// Current location

    // 16 bytes
    FVector		    BaseVelocity;			// Velocity = BaseVelocity at the start of each frame.
    float			Rotation;				// Rotation of particle (in Radians)

    // 16 bytes
    FVector		    Velocity;				// Current velocity, gets reset to BaseVelocity each frame to allow 
    float			BaseRotationRate;		// Initial angular velocity of particle (in Radians per second)

    // 16 bytes
    FVector		    BaseSize;				// Size = BaseSize at the start of each frame
    float			RotationRate;			// Current rotation rate, gets reset to BaseRotationRate each frame

    // 16 bytes
    FVector		    Size;					// Current size, gets reset to BaseSize each frame
    int32			Flags;					// Flags indicating various particle states

    // 16 bytes
    FLinearColor	Color;					// Current color of particle.

    // 16 bytes
    FLinearColor	BaseColor;				// Base color of the particle

    // 16 bytes
    float			RelativeTime;			// Relative time, range is 0 (==spawn) to 1 (==death)
    float			OneOverMaxLifetime;		// Reciprocal of lifetime
    float			Placeholder0;
    float			Placeholder1;
};

enum EParticleStates
{
    STATE_Particle_JustSpawned = 0x02000000,
    STATE_Particle_Freeze = 0x04000000,
    STATE_Particle_IgnoreCollisions = 0x08000000,
    STATE_Particle_FreezeTranslation = 0x10000000,
    STATE_Particle_FreezeRotation = 0x20000000,
    STATE_Particle_CollisionIgnoreCheck = STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation,
    STATE_Particle_DelayCollisions = 0x40000000,
    STATE_Particle_CollisionHasOccurred = 0x80000000,
    STATE_Mask = 0xFE000000,
    STATE_CounterMask = (~STATE_Mask)
};

enum EDynamicEmitterType
{
    DET_Unknown = 0,
    DET_Sprite,
    DET_Mesh,
    DET_Beam2,
    DET_Ribbon,
    DET_AnimTrail,
    DET_Custom
};

struct FParticleDataContainer
{
    int32 MemBlockSize;
    int32 ParticleDataNumBytes;
    int32 ParticleIndicesNumShorts;
    uint8* ParticleData;
    uint16* ParticleIndices;

    FParticleDataContainer()
        : MemBlockSize(0)
        , ParticleDataNumBytes(0)
        , ParticleIndicesNumShorts(0)
        , ParticleData(nullptr)
        , ParticleIndices(nullptr)
    {
    }
    ~FParticleDataContainer()
    {
        Free();
    }

    void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
    void Free();
};

struct FDynamicEmitterReplayDataBase
{
    EDynamicEmitterType eEmitterType;
    int32 ActiveParticleCount;
    int32 ParticleStride;
    FParticleDataContainer DataContainer;
    FVector Scale;
    uint32 SortMode;

    FDynamicEmitterReplayDataBase()
        : eEmitterType(DET_Unknown),
        ActiveParticleCount(0),
        ParticleStride(0),
        Scale(FVector(1.0f, 1.0f, 1.0f)),
        SortMode(0)
    {
    }
};

struct FDynamicEmitterDataBase
{
    FDynamicEmitterDataBase(const UParticleModuleRequired* RequiredModule);

    virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
    int32  EmitterIndex;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
    FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule) :
        FDynamicEmitterDataBase(RequiredModule)
    {
    }

    virtual ~FDynamicSpriteEmitterDataBase()
    {
    }
    void SortSpriteParticles() {};
};

class UMaterial;
struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
    UMaterial* Material = nullptr;
};

struct FDynamicSpriteEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
    FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule) :
        FDynamicSpriteEmitterDataBase(RequiredModule)
    {
    }

    ~FDynamicSpriteEmitterData()
    {
    }
    void Init() {}
    virtual const FDynamicEmitterReplayDataBase& GetSource() const override
    {
        return Source;
    }
    FDynamicSpriteEmitterReplayData Source;
};

struct FDynamicMeshEmitterReplayDataBase : FDynamicEmitterReplayDataBase
{
};

struct FDynamicMeshEmitterReplayData : public FDynamicMeshEmitterReplayDataBase
{
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
    void Init() {}
    virtual const FDynamicEmitterReplayDataBase& GetSource() const override
    {
        return Source;
    }
    FDynamicMeshEmitterReplayData Source;
};

#define DECLARE_PARTICLE_PTR(Name,Address)		\
	FBaseParticle* Name = (FBaseParticle*) (Address);

#define SPAWN_INIT																										\
	assert((Owner != NULL) && (Owner->Component != NULL));																\
	const int32		ActiveParticles	= Owner->ActiveParticles;															\
	const uint32	ParticleStride	= Owner->ParticleStride;															\
	uint32			CurrentOffset	= Offset;																			\
	FBaseParticle&	Particle		= *(ParticleBase);
