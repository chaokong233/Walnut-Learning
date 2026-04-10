/*
Input Data:
LdotH
NdotV
NdotL
NdotH

OutPutData:
BRDF_Final_Attenuation
*/

// OPACITY -> Roughness
// REFLECTIVITY -> Metallic

// NdotV = max(0.0, NdotV);
// NdotH = max(0.0, NdotH);
// LdotH = max(0.0, LdotH);
// NdotL = max(0.0, NdotL);

float roughness =  clamp(rayPayLoad.hitRoughness, 0, 1);
float metallic = clamp(rayPayLoad.hitMetallic, 0, 1);
// float roughness = 0.2;
// float metallic = 1;
float specular = clamp(rayPayLoad.hitSpecular, 0, 1);
vec3 specularTint = clamp(rayPayLoad.hitSpecularTint, vec3(0), vec3(1));
vec3 Cdlin = clamp(rayPayLoad.hitColor, vec3(0), vec3(1));

// Color Context
float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.
vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint), Cdlin, metallic);

// Diffuse
float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness;
float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

// specular
float Ds = GTR2(NdotH, roughness);
float FH = SchlickFresnel(LdotH);
vec3 Fs = mix(Cspec0, vec3(1), FH);
float alphaG = sqr(0.5 + roughness / 2);
float Gs = smithG_GGX(NdotL, alphaG) * smithG_GGX(NdotV, alphaG);

vec3 BRDF_Final_Attenuation = (1/M_PI) * Fd*Cdlin * (1 - metallic)
                                + Gs*Fs*Ds;
// BRDF_Final_Attenuation = clamp(BRDF_Final_Attenuation, vec3(0), vec3(1));

//===============================================
#ifdef asdarqwr
float NdotL = dot(N,L);
float NdotV = dot(N,V);
if (NdotL < 0 || NdotV < 0) return vec3(0);

vec3 H = normalize(L+V);
float NdotH = dot(N,H);
float LdotH = dot(L,H);

vec3 Cdlin = mon2lin(baseColor);
float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint), Cdlin, metallic);
vec3 Csheen = mix(vec3(1), Ctint, sheenTint);

// Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
// and mix in diffuse retro-reflection based on roughness
float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness;
float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

// Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
// 1.25 scale is used to (roughly) preserve albedo
// Fss90 used to "flatten" retroreflection based on roughness
float Fss90 = LdotH*LdotH*roughness;
float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

// specular
float aspect = sqrt(1-anisotropic*.9);
float ax = max(.001, sqr(roughness)/aspect);
float ay = max(.001, sqr(roughness)*aspect);
float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
float FH = SchlickFresnel(LdotH);
vec3 Fs = mix(Cspec0, vec3(1), FH);
float Gs;
Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);

// sheen
vec3 Fsheen = FH * sheen * Csheen;

// clearcoat (ior = 1.5 -> F0 = 0.04)
float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss));
float Fr = mix(.04, 1.0, FH);
float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

return ((1/PI) * mix(Fd, ss, subsurface)*Cdlin + Fsheen)
    * (1-metallic)
    + Gs*Fs*Ds + .25*clearcoat*Gr*Fr*Dr;
#endif