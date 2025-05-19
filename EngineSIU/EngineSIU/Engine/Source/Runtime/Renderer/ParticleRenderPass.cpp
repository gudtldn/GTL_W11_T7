// ParticleRenderPass.cpp (Instancing Version)
#include "ParticleRenderPass.h"

#include "EngineLoop.h"
#include "Particles/ParticleHelper.h"
#include "Engine/Engine.h"
#include "UnrealEd/EditorViewportClient.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "UnrealClient.h"
#include "RendererHelpers.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "UObject/UObjectIterator.h"

struct FSpriteVertex
{
    FVector2D UV;
};

struct FSpriteParticleInstance
{
    FVector Position;
    float RelativeTime;
    FVector OldPosition;
    float ParticleId;
    FVector2D Size;
    float Rotation;
    float SubImageIndex;
    FLinearColor Color;
};

FParticleRenderPass::FParticleRenderPass()
    : BufferManager(nullptr), Graphics(nullptr), ShaderManager(nullptr)
{
}

FParticleRenderPass::~FParticleRenderPass()
{
    ReleaseShader();
}

void FParticleRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;

    CreateShader();

    // Create static quad vertex buffer (6 vertices)
    TArray<FSpriteVertex> QuadVertices = {
        { {-0.5f, -0.5f} }, { {0.5f, -0.5f} }, { {0.5f, 0.5f} },
        { {-0.5f, -0.5f} }, { {0.5f, 0.5f} }, { {-0.5f, 0.5f} },
    };
    BufferManager->CreateVertexBuffer(TEXT("QuadSpriteVertex"), QuadVertices, QuadVertexInfo);
    BufferManager->CreateDynamicVertexBuffer(TEXT("Global_SpriteInstance"), sizeof(FSpriteParticleInstance) * 1000, InstanceInfoSprite);

}

void FParticleRenderPass::CreateShader()
{
    D3D11_INPUT_ELEMENT_DESC InputLayout[] = {
        { "TEXCOORD",        0, DXGI_FORMAT_R32G32_FLOAT,        0, 0,   D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "INSTANCE_POS",    0, DXGI_FORMAT_R32G32B32_FLOAT,     1, 0,   D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_TIME",   0, DXGI_FORMAT_R32_FLOAT,           1, 12,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_OLDPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT,     1, 16,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_ID",     0, DXGI_FORMAT_R32_FLOAT,           1, 28,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,        1, 32,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_ROT",    0, DXGI_FORMAT_R32_FLOAT,           1, 40,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_SUBUV",  0, DXGI_FORMAT_R32_FLOAT,           1, 44,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 48,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };

    ShaderManager->AddVertexShaderAndInputLayout(L"ParticleShader", L"Shaders/ParticleShader.hlsl", "mainVS", InputLayout, ARRAYSIZE(InputLayout));
    ShaderManager->AddPixelShader(L"ParticleShader", L"Shaders/ParticleShader.hlsl", "mainPS");
}

void FParticleRenderPass::ReleaseShader() {}

void FParticleRenderPass::PrepareRenderArr()
{
    ParticleComponents.Empty();
    for (auto Comp : TObjectRange<UParticleSystemComponent>())
    {
        if (Comp->GetWorld() == GEngine->ActiveWorld)
        {
            ParticleComponents.Add(Comp);
        }
    }
}

void FParticleRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11VertexShader* VS = ShaderManager->GetVertexShaderByKey(L"ParticleShader");
    ID3D11InputLayout* IL = ShaderManager->GetInputLayoutByKey(L"ParticleShader");
    ID3D11PixelShader* PS = ShaderManager->GetPixelShaderByKey(L"ParticleShader");

    Graphics->DeviceContext->VSSetShader(VS, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(IL);
    Graphics->DeviceContext->PSSetShader(PS, nullptr, 0);

    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RT = ViewportResource->GetRenderTarget(EResourceType::ERT_Scene);
    FDepthStencilRHI* DS = ViewportResource->GetDepthStencil(EResourceType::ERT_Scene);
    Graphics->DeviceContext->RSSetViewports(1, &ViewportResource->GetD3DViewport());
    Graphics->DeviceContext->OMSetRenderTargets(1, &RT->RTV, DS->DSV);
}
void FParticleRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRenderState(Viewport);

    for (UParticleSystemComponent* Comp : ParticleComponents)
    {
        if (!Comp || Comp->EmitterInstances.IsEmpty())
            continue;

        TArray<FSpriteParticleInstance> Instances;

        for (FParticleEmitterInstance* Emitter : Comp->EmitterInstances)
        {
            if (!Emitter || !Emitter->ParticleData || !Emitter->ParticleIndices)
                continue;

            const int32 Stride = Emitter->ParticleStride;
            const int32 ActiveCount = Emitter->ActiveParticles;

            for (int32 i = 0; i < ActiveCount; ++i)
            {
                const int32 ParticleIndex = Emitter->ParticleIndices[i];
                const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(Emitter->ParticleData + ParticleIndex * Stride);

                FSpriteParticleInstance Inst;
                Inst.Position = P->Location;
                Inst.OldPosition = P->OldLocation;
                Inst.RelativeTime = P->RelativeTime;
                Inst.ParticleId = static_cast<float>(i);
                Inst.Size = FVector2D(P->Size.X, P->Size.Y);
                Inst.Rotation = P->Rotation;
                Inst.SubImageIndex = 0.0f;
                Inst.Color = P->Color;
                Instances.Add(Inst);
            }
        }

        if (Instances.IsEmpty())
            continue;

        // Upload instance buffer
        BufferManager->UpdateDynamicVertexBuffer(TEXT("Global_SpriteInstance"), Instances);

        FObjectConstantBuffer ObjectData;
        FMatrix WorldMatrix = Comp->GetWorldMatrix();
        ObjectData.WorldMatrix = WorldMatrix;
        ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
        ObjectData.UUIDColor = Comp->EncodeUUID() / 255.0f;
        ObjectData.bIsSelected = false;
        BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);

        ID3D11Buffer* Buffers[2] = { QuadVertexInfo.VertexBuffer, InstanceInfoSprite.VertexBuffer };
        UINT Strides[2] = { sizeof(FSpriteVertex), sizeof(FSpriteParticleInstance) };
        UINT Offsets[2] = { 0, 0 };

        Graphics->DeviceContext->IASetVertexBuffers(0, 2, Buffers, Strides, Offsets);
        Graphics->DeviceContext->DrawInstanced(6, Instances.Num(), 0, 0);
    }

    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void FParticleRenderPass::ClearRenderArr()
{
    ParticleComponents.Empty();
}
