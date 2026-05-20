/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

const float M_PI = 3.141592653589793238462;

// Tiny Encryption Algorithm
// By Fahad Zafar, Marc Olano and Aaron Curtis, see https://www.highperformancegraphics.org/previous/www_2010/media/GPUAlgorithms/HPG2010_GPUAlgorithms_Zafar.pdf
uint tea(uint val0, uint val1)
{
    uint sum = 0;
    uint v0 = val0;
    uint v1 = val1;
    for (uint n = 0; n < 16; n++)
    {
        sum += 0x9E3779B9;
        v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + sum) ^ ((v1 >> 5) + 0xC8013EA4);
        v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7E95761E);
    }
    return v0;
}

// Linear congruential generator based on the previous RNG state
// See https://en.wikipedia.org/wiki/Linear_congruential_generator
uint lcg(inout uint previous)
{
    const uint multiplier = 1664525u;
    const uint increment = 1013904223u;
    previous   = (multiplier * previous + increment);
    return previous & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint previous)
{
    return (float(lcg(previous)) / float(0x01000000));
}

vec2 rnd2(inout uint previous)
{
    return vec2(rnd(previous), rnd(previous));
}

float rndRange(inout uint previous, float minf, float maxf)
{
    return mix(minf, maxf, rnd(previous));
}

uint nrndRange(inout uint previous, uint minn, uint maxn)
{
    return uint(floor(rnd(previous) * (maxn - minn + 1)) + minn);
}

vec3 unitSph(inout uint previous)
{
    float a = rndRange(previous, 0, 2 * M_PI);
    float z = rndRange(previous, -1, 1);
    float r = sqrt(1 - z*z);
    return vec3(r*cos(a), r*sin(a), z);
}