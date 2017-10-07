#include "EncoderCUDA.h"
#include <string>
#include <assert.h>


inline void CUDA_THROW(CUresult code, const std::string& errorMessage)
{
	if (code != CUDA_SUCCESS)
	{
		throw std::runtime_error(errorMessage + " (Error " + std::to_string(code) + ")");
	}
}


/**
* @brief CUDA-based encoder constructor.
* @param width
* @param height
* @param hevc
* @param bitrate
* @param ptxSearchPath
*/
EncoderCUDA::EncoderCUDA(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate)
{
	// Make sure we have a CUDA context
	CUcontext context = nullptr;
	assert(cuCtxGetCurrent(&context) == CUDA_SUCCESS && context != nullptr);

	// Init CUDA-based encoder
	Encoder::Init(NV_ENC_DEVICE_TYPE_CUDA, nullptr, width, height, hevc, bitrate);

	// Initialize NV12 to RGBA conversion kernel
#if defined(_WIN32) || defined(_WIN64)
	std::string ptxFile = ptxSearchPath + "\\colorConversion.ptx";
#else
	std::string ptxFile = ptxSearchPath + "/colorConversion.ptx";
#endif
	
	// TODO: compile this CUDA properly as part of the project and link it in.
	CUDA_THROW(cuModuleLoad(&m_colorConversionModule, ptxFile.c_str()),
		"Failed to load module " + ptxFile);

	CUDA_THROW(cuModuleGetFunction(&m_RGBAToNV12Kernel, m_colorConversionModule, "RGBAToNV12"),
		"Failed to get RGBAToNV12 function in PTX module");

	CUDA_THROW(cuModuleGetFunction(&m_RGBASurfaceToNV12Kernel, m_colorConversionModule, "RGBASurfaceToNV12"),
		"Failed to get RGBASurfaceToNV12 function in PTX module");

	// Set BT.709 color conversion matrix
	float Kr = 0.2126f;
	float Kb = 0.0722f;

	float matrix[9];
	// Y
	matrix[0] = Kr;
	matrix[2] = Kb;
	matrix[1] = (1.0 - Kr - Kb);
	// U
	matrix[3] = (-0.5 * Kr / (1.0 - Kb));
	matrix[4] = -0.5f - matrix[3];
	matrix[5] = 0.5f;
	// V
	matrix[6] = 0.5f;
	matrix[8] = (-0.5 * Kb / (1.0 - Kr));
	matrix[7] = -0.5f - matrix[8];

	CUdeviceptr ptr;
	size_t size;

	CUDA_THROW(cuModuleGetGlobal(&ptr, &size, m_colorConversionModule, "colorMat"),
		"Failed to get color matrix address from color conversion module");

	CUDA_THROW(cuMemcpyHtoD(ptr, matrix, size),
		"Failed to set color matrix in color conversion module");
}



/**
* @brief CUDA-based encoder destructor.
*/
EncoderCUDA::~EncoderCUDA()
{
	if (m_bufferNV12.pointer)
		cuMemFree(m_bufferNV12.pointer);

	if (m_colorConversionModule)
		cuModuleUnload(m_colorConversionModule);
}



/**
* @brief Encodes the given linear RGBA buffer in device memory.
* @param bufferRGBA
* @param width
* @param height
* @param iFrame
* @param buffer
*/
void EncoderCUDA::Encode(CUdeviceptr bufferRGBA, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
#ifdef ENCODER_RGBA_INPUT

	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)bufferRGBA, NV_ENC_BUFFER_FORMAT_ABGR, width * 4, width, height, iFrame, buffer);

#else

	ResizeNV12Buffer(width, height);

	dim3 block(32, 16, 1);
	dim3 grid((width + (block.x - 1)) / (block.x), (height + (block.y - 1)) / block.y, 1);

	CUdeviceptr dst_y = m_bufferNV12.pointer;
	CUdeviceptr dst_uv = dst_y + height * m_bufferNV12.pitch;

	void *args[] = { &bufferRGBA, &width, &height, &dst_y, &dst_uv, &m_bufferNV12.pitch };
	CUDA_THROW(cuLaunchKernel(m_RGBAToNV12Kernel,
		grid.x, grid.y, grid.z,
		block.x, block.y, block.z,
		0, 0, args, NULL),
		"Failed to launch RGBA to NV12 conversion kernel");

	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)m_bufferNV12.pointer, NV_ENC_BUFFER_FORMAT_NV12, m_bufferNV12.pitch, width, height, iFrame, buffer);

#endif
}



/**
* @brief Encodes the given RGBA array.
* @param arrayRGBA
* @param width
* @param height
* @param iFrame
* @param buffer
*/
void EncoderCUDA::EncodeArray(CUarray arrayRGBA, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
	// NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY *always* needs to be converted to linear NV12,
	// since NvEncodeAPI does not support cuda arrays as input

	ResizeNV12Buffer(width, height);

	CUDA_RESOURCE_DESC resourceDesc;
	resourceDesc.resType = CU_RESOURCE_TYPE_ARRAY;
	resourceDesc.res.array.hArray = arrayRGBA;
	resourceDesc.flags = 0;

	CUsurfObject surfaceObject;
	CUDA_THROW(cuSurfObjectCreate(&surfaceObject, &resourceDesc),
		"Failed to create surface object");

	dim3 block(32, 16, 1);
	dim3 grid((width + (block.x - 1)) / (block.x), (height + (block.y - 1)) / block.y, 1);

	CUdeviceptr dst_y = m_bufferNV12.pointer;
	CUdeviceptr dst_uv = dst_y + height * m_bufferNV12.pitch;

	void *args[] = { &surfaceObject, &width, &height, &dst_y, &dst_uv, &m_bufferNV12.pitch };
	CUDA_THROW(cuLaunchKernel(m_RGBASurfaceToNV12Kernel,
		grid.x, grid.y, grid.z,
		block.x, block.y, block.z,
		0, 0, args, NULL),
		"Failed to launch RGBA array to NV12 conversion kernel");

	CUDA_THROW(cuSurfObjectDestroy(surfaceObject),
		"Failed to destroy surface object");

	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY, (void*)m_bufferNV12.pointer, NV_ENC_BUFFER_FORMAT_NV12, m_bufferNV12.pitch, width, height, iFrame, buffer);
}



/**
* @brief Ensures the internal NV12 buffer is correctly sized.
* @param width
* @param height
*/
void EncoderCUDA::ResizeNV12Buffer(uint32_t width, uint32_t height)
{
	if (m_bufferNV12.width != width || m_bufferNV12.height != height)
	{
		if (m_bufferNV12.pointer)
			cuMemFree(m_bufferNV12.pointer);

		// Why are we allocating a 16 byte element size pitched buffer here? 
		size_t pitch;
		CUDA_THROW(cuMemAllocPitch(&m_bufferNV12.pointer, &pitch, width, height * 3 / 2, 16),
			"Failed to allocate internal pitched NV12 buffer");

		m_bufferNV12.pitch = (uint32_t)pitch;
		m_bufferNV12.width = width;
		m_bufferNV12.height = height;
	}
}