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
#include "Engine/FObjLoader.h"
#include "Engine/StaticMesh.h"
#include "Engine/Asset/StaticMeshAsset.h"
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
struct FMeshParticleInstance
{
    FMatrix Transform;      // 전체 4x4 행렬
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
    // === [1] Alpha Blend State 생성 ===
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Graphics->Device->CreateBlendState(&BlendDesc, &AlphaBlendState);

    // === [2] DepthStencil State (Z-Write Off) 생성 ===
    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = TRUE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // ✅ 깊이 기록 안함
    DepthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    Graphics->Device->CreateDepthStencilState(&DepthDesc, &NoZWriteState);

    // Create static quad vertex buffer (6 vertices)
    TArray<FSpriteVertex> QuadVertices = {
        { {-0.5f, -0.5f} }, { {0.5f, -0.5f} }, { {0.5f, 0.5f} },
        { {-0.5f, -0.5f} }, { {0.5f, 0.5f} }, { {-0.5f, 0.5f} },
    };
    BufferManager->CreateVertexBuffer(TEXT("QuadSpriteVertex"), QuadVertices, QuadVertexInfo);

    BufferManager->CreateDynamicVertexBuffer(
        TEXT("Global_SpriteInstance"), 
        sizeof(FSpriteParticleInstance) * 1000, 
        InstanceInfoSprite
    );
    BufferManager->CreateDynamicVertexBuffer(
        TEXT("Global_MeshInstance"),
        sizeof(FMeshParticleInstance) * 1000,
        InstanceInfoMesh
    );

}

void FParticleRenderPass::CreateShader()
{
    // Sprite용
    D3D_SHADER_MACRO DefinesSprite[] = {
        { "PARTICLE_SPRITE", "1" },
        { nullptr, nullptr }
    };
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
    ShaderManager->AddVertexShaderAndInputLayout(L"ParticleShader_Sprite", L"Shaders/ParticleShader.hlsl", "mainVS", InputLayout, ARRAYSIZE(InputLayout), DefinesSprite);
    ShaderManager->AddPixelShader(L"ParticleShader_Sprite", L"Shaders/ParticleShader.hlsl", "mainPS", DefinesSprite);

    // Mesh용
    D3D_SHADER_MACRO DefinesMesh[] = {
        { "PARTICLE_MESH", "1" },
        { nullptr, nullptr }
    };
    D3D11_INPUT_ELEMENT_DESC InputLayoutMesh[] = {
        { "POSITION",           0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 0,  D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",           0, DXGI_FORMAT_R32G32_FLOAT,        0, 56, D3D11_INPUT_PER_VERTEX_DATA,   0 }, // UV
        { "INSTANCE_TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT,  1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };


    ShaderManager->AddVertexShaderAndInputLayout(L"ParticleShader_Mesh", L"Shaders/ParticleShader.hlsl", "mainVS", InputLayoutMesh, ARRAYSIZE(InputLayoutMesh), DefinesMesh);
    ShaderManager->AddPixelShader(L"ParticleShader_Mesh", L"Shaders/ParticleShader.hlsl", "mainPS", DefinesMesh);
}


void FParticleRenderPass::ReleaseShader()
{
}

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

    /*ID3D11VertexShader* VS = ShaderManager->GetVertexShaderByKey(L"ParticleShader");
    ID3D11InputLayout* IL = ShaderManager->GetInputLayoutByKey(L"ParticleShader");
    ID3D11PixelShader* PS = ShaderManager->GetPixelShaderByKey(L"ParticleShader");

    Graphics->DeviceContext->VSSetShader(VS, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(IL);
    Graphics->DeviceContext->PSSetShader(PS, nullptr, 0);*/
    BufferManager->BindConstantBuffer(TEXT("FParticleSettingsConstants"), 4, EShaderStage::Vertex);

    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RT = ViewportResource->GetRenderTarget(EResourceType::ERT_Scene);
    FDepthStencilRHI* DS = ViewportResource->GetDepthStencil(EResourceType::ERT_Scene);
    Graphics->DeviceContext->RSSetViewports(1, &ViewportResource->GetD3DViewport());
    Graphics->DeviceContext->OMSetRenderTargets(1, &RT->RTV, DS->DSV);

    // ✅ BlendState, DepthStencilState 설정
    float BlendFactor[4] = { 0, 0, 0, 0 };
    Graphics->DeviceContext->OMSetBlendState(AlphaBlendState, BlendFactor, 0xffffffff);
    Graphics->DeviceContext->OMSetDepthStencilState(NoZWriteState, 0);
}
void FParticleRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRenderState(Viewport);

    for (UParticleSystemComponent* Comp : ParticleComponents)
    {
        if (!Comp || Comp->EmitterInstances.IsEmpty()) continue;

        for (FParticleEmitterInstance* Emitter : Comp->EmitterInstances)
        {
            if (!Emitter) continue;

            FDynamicEmitterDataBase* DynamicData = Emitter->GetDynamicData();
            if (!DynamicData) continue;

            const FDynamicEmitterReplayDataBase& ReplayData = DynamicData->GetSource();
            switch (ReplayData.eEmitterType)
            {
            case DET_Sprite:
                RenderSpriteEmitter(Viewport,Comp, Emitter, (const FDynamicSpriteEmitterReplayDataBase&)ReplayData);
                break;

            case DET_Mesh:
                RenderMeshEmitter(Comp, (FParticleMeshEmitterInstance*)Emitter, (const FDynamicMeshEmitterReplayDataBase&)ReplayData);
                break;

            default:
                break;
            }
        }
    }

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    Graphics->DeviceContext->OMSetDepthStencilState(nullptr, 0);
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}
void FParticleRenderPass::RenderSpriteEmitter(const std::shared_ptr<FEditorViewportClient>& Viewport,UParticleSystemComponent* Comp, FParticleEmitterInstance* Emitter, const FDynamicSpriteEmitterReplayDataBase& ReplayData)
{
    //TestTexture = FEngineLoop::ResourceManager.GetTexture(L"Assets/Texture/T_Explosion_SubUV.png");
    ID3D11VertexShader* VS = ShaderManager->GetVertexShaderByKey(L"ParticleShader_Sprite");
    ID3D11InputLayout* IL = ShaderManager->GetInputLayoutByKey(L"ParticleShader_Sprite");
    ID3D11PixelShader* PS = ShaderManager->GetPixelShaderByKey(L"ParticleShader_Sprite");

    Graphics->DeviceContext->VSSetShader(VS, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(IL);
    Graphics->DeviceContext->PSSetShader(PS, nullptr, 0);

    //TODO: 애니메이션 모듈 추가되면 지우기

    //blend 테스트용
    /*
    static float SubImageIndexTimer = 0.0f;
    static float SubImageOffset = 0;
    SubImageOffset += 0.05f;
    if (SubImageOffset >= 35.0f)SubImageOffset = 0.0f;*/
    //no blend 테스트용
    /*
    SubImageIndexTimer += 0.05f;
    if (SubImageIndexTimer >= 1.0f)
    {
        SubImageIndexTimer = 0.0f;
        SubImageOffset += 1.0f;
        if (SubImageOffset >= 35.0f)SubImageOffset = 0.0f;
    }*/
    // Set Object Constant Buffer
    FObjectConstantBuffer ObjectData;
    FMatrix WorldMatrix = Comp->GetWorldMatrix();
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = Comp->EncodeUUID() / 255.0f;
    ObjectData.bIsSelected = false;
    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);

    // SubUV 정보 바인딩
    FParticleSettingsConstants ParticleSettings;
    ParticleSettings.SubUVCols = ReplayData.SubImages_Horizontal;
    ParticleSettings.SubUVRows = ReplayData.SubImages_Vertical;
    int TotalFrames = ParticleSettings.SubUVCols * ParticleSettings.SubUVRows-1;
    BufferManager->UpdateConstantBuffer("FParticleSettingsConstants", ParticleSettings);

    //TArray<FSpriteParticleInstance> Instances;

    const int32 Stride = Emitter->ParticleStride;
    const int32 ActiveCount = Emitter->ActiveParticles;
    struct FSortableParticle
    {
        float DistanceToCamera;
        FSpriteParticleInstance Instance;

        bool operator<(const FSortableParticle& Other) const
        {
            return DistanceToCamera > Other.DistanceToCamera; // 뒤에서부터 그리기 (카메라 기준 먼 것부터)
        }
    };
    TArray<FSortableParticle> SortedParticles;

    FVector CameraPos = Viewport->PerspectiveCamera.GetLocation(); // 또는 Viewport->GetViewOrigin() 사용

    for (int32 i = 0; i < ActiveCount; ++i)
    {
        const int32 ParticleIndex = Emitter->ParticleIndices[i];
        const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(Emitter->ParticleData + ParticleIndex * Stride);

        //TODO: ParticleData에서 SubUV 정보 가져오기
        FSpriteParticleInstance Inst;
        Inst.Position = P->Location;
        Inst.OldPosition = P->OldLocation;
        Inst.RelativeTime = P->RelativeTime;
        Inst.ParticleId = static_cast<float>(i);
        Inst.Size = FVector2D(P->Size.X, P->Size.Y);
        Inst.Rotation = P->Rotation;
        Inst.SubImageIndex = P->RelativeTime * static_cast<float> (TotalFrames);
        //Inst.Color = FLinearColor(1,1,1,0.8f);
        Inst.Color = P->Color;
        //Instances.Add(Inst);

        float DistSq = FVector::DistSquared(CameraPos, P->Location);
        SortedParticles.Add({ DistSq, Inst });
    }
    SortedParticles.Sort();

    TArray<FSpriteParticleInstance> Instances;
    Instances.Reserve(SortedParticles.Num());

    for (const FSortableParticle& Sortable : SortedParticles)
    {
        Instances.Add(Sortable.Instance);
    }

    if (Instances.IsEmpty()) return;

    // Upload instance buffer
    BufferManager->UpdateDynamicVertexBuffer(TEXT("Global_SpriteInstance"), Instances);

    // Draw
    ID3D11Buffer* Buffers[2] = { QuadVertexInfo.VertexBuffer, InstanceInfoSprite.VertexBuffer };
    UINT Strides[2] = { sizeof(FSpriteVertex), sizeof(FSpriteParticleInstance) };
    UINT Offsets[2] = { 0, 0 };

    //Sprite니까 무조건 0번?
    if (!ReplayData.Material)return;
    std::shared_ptr<FTexture> Texture = 
        FEngineLoop::ResourceManager.GetTexture(ReplayData.Material->GetMaterialInfo().TextureInfos[0].TexturePath);

    ID3D11ShaderResourceView* SRVs[9] = {};
    ID3D11SamplerState* Samplers[9] = {};
    if (ReplayData.Material->GetMaterialInfo().TextureInfos.IsValidIndex(0))
    {
        const FWString& TexturePath = ReplayData.Material[0].GetMaterialInfo().TextureInfos[0].TexturePath;
        std::shared_ptr<FTexture> Texture = FEngineLoop::ResourceManager.GetTexture(TexturePath);
        if (Texture)
        {
            SRVs[0] = Texture->TextureSRV;
            Samplers[0] = Texture->SamplerState;
        }
    }

    if (Texture)
    {
        Graphics->DeviceContext->PSSetShaderResources(0, 1, &Texture->TextureSRV);
        Graphics->DeviceContext->PSSetSamplers(0, 1, &Texture->SamplerState);
    }

    Graphics->DeviceContext->IASetVertexBuffers(0, 2, Buffers, Strides, Offsets);
    Graphics->DeviceContext->DrawInstanced(6, Instances.Num(), 0, 0);
}
void FParticleRenderPass::RenderMeshEmitter(
    UParticleSystemComponent* Comp,
    FParticleMeshEmitterInstance* Emitter,
    const FDynamicMeshEmitterReplayDataBase& ReplayData)
{
    ID3D11VertexShader* VS = ShaderManager->GetVertexShaderByKey(L"ParticleShader_Mesh");
    ID3D11InputLayout* IL = ShaderManager->GetInputLayoutByKey(L"ParticleShader_Mesh");
    ID3D11PixelShader* PS = ShaderManager->GetPixelShaderByKey(L"ParticleShader_Mesh");

    Graphics->DeviceContext->VSSetShader(VS, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(IL);
    Graphics->DeviceContext->PSSetShader(PS, nullptr, 0);

    // [1] Mesh 및 RenderData 가져오기
    UStaticMesh* StaticMesh = ((FDynamicMeshEmitterData*)(Emitter->GetDynamicData()))->StaticMesh;
    if (!StaticMesh || !StaticMesh->GetRenderData()) return;

    FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
    if (RenderData->Vertices.IsEmpty() || RenderData->Indices.IsEmpty()) return;
    FString VertexBufferKey = FString::Printf(TEXT("ParticleMeshVertexBuffer_%s"), *StaticMesh->GetName());
    FString IndexBufferKey = FString::Printf(TEXT("ParticleMeshIndexBuffer_%s"), *StaticMesh->GetName());

    // [2] Vertex/Index Buffer 초기화
    BufferManager->CreateVertexBuffer(
        VertexBufferKey,
        RenderData->Vertices,
        StaticMeshVertexInfo
    );
    BufferManager->CreateIndexBuffer(
        IndexBufferKey,
        RenderData->Indices,
        StaticMeshIndexInfo
    );
    // [4-1] 정점/인스턴스 버퍼 바인딩
    ID3D11Buffer* Buffers[2] = { StaticMeshVertexInfo.VertexBuffer, InstanceInfoMesh.VertexBuffer };
    UINT Strides[2] = { sizeof(FStaticMeshVertex), sizeof(FMeshParticleInstance) };
    UINT Offsets[2] = { 0, 0 };

    Graphics->DeviceContext->IASetVertexBuffers(0, 2, Buffers, Strides, Offsets);

    // [4-2] 인덱스 버퍼 바인딩
    Graphics->DeviceContext->IASetIndexBuffer(StaticMeshIndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // [3] Instance 데이터 생성
    TArray<FMeshParticleInstance> Instances;
    const int32 Stride = Emitter->ParticleStride;
    const int32 ActiveCount = Emitter->ActiveParticles;

    for (int32 i = 0; i < ActiveCount; ++i)
    {
        const int32 ParticleIndex = Emitter->ParticleIndices[i];
        const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(Emitter->ParticleData + ParticleIndex * Stride);

        // [1] Scale + Rotation (Z축 회전)
        const FMatrix Location = FMatrix::CreateTranslationMatrix(P->Location);
        const FMatrix Scale = FMatrix::CreateScaleMatrix(P->Size);


        FMatrix ParticleTransform = Scale*Location;

        // [2] 위치는 Translation에 넣지 않음 — GPU에서 WorldMatrix 곱해줌
        // ParticleTransform.SetOrigin(P->Location); // 제거

        // [3] ParticleInstance 생성
        FMeshParticleInstance Inst;
        Inst.Transform = ParticleTransform;
        Inst.Color = P->Color;

        Instances.Add(Inst);
    }


    if (Instances.IsEmpty()) return;

    BufferManager->UpdateDynamicVertexBuffer(TEXT("Global_MeshInstance"), Instances);

    // [4] Constant Buffer
    FObjectConstantBuffer ObjectData;
    FMatrix WorldMatrix = Comp->GetWorldMatrix();
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = Comp->EncodeUUID() / 255.0f;
    ObjectData.bIsSelected = false;
    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);

    const TArray<FMaterialSubset>& Subsets = RenderData->MaterialSubsets;

    for (int32 SubMeshIndex = 0; SubMeshIndex < Subsets.Num(); ++SubMeshIndex)
    {
        const FMaterialSubset& Subset = Subsets[SubMeshIndex];
        const uint32 IndexCount = Subset.IndexCount;
        const uint32 StartIndex = Subset.IndexStart;

        if (StartIndex + IndexCount > RenderData->Indices.Num())
        {
            UE_LOG(ELogLevel::Error, TEXT("SubMesh %d: Invalid index range (%d + %d > %d)"),
                SubMeshIndex, StartIndex, IndexCount, RenderData->Indices.Num());
            continue; // 에러 발생 방지
        }

        // [2] 텍스처 바인딩
        if (RenderData->Materials.IsValidIndex(Subset.MaterialIndex))
        {
            const TArray<FTextureInfo>& TextureInfos = RenderData->Materials[Subset.MaterialIndex].TextureInfos;
            ID3D11ShaderResourceView* SRVs[9] = {};
            ID3D11SamplerState* Samplers[9] = {};

            if (TextureInfos.IsValidIndex(0))
            {
                const FWString& TexturePath = TextureInfos[0].TexturePath;
                std::shared_ptr<FTexture> Texture = FEngineLoop::ResourceManager.GetTexture(TexturePath);
                if (Texture)
                {
                    SRVs[0] = Texture->TextureSRV;
                    Samplers[0] = Texture->SamplerState;
                }
            }

            Graphics->DeviceContext->PSSetShaderResources(0, 9, SRVs);
            Graphics->DeviceContext->PSSetSamplers(0, 9, Samplers);
        }

        // [3] 인덱싱된 인스턴스 드로우
        const UINT StartIndexLocation = Subset.IndexStart;
        //const UINT IndexCount = Subset.IndexCount;

        Graphics->DeviceContext->DrawIndexedInstanced(
            IndexCount,
            Instances.Num(),
            StartIndexLocation,
            0,
            0
        );
    }
}

void FParticleRenderPass::ClearRenderArr()
{
    ParticleComponents.Empty();
}

void FParticleRenderPass::AddParticleComponent(UParticleSystemComponent* InParticleComponent)
{
    ParticleComponents.Add(InParticleComponent);
}
