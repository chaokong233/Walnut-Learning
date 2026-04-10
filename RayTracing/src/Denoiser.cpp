#include "Denoiser.h"
#include "iostream"


Denoiser::Denoiser()
{
	int deviceCount = oidnGetNumPhysicalDevices();
	const char* n = oidnGetPhysicalDeviceString(0, "name");
	std::cout << deviceCount << std::endl;
	std::cout << "name: " << n << std::endl;
	// auto deviec = oidnNewDeviceByID(0);
	device_ = oidn::newDevice(oidn::DeviceType::CPU);
	device_.commit();
}

Denoiser::~Denoiser()
{

}

void Denoiser::reserve(uint32_t width, uint32_t height, bool isPreFilter, bool isHDR)
{
	isPreFilter_ = isPreFilter;
	width_ = width;
	height_ = height;

	// Create buffers for input/output images accessible by both host (CPU) and device (CPU/GPU)
	colorBuf_  = device_.newBuffer(width * height * 3 * sizeof(float));
	albedoBuf_  = device_.newBuffer(width * height * 3 * sizeof(float));
	normalBuf_  = device_.newBuffer(width * height * 3 * sizeof(float));

	// Create a filter for denoising a beauty (color) image using optional auxiliary images too
	// This can be an expensive operation, so try no to create a new filter for every image!
	filter_ = device_.newFilter("RT"); // generic ray tracing filter
	filter_.setImage("color",  colorBuf_,  oidn::Format::Float3, width, height); // beauty
	filter_.setImage("albedo", albedoBuf_, oidn::Format::Float3, width, height); // auxiliary
	filter_.setImage("normal", normalBuf_, oidn::Format::Float3, width, height); // auxiliary
	filter_.setImage("output", colorBuf_,  oidn::Format::Float3, width, height); // denoised beauty
	filter_.set("hdr", isHDR); // beauty image is HDR
	filter_.set("cleanAux", isPreFilter);
	filter_.set("quality", filterQuality_);
	filter_.commit();
	 
	if (isPreFilter)
	{
		// Create a separate filter for denoising an auxiliary albedo image (in-place)
		oidn::FilterRef albedoFilter = device_.newFilter("RT"); // same filter type as for beauty
		albedoFilter.setImage("albedo", albedoBuf_, oidn::Format::Float3, width, height);
		albedoFilter.setImage("output", albedoBuf_, oidn::Format::Float3, width, height);
		albedoFilter.commit();

		// Create a separate filter for denoising an auxiliary normal image (in-place)
		oidn::FilterRef normalFilter = device_.newFilter("RT"); // same filter type as for beauty
		normalFilter.setImage("normal", normalBuf_, oidn::Format::Float3, width, height);
		normalFilter.setImage("output", normalBuf_, oidn::Format::Float3, width, height);
		normalFilter.commit();
	}

}

float* Denoiser::GetInputPointer(Denoise_Image_Type type)
{
	switch (type)
	{
	case Denoise_Image_Type::color:
		return (float*)colorBuf_.getData();
		break;
	case Denoise_Image_Type::albedo:
		return (float*)albedoBuf_.getData();
		break;
	case Denoise_Image_Type::normal:
		return (float*)normalBuf_.getData();
		break;
	default:
		break;
	}
	
}

float* Denoiser::execute()
{
	if (isPreFilter_)
	{
		// Prefilter the auxiliary images
		albedoFilter_.execute();
		normalFilter_.execute();
	}

	// Filter the beauty image
	filter_.execute();

	// Check for errors
	const char* errorMessage;
	if (device_.getError(errorMessage) != oidn::Error::None)
	  std::cout << "Error: " << errorMessage << std::endl;

	return (float*)colorBuf_.getData();
}
