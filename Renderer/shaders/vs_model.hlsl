#pragma pack_matrix( row_major )

#include "mvp.hlsli"


struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    int4 BI : BONE;
    float4 BW : WEIGHT;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 world_pos : POSITIONT;
   // float4 eye_pos : EYEPOS;
};



VSOut main(VSIn input)
{
    VSOut output;

    float4 skinned_pos = { 0.0f, 0.0f, 0.0f, 0.0f };
    float4 skinned_norm = { 0.0f, 0.0f, 0.0f, 0.0f };
    
   // input.pos.x = -input.pos.x;
   // input.normal.x = -input.normal.x;
    //input.uv.y = 1.0f - input.uv.y;
    
    for (int j = 0; j < 4; j++)
    {
        skinned_pos += mul(float4(input.pos.xyz, 1.0f), transforms[input.BI[j]]) * input.BW[j];
        skinned_norm += mul(float4(input.normal.xyz, 0.0f), transforms[input.BI[j]]) * input.BW[j];
    }

    //skinned_pos.x = -skinned_pos.x;
    //skinned_norm.x = -skinned_norm.x;
    
    //// SKINNED
    
    output.pos = float4(skinned_pos.xyz, 1.0f);
    output.pos = mul(output.pos, modeling);
    output.world_pos = output.pos;
    output.pos = mul(output.pos, view);
    output.pos = mul(output.pos, projection);
    output.normal = mul(float4(skinned_norm.xyz, 0.0f), modeling);
    
    input.uv.y = 1.0f - input.uv.y;
    output.uv = input.uv.xy;
    
    //NOT SKINNED

    //output.pos = float4(input.pos, 1.0f);
    //output.pos = mul(output.pos, modeling);
    //output.world_pos = output.pos;
    //output.pos = mul(output.pos, view);
    //output.pos = mul(output.pos, projection);
    //output.normal = mul(float4(input.normal.xyz, 0.0f), modeling);
    //output.uv = input.uv.xy;
    

    //float4x4 skinMat = transforms[input.BI.x] * input.BW.x;
    //skinMat += transforms[input.BI.y] * input.BW.y;
    //skinMat += transforms[input.BI.z] * input.BW.z;
    //skinMat += transforms[input.BI.w] * input.BW.w;
    //output.pos = mul(float4(input.pos, 1.0), skinMat);
    //output.normal = mul(float4(input.normal, 0.0), skinMat);
    //output.world_pos = output.pos;
    //output.pos = mul(output.pos, view);
    //output.pos = mul(output.pos, projection);
    //output.uv = input.uv.xy;

    return output;
}