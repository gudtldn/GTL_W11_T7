#ifndef PARTICLE_SHADER
#define PARTICLE_SHADER

#include "ShaderRegisters.hlsl"

// ==================== Config ====================

#ifndef PARTICLE_MESH
#define PARTICLE_SPRITE
#endif

cbuffer ParticleSettings : register(b4)
{
    int SubUVCols;
    int SubUVRows;
};

// ==================== Vertex Structures ====================

#if defined(PARTICLE_SPRITE)
struct VS_INPUT
{
    float2 UV : TEXCOORD0; // Per-vertex (shared quad)

    // Instance data (per particle)
    float3 Position : INSTANCE_POS; // World-space center
    float RelativeTime : INSTANCE_TIME;
    float3 OldPosition : INSTANCE_OLDPOS;
    float ParticleId : INSTANCE_ID;
    float2 Size : INSTANCE_SIZE;
    float Rotation : INSTANCE_ROT;
    float SubImageIndex : INSTANCE_SUBUV;
    float4 Color : INSTANCE_COLOR;
};


#endif

#if defined(PARTICLE_MESH)
struct VS_INPUT
{
    float3 LocalPos       : POSITION;
    float4 Color          : COLOR0;
    float4 Transform0     : TEXCOORD0;
    float4 Transform1     : TEXCOORD1;
    float4 Transform2     : TEXCOORD2;
    float4 Velocity       : TEXCOORD3;
    int2   SubUVParams01  : TEXCOORD4;
    float2 SubUVTime      : TEXCOORD5; // .x = SubUVLerp, .y = RelativeTime
};
#endif

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

// ==================== Vertex Shader ====================

VS_OUTPUT mainVS(VS_INPUT input)
{
    VS_OUTPUT output;

#if defined(PARTICLE_SPRITE)
    float2 localOffset = input.UV * input.Size;

    float3 worldCenter = mul(float4(input.Position, 1.0f), WorldMatrix).xyz;

    // [1] 카메라 → 파티클 방향 계산 (정상적인 빌보딩)
    float3 toCamera = normalize(worldCenter - ViewWorldLocation);

    // [2] 월드 Up 벡터 (Unreal Z-Up)
    float3 worldUp = float3(0, 0, 1);

    // [3] 카메라 위치 기준 Right/Up 축 유도
    float3 right = normalize(cross(worldUp, toCamera));
    float3 up = -normalize(cross(toCamera, right));

    // [4] 쿼드 내 회전 적용
    float s = sin(input.Rotation);
    float c = cos(input.Rotation);
    float2 rotated = float2(
        localOffset.x * c - localOffset.y * s,
        localOffset.x * s + localOffset.y * c
    );

    // [5] 쿼드 오프셋 적용
    float3 offset = rotated.x * right + rotated.y * up;
    float3 worldPos = worldCenter + offset;

    // [6] 최종 변환
    float4 viewPos = mul(float4(worldPos, 1.0f), ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);
    output.WorldPos = worldPos;

    // [7] SubUV
    float2 frameSize = float2(1.0f / SubUVCols, 1.0f / SubUVRows);
    float2 UVOffset = float2(
        fmod(input.SubImageIndex, SubUVCols),
        floor(input.SubImageIndex / SubUVCols)
    ) * frameSize;
    output.UV = UVOffset + (input.UV + 0.5f) * frameSize;

    output.Color = input.Color;
#endif

#if defined(PARTICLE_MESH)
    float3 worldPos =
        input.LocalPos.x * input.Transform0.xyz +
        input.LocalPos.y * input.Transform1.xyz +
        input.LocalPos.z * input.Transform2.xyz +
        float3(input.Transform0.w, input.Transform1.w, input.Transform2.w);

    float4 viewPos = mul(float4(worldPos, 1.0f), ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);
    output.WorldPos = worldPos;

    output.Color = input.Color;
    output.UV = float2(0.0f, 0.0f);
#endif

    return output;
}



// ==================== Pixel Shader ====================

float4 mainPS(VS_OUTPUT input) : SV_TARGET
{
    float4 finalColor = input.Color;
    finalColor.rgb = LinearToSRGB(finalColor.rgb);

    // 디버그용 컬러 제거
     //finalColor.rgb = float3(1, 1, 0);

    return finalColor;
}

#endif // PARTICLE_SHADER
