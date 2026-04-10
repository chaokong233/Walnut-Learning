#pragma once

#include "oidn.hpp"

enum class Denoise_Image_Type
{
	color, albedo, normal
};

class Denoiser
{
public:
	Denoiser();
	~Denoiser();

	void reserve(uint32_t width, uint32_t height, bool isPreFilter = false, bool isHDR = false);
	float* GetInputPointer(Denoise_Image_Type type);
	float* execute();

private:
	oidn::DeviceRef device_;
	uint32_t width_;
	uint32_t height_;

	oidn::BufferRef colorBuf_;
	oidn::BufferRef albedoBuf_;
	oidn::BufferRef normalBuf_;

	oidn::FilterRef filter_;
	oidn::FilterRef albedoFilter_;
	oidn::FilterRef normalFilter_;

	bool isPreFilter_{ false };
	OIDNQuality filterQuality_{ OIDN_QUALITY_HIGH };
};
