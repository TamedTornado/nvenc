// Copyright 2017 NVIDIA Corporation. All Rights Reserved.

#include "encoder.h"

#include <string>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "shared.h"

#ifndef WIN32
#include <dlfcn.h>
#endif

static const char* NvEncStatusStrings[] =
{
	"NV_ENC_SUCCESS",
	"NV_ENC_ERR_NO_ENCODE_DEVICE",
	"NV_ENC_ERR_UNSUPPORTED_DEVICE",
	"NV_ENC_ERR_INVALID_ENCODERDEVICE",
	"NV_ENC_ERR_INVALID_DEVICE",
	"NV_ENC_ERR_DEVICE_NOT_EXIST",
	"NV_ENC_ERR_INVALID_PTR",
	"NV_ENC_ERR_INVALID_EVENT",
	"NV_ENC_ERR_INVALID_PARAM",
	"NV_ENC_ERR_INVALID_CALL",
	"NV_ENC_ERR_OUT_OF_MEMORY",
	"NV_ENC_ERR_ENCODER_NOT_INITIALIZED",
	"NV_ENC_ERR_UNSUPPORTED_PARAM",
	"NV_ENC_ERR_LOCK_BUSY",
	"NV_ENC_ERR_NOT_ENOUGH_BUFFER",
	"NV_ENC_ERR_INVALID_VERSION",
	"NV_ENC_ERR_MAP_FAILED",
	"NV_ENC_ERR_NEED_MORE_INPUT",
	"NV_ENC_ERR_ENCODER_BUSY",
	"NV_ENC_ERR_EVENT_NOT_REGISTERD",
	"NV_ENC_ERR_GENERIC",
	"NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY",
	"NV_ENC_ERR_UNIMPLEMENTED",
	"NV_ENC_ERR_RESOURCE_REGISTER_FAILED",
	"NV_ENC_ERR_RESOURCE_NOT_REGISTERED",
	"NV_ENC_ERR_RESOURCE_NOT_MAPPED"
};

static const char* NvEncStringError(NVENCSTATUS err)
{
	if ((err >= 25) || (err < 0))
		return "unknown";
	return NvEncStatusStrings[err];
}

inline void NVENC_THROW(NVENCSTATUS code, const std::string& errorMessage)
{
	if (code != NV_ENC_SUCCESS)
	{
		throw std::runtime_error(errorMessage + " (Error " + std::to_string(code) + ": " + NvEncStringError(code) + ")");
	}
}

Encoder::Encoder():
	m_nvencHandle(nullptr),
	m_nvencFuncs(),
	m_nvencConfig(),
	m_nvencEncoder(nullptr),
	m_forceReinit(true),
	m_hevc(true)
{

}

/**
 * @brief Initializes the encode session and the conversion kernels.
 * @param deviceType
 * @param device
 * @param width
 * @param height
 * @param hevc
 * @param bitrate
 */
void Encoder::Init(NV_ENC_DEVICE_TYPE deviceType, void* device, uint32_t width, uint32_t height, bool hevc, uint32_t bitrate)
{
	assert(!m_nvencEncoder);
	m_hevc = hevc;

	// Initialize NvEncodeAPI
	m_nvencFuncs = { NV_ENCODE_API_FUNCTION_LIST_VER };

	typedef NVENCSTATUS(NVENCAPI *NvEncodeAPICreateInstance_Type)(NV_ENCODE_API_FUNCTION_LIST*);
#if defined(_WIN32)
	NvEncodeAPICreateInstance_Type NvEncodeAPICreateInstance = (NvEncodeAPICreateInstance_Type)(void *)(GetProcAddress(hEncodeDLL, "NvEncodeAPICreateInstance"));
#else
	NvEncodeAPICreateInstance_Type NvEncodeAPICreateInstance = (NvEncodeAPICreateInstance_Type)dlsym(hEncodeDLL, "NvEncodeAPICreateInstance");
#endif

	// FIXME
	if (!NvEncodeAPICreateInstance)
		return;//throw std::runtime_error("Cannot find NvEncodeAPICreateInstance() entry in nvEncodeAPI library");

	NVENC_THROW(NvEncodeAPICreateInstance(&m_nvencFuncs),
		"Failed to create encode API instance");

	static GUID cloudCarbonGuid = { 0x24f97f33, 0x8524, 0x463d,{ 0x82, 0x33, 0x7f, 0xe3, 0x42, 0x2e, 0x41, 0x3d } };

	// Create encoder
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openSessionExParams;
	memset(&openSessionExParams, 0, sizeof(openSessionExParams));
	openSessionExParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
	openSessionExParams.reserved = &cloudCarbonGuid;
	openSessionExParams.device = device;
	openSessionExParams.deviceType = deviceType;
	openSessionExParams.apiVersion = NVENCAPI_VERSION;

	NVENC_THROW(m_nvencFuncs.nvEncOpenEncodeSessionEx(&openSessionExParams, &m_nvencEncoder),
		"Failed to open encode session");

	memset(&m_nvencParams, 0, sizeof(m_nvencParams));
	m_nvencParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
	m_nvencParams.encodeGUID = hevc ? NV_ENC_CODEC_HEVC_GUID : NV_ENC_CODEC_H264_GUID;
	m_nvencParams.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
	m_nvencParams.encodeWidth = width;
	m_nvencParams.encodeHeight = height;
	m_nvencParams.darWidth = width;
	m_nvencParams.darHeight = height;
	m_nvencParams.maxEncodeWidth = 4096;
	m_nvencParams.maxEncodeHeight = 4096;
	m_nvencParams.frameRateNum = 90; // Target FPS
	m_nvencParams.frameRateDen = 1;
	m_nvencParams.encodeConfig = &m_nvencConfig;
	m_nvencParams.enablePTD = 1;

	SetupEncoder(bitrate);

	NVENC_THROW(m_nvencFuncs.nvEncInitializeEncoder(m_nvencEncoder, &m_nvencParams),
		"Failed to initialize encoder");

	CreateBitstream(width * height); // TODO: currently just a guess, GFN uses a fixed 1MB.
}



/**
 * @brief Base destructor.
 */
Encoder::~Encoder()
{
	if (!m_nvencEncoder)
		return;

	if (m_registeredResource)
		m_nvencFuncs.nvEncUnregisterResource(m_nvencEncoder, m_registeredResource);

	m_nvencFuncs.nvEncDestroyBitstreamBuffer(m_nvencEncoder, m_bitstreamBuffer);
	m_nvencFuncs.nvEncDestroyEncoder(m_nvencEncoder);
}





/**
 * @brief Base encode method which is called by the different specialized subclasses.
 * @param resourceType
 * @param resource
 * @param format
 * @param pitch
 * @param width
 * @param height
 * @param iFrame
 * @param buffer
 */
void Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
	// Preprocess input and resize (if necessary)
	PrepareEncode(resourceType, resource, format, pitch, width, height);

	NV_ENC_MAP_INPUT_RESOURCE mapInputResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };
	mapInputResource.registeredResource = m_registeredResource;

	NVENC_THROW(m_nvencFuncs.nvEncMapInputResource(m_nvencEncoder, &mapInputResource),
		"Failed to map input resource");

	// Do the encode
	NV_ENC_PIC_PARAMS picParams = {};
	picParams.version = NV_ENC_PIC_PARAMS_VER;
	picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	picParams.inputBuffer = mapInputResource.mappedResource;
	picParams.bufferFmt = format;
	//picParams.pictureType = NV_ENC_PIC_TYPE_P;
	picParams.inputWidth = width;
	picParams.inputHeight = height;
	picParams.outputBitstream = m_bitstreamBuffer;
	picParams.completionEvent = NULL;
	if (iFrame)
		picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;

	if (m_hevc)
	{
		NV_ENC_PIC_PARAMS_HEVC& hevcpicParams = picParams.codecPicParams.hevcPicParams;
		hevcpicParams.constrainedFrame = 1;
		hevcpicParams.sliceMode = 0;
		hevcpicParams.sliceModeData = 0;
	}

	NVENC_THROW(m_nvencFuncs.nvEncEncodePicture(m_nvencEncoder, &picParams),
		"Failed to encode picture");

	NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };

	lockBitstreamData.outputBitstream = m_bitstreamBuffer;
	lockBitstreamData.doNotWait = false;

	NVENC_THROW(m_nvencFuncs.nvEncLockBitstream(m_nvencEncoder, &lockBitstreamData),
		"Failed to lock bitstream");

	uint8_t *pData = (uint8_t*)lockBitstreamData.bitstreamBufferPtr;

	buffer.clear();
	buffer.insert(buffer.begin(), &pData[0], &pData[lockBitstreamData.bitstreamSizeInBytes]);

	NVENC_THROW(m_nvencFuncs.nvEncUnlockBitstream(m_nvencEncoder, lockBitstreamData.outputBitstream),
		"Failed to unlock bitstream");

	NVENC_THROW(m_nvencFuncs.nvEncUnmapInputResource(m_nvencEncoder, mapInputResource.mappedResource),
		"Failed to unmap input resource");
}



std::shared_ptr<NV_ENC_LOCK_BITSTREAM> Encoder::EncodeFrame(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height, bool iFrame)
{
	// Preprocess input and resize (if necessary)
	PrepareEncode(resourceType, resource, format, pitch, width, height);

	NV_ENC_MAP_INPUT_RESOURCE mapInputResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };
	mapInputResource.registeredResource = m_registeredResource;

	NVENC_THROW(m_nvencFuncs.nvEncMapInputResource(m_nvencEncoder, &mapInputResource),
		"Failed to map input resource");

	// Do the encode
	NV_ENC_PIC_PARAMS picParams = {};
	picParams.version = NV_ENC_PIC_PARAMS_VER;
	picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	picParams.inputBuffer = mapInputResource.mappedResource;
	picParams.bufferFmt = format;
	//picParams.pictureType = NV_ENC_PIC_TYPE_P;
	picParams.inputWidth = width;
	picParams.inputHeight = height;
	picParams.outputBitstream = m_bitstreamBuffer;
	picParams.completionEvent = NULL;
	if (iFrame)
		picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;

	if (m_hevc)
	{
		NV_ENC_PIC_PARAMS_HEVC& hevcpicParams = picParams.codecPicParams.hevcPicParams;
		hevcpicParams.constrainedFrame = 1;
		hevcpicParams.sliceMode = 0;
		hevcpicParams.sliceModeData = 0;
	}

	NVENC_THROW(m_nvencFuncs.nvEncEncodePicture(m_nvencEncoder, &picParams),
		"Failed to encode picture");

	auto lockBitstreamData = std::make_shared<NV_ENC_LOCK_BITSTREAM>(NV_ENC_LOCK_BITSTREAM{ NV_ENC_LOCK_BITSTREAM_VER });
//	NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };

	lockBitstreamData->outputBitstream = m_bitstreamBuffer;
	lockBitstreamData->doNotWait = false;

	NVENC_THROW(m_nvencFuncs.nvEncLockBitstream(m_nvencEncoder, lockBitstreamData.get()),
		"Failed to lock bitstream");

	// Now just return the struct, we don't unlock it.

	return lockBitstreamData;

// 	uint8_t *pData = (uint8_t*)lockBitstreamData.bitstreamBufferPtr;
// 
// 	buffer.clear();
// 	buffer.insert(buffer.begin(), &pData[0], &pData[lockBitstreamData.bitstreamSizeInBytes]);
// 
// 	NVENC_THROW(m_nvencFuncs.nvEncUnlockBitstream(m_nvencEncoder, lockBitstreamData.outputBitstream),
// 		"Failed to unlock bitstream");
// 
// 	NVENC_THROW(m_nvencFuncs.nvEncUnmapInputResource(m_nvencEncoder, mapInputResource.mappedResource),
// 		"Failed to unmap input resource");
}

/**
 * @brief Pre-encode operations such as resize and color conversion.
 * @param resourceType
 * @param resource
 * @param format
 * @param pitch
 * @param width
 * @param height
 * @return
 */
void Encoder::PrepareEncode(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resource, NV_ENC_BUFFER_FORMAT format, uint32_t pitch, uint32_t width, uint32_t height)
{
	if (m_forceReinit || (width != m_nvencParams.encodeWidth) || (height != m_nvencParams.encodeHeight))
	{
		// Reconfigure encoder and register resource
		m_nvencParams.encodeWidth = width;
		m_nvencParams.encodeHeight = height;
		m_nvencParams.darWidth = width;
		m_nvencParams.darHeight = height;

		NV_ENC_RECONFIGURE_PARAMS reInitEncodeParams;
		reInitEncodeParams.version = NV_ENC_RECONFIGURE_PARAMS_VER;
		reInitEncodeParams.forceIDR = true;
		reInitEncodeParams.resetEncoder = true;
		reInitEncodeParams.reInitEncodeParams = m_nvencParams;

		NVENC_THROW(m_nvencFuncs.nvEncReconfigureEncoder(m_nvencEncoder, &reInitEncodeParams),
			"Failed to reconfigure encoder");

		m_nvencFuncs.nvEncDestroyBitstreamBuffer(m_nvencEncoder, m_bitstreamBuffer);
		CreateBitstream(width * height);

		if (m_registeredResource)
		{
			NVENC_THROW(m_nvencFuncs.nvEncUnregisterResource(m_nvencEncoder, m_registeredResource),
				"Failed to unregister resource");

			m_registeredResource = nullptr;
		}

		NV_ENC_REGISTER_RESOURCE registerResource = { NV_ENC_REGISTER_RESOURCE_VER };
		registerResource.width = width;
		registerResource.height = height;
		registerResource.resourceType = resourceType;
		registerResource.resourceToRegister = resource;
		registerResource.bufferFormat = format;
		registerResource.pitch = pitch;

		NVENC_THROW(m_nvencFuncs.nvEncRegisterResource(m_nvencEncoder, &registerResource),
			"Failed to register resource");

		m_registeredResource = registerResource.registeredResource;

		m_forceReinit = false;
	}
}



/**
 * @brief Reconfigures the encoder for the given bitrate.
 * @param bps
 */
void Encoder::SetupEncoder(uint32_t bps)
{
	NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER,{ NV_ENC_CONFIG_VER } };
	m_nvencFuncs.nvEncGetEncodePresetConfig(m_nvencEncoder, m_nvencParams.encodeGUID, m_nvencParams.presetGUID, &presetConfig);
	m_nvencConfig = presetConfig.presetCfg;
	m_nvencConfig.frameIntervalP = 1;
	m_nvencConfig.gopLength = 1;
	m_nvencConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_nvencConfig.rcParams.maxBitRate = bps;
	m_nvencConfig.rcParams.averageBitRate = m_nvencConfig.rcParams.maxBitRate;
	m_nvencConfig.rcParams.vbvBufferSize = (uint32_t)(1.05f * bps * m_nvencParams.frameRateDen / m_nvencParams.frameRateNum);
	m_nvencConfig.rcParams.vbvInitialDelay = m_nvencConfig.rcParams.vbvBufferSize;
	m_nvencConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
	m_nvencConfig.frameIntervalP = 1;

	auto setVUIParameters = [](auto& vui)
	{
		vui.chromaSampleLocationFlag = 1;
		vui.chromaSampleLocationTop = 1;
		vui.chromaSampleLocationBot = 1;
		vui.videoSignalTypePresentFlag = 1;
		vui.videoFullRangeFlag = 1;
		vui.colourDescriptionPresentFlag = 1;
		vui.colourMatrix = 1;
		vui.colourPrimaries = 1;
		vui.transferCharacteristics = 1;
	};

	if (m_hevc)
	{
		NV_ENC_CONFIG_HEVC& hc = m_nvencConfig.encodeCodecConfig.hevcConfig;
		hc.repeatSPSPPS = 1;
		hc.chromaFormatIDC = 1;
		setVUIParameters(hc.hevcVUIParameters);
	}
	else
	{
		NV_ENC_CONFIG_H264& hc = m_nvencConfig.encodeCodecConfig.h264Config;
		hc.repeatSPSPPS = 1;
		hc.chromaFormatIDC = 1;
		setVUIParameters(hc.h264VUIParameters);
	}
}



/**
 * @brief Allocates an internal bitstream buffer of given size.
 * @param size
 */
void Encoder::CreateBitstream(uint32_t size)
{
	NV_ENC_CREATE_BITSTREAM_BUFFER createBitstreamBuffer = { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
	createBitstreamBuffer.size = (std::max)(size, m_nvencConfig.rcParams.vbvBufferSize);
	createBitstreamBuffer.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;

	NVENC_THROW(m_nvencFuncs.nvEncCreateBitstreamBuffer(m_nvencEncoder, &createBitstreamBuffer),
		"Failed to create bitstream buffer");

	m_bitstreamBuffer = createBitstreamBuffer.bitstreamBuffer;
}



/**
 * @brief Switches the compression bandwidths in discrete intervals.
 * @param bw
 */
void Encoder::SwitchRate(CompressionBandwidth bw)
{
	const uint32_t minRate = 8 * 1024 * 1024;

	uint32_t rate = 0;

	if (bw != COMPRESSION_BANDWIDTH_LOW)
	{
		rate = m_nvencConfig.rcParams.maxBitRate;
		uint32_t step = (std::min)(minRate, rate >> 1);

		if (bw == COMPRESSION_BANDWIDTH_INCREASE)
			rate += step;
		else
			rate -= step;
	}

	SetRate(rate);
}



/**
 * @brief Explicitly sets the encoding bitrate to an absolute value.
 * @param bps
 */
void Encoder::SetRate(uint32_t bps)
{
	const uint32_t minRate = 8 * 1024 * 1024;
	const uint32_t maxRate = 256 * 1024 * 1024;

	uint32_t rate = (std::min)((std::max)(bps, minRate), maxRate);

	if (rate != m_nvencConfig.rcParams.maxBitRate)
	{
		SetupEncoder(rate);
		m_forceReinit = true;
	}
}


