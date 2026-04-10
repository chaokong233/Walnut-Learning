/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

struct Triangle {
	Vertex vertices[3];
	vec3 normal;
	vec3 tangent;
	vec3 position;
	vec2 uv;
};

// This function will unpack our vertex buffer data into a single triangle and calculates uv coordinates
Triangle unpackTriangle(uint index, int vertexSize, uint geometryNodeIndex) {
	Triangle tri;
	const uint triIndex = index * 3;

	GeometryNode geometryNode = geometryNodes.nodes[geometryNodeIndex];

	Indices indices   = Indices(geometryNode.indexBufferDeviceAddress);
	Vertices vertices = Vertices(geometryNode.vertexBufferDeviceAddress);

	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	// We match vkglTF::Vertex: pos.xyz+normal.x, normalyz+uv.xy
	// glm::vec3 pos;
	// glm::vec3 normal;
	// glm::vec2 uv;
	// ...
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i];
		Vertex ver = vertices.v[offset];
		tri.vertices[i].position = ver.position;
		tri.vertices[i].normal = ver.normal;
		tri.vertices[i].tangent = ver.tangent;
		tri.vertices[i].texcoord = ver.texcoord;
	}
	// Calculate values at barycentric coordinates
	vec3 barycentricCoords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	tri.uv = tri.vertices[0].texcoord * barycentricCoords.x + tri.vertices[1].texcoord * barycentricCoords.y + tri.vertices[2].texcoord * barycentricCoords.z;
	tri.position = tri.vertices[0].position * barycentricCoords.x + tri.vertices[1].position * barycentricCoords.y + tri.vertices[2].position * barycentricCoords.z;
	tri.tangent = tri.vertices[0].tangent * barycentricCoords.x + tri.vertices[1].tangent * barycentricCoords.y + tri.vertices[2].tangent * barycentricCoords.z;
	tri.normal = tri.vertices[0].normal * barycentricCoords.x + tri.vertices[1].normal * barycentricCoords.y + tri.vertices[2].normal * barycentricCoords.z;
	return tri;
}