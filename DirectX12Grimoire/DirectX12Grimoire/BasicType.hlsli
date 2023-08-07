//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体

struct Output
{
    float4 svpos : SV_POSITION; //システム用頂点座標
    float2 uv : TEXCOORD; //UV値
};