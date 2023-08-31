#include "BasicType.hlsli"
Texture2D<float4> tex : register(t0); //0番スロットに設定されたテクスチャ
SamplerState smp : register(s0); //0番スロットに設定されたサンプラ 

//struct Input
//{
//    float4 pos : POSITION;
//    float4 svpos : SV_POSITION;
//};

float4 BasicPS(BasicType input) : SV_TARGET
{
    //return float4(tex.Sample(smp, input.uv));
    return float4(0, 0, 0, 1);
}