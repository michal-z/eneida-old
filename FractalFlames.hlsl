#define RootSig \
    "RootConstants(b0, num32BitConstants = 2)"

struct PsData
{
    float4 position : SV_Position;
};

#if defined VS_TRANSFORM

struct CbData
{
    float2 position;
};
ConstantBuffer<CbData> s_Cb : register(b0);

[RootSignature(RootSig)]
PsData VsTransform()
{
    PsData output;
    output.position = float4(s_Cb.position, 0.0f, 1.0f);
    return output;
}

#elif defined PS_SHADE

[RootSignature(RootSig)]
float4 PsShade(PsData input) : SV_Target0
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

#endif
// vim: set ts=4 sw=4 expandtab:
