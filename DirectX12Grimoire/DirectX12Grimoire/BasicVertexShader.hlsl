#include "BasicType.hlsli"
Texture2D<float4> tex : register(t0); //0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
SamplerState smp : register(s0); //0�ԃX���b�g�ɐݒ肳�ꂽ�T���v��

//�ϊ����܂Ƃ߂��\����
cbuffer cbuff0 : register(b0)
{
    matrix mat; //�ϊ��s��
};

//���_�V�F�[�_
BasicType BasicVS(float4 pos : POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT)
{
    BasicType output; //�s�N�Z���V�F�[�_�֓n���l
    //output.svpos = pos;
    output.svpos = mul(mat, pos);
    output.normal = normal;
    output.uv = uv;
    return output;
}
