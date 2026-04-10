#ifndef BRDF
#define BRDF

float sqr(float x) { return x*x; }

float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/M_PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (M_PI*log(a2)*t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (M_PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (M_PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

// MIS
// 使用GGX重要性采样在局部坐标系中采样微表面法线
// alpha = roughness，与GTR2/brdf.glsl保持一致
vec3 sample_ggx(vec2 xi, float roughness) {
    float a = roughness;          // 与GTR2(NdotH, roughness)一致，a = roughness
    float a2 = a * a;
    
    float phi = 2.0 * M_PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a2 - 1.0) * xi.y));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta)); // max(0,…)防止浮点误差导致根号内出现微小负值
    
    // 在局部坐标系中构建微表面法线
    vec3 h;
    h.x = sin_theta * cos(phi);
    h.y = sin_theta * sin(phi);
    h.z = cos_theta;
    
    return h;
}

float pdf_ggx(float NdotV, float NdotH, float VdotH, float roughness) {
    float a = roughness;          // 与GTR2(NdotH, roughness)一致，a = roughness
    float a2 = a * a;
    
    NdotV = max(0.0, NdotV);
    NdotH = max(0.0, NdotH);
    VdotH = max(1e-6, VdotH);    // 防止除零导致 +inf

    // D_GGX项（与GTR2相同的参数化）
    float d = a2 / (M_PI * pow(NdotH * NdotH * (a2 - 1.0) + 1.0, 2.0));
    
    // 转换为入射方向的PDF；下限1e-4防止极端MIS权重，同时保留比0.01更多的能量
    return max(1e-4, d * NdotH / (4.0 * VdotH));
}

#endif