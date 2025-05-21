#pragma once
#include "SubEngine.h"

class UParticleSystemComponent;
class UParticleSubEngine : public USubEngine
{
    DECLARE_CLASS(UParticleSubEngine, USubEngine)
public:
    UParticleSubEngine();
    ~UParticleSubEngine();
public:
    virtual void Initialize(HWND& hWnd, FGraphicsDevice* InGraphics, FDXDBufferManager* InBufferManager, UImGuiManager* InSubWindow, UnrealEd* InUnrealEd);
    virtual void Tick(float DeltaTime);
    virtual void Input(float DeltaTime);
    virtual void Render();
    virtual void Release();
public:
    UParticleSystemComponent* GetParticleSystemComponent() const;
private:
    UParticleSystemComponent* ParticleSystemComponent = nullptr;
private:
    const float CameraSpeedMultiplier = 10;
};

