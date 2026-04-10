#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
	vec3 hitPosition;
	vec3 hitNormal;
	vec3 hitTangent;
	vec3 hitColor;
	vec3 hitSpecularTint;
	float hitRoughness;
	float hitMetallic;
	float hitSpecular;
  
 	bool isHit;
	bool isLight;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayLoad;

void main()
{
    rayPayLoad.isHit = false;
}