#include "BasicType.hlsli"

struct Input
{
    float4 pos : POSITION;
    float4 svpos : SV_POSITION;
};

float4 BasicPS(Output input) : SV_TARGET
{
    return float4(input.uv, 1, 1);
}