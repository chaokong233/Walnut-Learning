/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

layout(push_constant) uniform BufferReferences {
	uint64_t vertices;
	uint64_t indices;
} bufferReferences;

struct Vertex
{
    vec3 position;
    float padding;
    vec3 normal;
    float padding2;
    vec3 tangent;
    float padding3;
    vec2 texcoord;
    vec2 padding4;
};

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices {uint i[]; };