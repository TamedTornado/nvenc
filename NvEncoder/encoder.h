// Copyright 2017 NVIDIA Corporation. All Rights Reserved.
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "nvEncodeAPI.h"
#include <memory>


// RGBA input is only available in SDK 6+ (otherwise we use manual CUDA conversion kernels)
#if NVENCAPI_MAJOR_VERSION >= 6
#define ENCODER_RGBA_INPUT
#endif

// Direct OpenGL texture input is only available in SDK 8+ on Linux (otherwise we use CUDA/GL mapping; no additional transfers needed)
// Note: The OpenGL device does not accept PBOs! Only enable this if you know what you're doing and really don't need PBO input.
// Note: Also, OpenGL device input does not work with EGL-based devices (yet).
#if !defined(_WIN32)
	#if NVENCAPI_MAJOR_VERSION >= 8
		#define ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS
	#endif
#endif



/**
 * @brief Shared base class for different encoder specializations.
 */
class Encoder
{
protected:
	Encoder(const Encoder&) = delete;

public:
	Encoder();
	virtual ~Encoder();

	enum CompressionBandwidth
	{
		COMPRESSION_BANDWIDTH_LOW,
		COMPRESSION_BANDWIDTH_DECREASE,
		COMPRESSION_BANDWIDTH_INCREASE
	};

	void SwitchRate(CompressionBandwidth bw);
	void SetRate(uint32_t bps);

	virtual void Init(NV_ENC_DEVICE_TYPE deviceType, void* device, uint32_t width, uint32_t height, bool hevc, uint32_t bitrate);
protected:
	void Encode(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

	std::shared_ptr<NV_ENC_LOCK_BITSTREAM> EncodeFrame(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height, bool iFrame);

private:
	void PrepareEncode(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height);
	void SetupEncoder(uint32_t bps);
	void CreateBitstream(uint32_t size);

private:
	void* m_nvencHandle;
	NV_ENCODE_API_FUNCTION_LIST m_nvencFuncs;
	NV_ENC_INITIALIZE_PARAMS m_nvencParams;
	NV_ENC_CONFIG m_nvencConfig;
	void* m_nvencEncoder;

	//TODO: Replace me!
	NV_ENC_OUTPUT_PTR m_bitstreamBuffer;
	NV_ENC_REGISTERED_PTR m_registeredResource;

	bool m_forceReinit = true;
	bool m_hevc;
};



