#pragma once
#include <cuda_runtime_api.h>
#include <cuda.h>
#include <vector_types.h>
#include "nvEncodeAPI.h"
#include "encoder.h"

/**
* @brief Encoder for CUDA device memory and array input (uses current CUDA context in constructor).
*/
class EncoderCUDA : public Encoder
{
public:
	EncoderCUDA();

	EncoderCUDA(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate);
	virtual ~EncoderCUDA();

	void Encode(CUdeviceptr bufferRGBA, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);
	void EncodeArray(CUarray arrayRGBA, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

	std::shared_ptr<NV_ENC_LOCK_BITSTREAM> EncodeFrame(CUdeviceptr bufferRGBA, uint32_t width, uint32_t height, bool iFrame);


protected:
	

	EncoderCUDA() = default;

	virtual void Init(NV_ENC_DEVICE_TYPE deviceType, void* device, uint32_t width, uint32_t height, bool hevc, uint32_t bitrate) override;

	//	void ResizeNV12Buffer(uint32_t width, uint32_t height);

private:

	CUcontext		m_CUDAContext;

// 	CUmodule m_colorConversionModule = nullptr;
// 	CUfunction m_RGBAToNV12Kernel = nullptr;
// 	CUfunction m_RGBASurfaceToNV12Kernel = nullptr;

// 	struct
// 	{
// 		CUdeviceptr pointer = 0;
// 		uint32_t pitch = 0;
// 		uint32_t width = 0;
// 		uint32_t height = 0;
// 	} m_bufferNV12;
};