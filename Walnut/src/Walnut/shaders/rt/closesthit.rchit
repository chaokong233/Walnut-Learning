#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

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
hitAttributeEXT vec2 attribs;

struct GeometryNode {
	uint64_t vertexBufferDeviceAddress;
	uint64_t indexBufferDeviceAddress;
  vec3 BaseColor;
      float padding;
  vec3 EmissiveColor;
      float padding1;
  vec3 SpecularTint;
  float Roughness;
  float Metallic;
  float Specular;
  float Subsurface;
  float Anisotropic;
  int BaseColorTextureID;
  int IBLTextureID; // r:roughness, g:metallic, b:specular 
};

layout(binding = 7, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;

layout(binding=9, set = 0) uniform sampler2D textures[26];

#include "bufferreferences.glsl"
#include "geometrytypes.glsl"

void main()
{
    GeometryNode geometryNode = geometryNodes.nodes[gl_GeometryIndexEXT];
    Triangle tri = unpackTriangle(gl_PrimitiveID, 80, gl_GeometryIndexEXT);
    
    rayPayLoad.isHit = true;
    rayPayLoad.hitPosition = tri.position;
    rayPayLoad.hitNormal = dot(gl_WorldRayDirectionEXT, tri.normal) <= 0 ? tri.normal : -tri.normal;
    rayPayLoad.hitTangent = tri.tangent;
    // Mat
    rayPayLoad.hitSpecularTint = geometryNode.SpecularTint;
      // IBL 
    int IBLTextureID = geometryNode.IBLTextureID;
    if(IBLTextureID < 0)
    {
        rayPayLoad.hitRoughness = geometryNode.Roughness;
        rayPayLoad.hitMetallic = geometryNode.Metallic;
        rayPayLoad.hitSpecular = geometryNode.Specular;
    }
    else
    {
        vec3 IBLColor = textureLod(textures[IBLTextureID], tri.uv, 0).xyz;
        rayPayLoad.hitRoughness = IBLColor.x;
        rayPayLoad.hitMetallic = IBLColor.y;
        rayPayLoad.hitSpecular = IBLColor.z;
    }

    vec3 emissive = geometryNode.EmissiveColor;
    // Emissive
    if(emissive.r > 1.1 || emissive.g > 1.1 || emissive.b > 1.1 )
    {
      rayPayLoad.hitColor = emissive;
      rayPayLoad.isLight = true;
    }
    // Bounce
    else{
      int BaseColorTextureID = geometryNode.BaseColorTextureID;
      if(BaseColorTextureID < 0)
        rayPayLoad.hitColor = geometryNode.BaseColor;
      else
        rayPayLoad.hitColor = textureLod(textures[BaseColorTextureID], tri.uv, 0).xyz;
    }
}
