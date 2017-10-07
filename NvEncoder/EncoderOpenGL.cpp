#include "EncoderOpenGL.h"
#include <assert.h>
#include <string>
#include "cuda.h"   // <cuda.h>
#include "cudaGL.h" // <cudaGL.h>

inline void OPENGL_THROW(CUresult code, const std::string& errorMessage)
{
	if (code != CUDA_SUCCESS)
	{
		throw std::runtime_error(errorMessage + " (Error " + std::to_string(code) + ")");
	}
}



/**
* @brief OpenGL-based encoder constructor. Assumes an existing OpenGL context.
* @param width
* @param height
* @param hevc
* @param bitrate
* @param ptxSearchPath
*/
EncoderOpenGL::EncoderOpenGL(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate)
#ifndef ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS
	: EncoderCUDA(width, height, hevc, bitrate)
#endif
{
#ifdef ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS
	// FIXME
	if (glewInit() != GLEW_OK)
		return;//throw std::runtime_error("GLEW initialization failed");
	Encoder::Init(NV_ENC_DEVICE_TYPE_OPENGL, nullptr, width, height, hevc, bitrate);
#else

#endif
}



/**
* @brief OpenGL-based encoder destructor.
*/
EncoderOpenGL::~EncoderOpenGL()
{
	for (auto& r : m_registeredPBOs)
		OPENGL_THROW(cuGraphicsUnregisterResource(r.second.graphicsResource),
			"Failed to unregister resource");

	for (auto& r : m_registeredTextures)
		OPENGL_THROW(cuGraphicsUnregisterResource(r.second.graphicsResource),
			"Failed to unregister resource");
}



/**
* @brief Encodes the given RGBA pixel buffer object (PBO).
* @param pbo
* @param width
* @param height
* @param iFrame
* @param buffer
*/
void EncoderOpenGL::EncodePBO(GLuint pbo, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
#ifdef ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS

	// Encoders with device type OpenGL can only read textures, not PBOs...
	assert(false);

#else

	// Check if PBO needs to be (re)registered
	RegisteredPBO& reg = m_registeredPBOs[pbo];

	const size_t pboSize = width * height * 4;
	if (reg.size != pboSize)
	{
		if (reg.graphicsResource)
		{
			OPENGL_THROW(cuGraphicsUnregisterResource(reg.graphicsResource),
				"Failed to unregister resource");

			reg.graphicsResource = nullptr;
		}

		OPENGL_THROW(cuGraphicsGLRegisterBuffer(&reg.graphicsResource, pbo, CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY),
			"Failed to register PBO as graphics resource");

		reg.size = pboSize;
	}

	// Map PBO
	OPENGL_THROW(cuGraphicsMapResources(1, &reg.graphicsResource, 0),
		"Failed to map PBO graphics resource");

	CUdeviceptr deviceBuffer;
	size_t numBytes = 0;
	OPENGL_THROW(cuGraphicsResourceGetMappedPointer(&deviceBuffer, &numBytes, reg.graphicsResource),
		"Failed to get mapped pointer to PBO graphics resource");

	// Encode
	Encode(deviceBuffer, width, height, iFrame, buffer);

	// Unmap PBO
	OPENGL_THROW(cuGraphicsUnmapResources(1, &reg.graphicsResource, 0),
		"Failed to unmap PBO graphics resource");

#endif
}


std::shared_ptr<NV_ENC_LOCK_BITSTREAM> EncoderOpenGL::EncodeFrame(GLuint texture, GLenum target, uint32_t width, uint32_t height, bool iFrame)
{
	// Check if texture needs to be (re)registered
	RegisteredTexture& reg = m_registeredTextures[texture];

	if (reg.width != width || reg.height != height)
	{
		if (reg.graphicsResource)
		{
			OPENGL_THROW(cuGraphicsUnregisterResource(reg.graphicsResource),
				"Failed to unregister resource");

			reg.graphicsResource = nullptr;
		}

		OPENGL_THROW(cuGraphicsGLRegisterImage(&reg.graphicsResource, texture, target, CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY),
			"Failed to register texture image as graphics resource");

		reg.width = width;
		reg.height = height;
	}

	// Map Texture
	OPENGL_THROW(cuGraphicsMapResources(1, &reg.graphicsResource, 0),
		"Failed to map texture image graphics resource");

	CUdeviceptr devicePtr;
	size_t sizeOfBuffer;
	OPENGL_THROW(cuGraphicsResourceGetMappedPointer(&devicePtr, &sizeOfBuffer, reg.graphicsResource),
		"Failed to get device pointer to texture image graphics resource");

	// 	CUarray textureArray;
	// 	OPENGL_THROW(cuGraphicsSubResourceGetMappedArray(&textureArray, reg.graphicsResource, 0, 0),
	// 		"Failed to get mapped array to texture image graphics resource");
	// 
	// 	// Encode
	// 	EncodeArray(textureArray, width, height, iFrame, buffer);

	// NOTE: What format is this texture?!
	auto result = Encoder::EncodeFrame(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, reg.graphicsResource, NV_ENC_BUFFER_FORMAT_ARGB, 1, width, height, iFrame);

	// Unmap texture
	OPENGL_THROW(cuGraphicsUnmapResources(1, &reg.graphicsResource, 0),
		"Failed to unmap texture image graphics resource");

	return result;
}

/**
* @brief Encodes the given OpenGL RGBA texture.
* @param texture
* @param target
* @param width
* @param height
* @param iFrame
* @param buffer
*/
void EncoderOpenGL::EncodeTexture(GLuint texture, GLenum target, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
#ifdef ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS

	NV_ENC_INPUT_RESOURCE_OPENGL_TEX resourceTex;
	resourceTex.texture = texture;
	resourceTex.target = target;
	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX, (void*)&resourceTex, NV_ENC_BUFFER_FORMAT_ABGR, width * 4, width, height, iFrame, buffer);

#else

	// Check if texture needs to be (re)registered
	RegisteredTexture& reg = m_registeredTextures[texture];

	if (reg.width != width || reg.height != height)
	{
		if (reg.graphicsResource)
		{
			OPENGL_THROW(cuGraphicsUnregisterResource(reg.graphicsResource),
				"Failed to unregister resource");

			reg.graphicsResource = nullptr;
		}

		OPENGL_THROW(cuGraphicsGLRegisterImage(&reg.graphicsResource, texture, target, CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY),
			"Failed to register texture image as graphics resource");

		reg.width = width;
		reg.height = height;
	}

	// Map Texture
	OPENGL_THROW(cuGraphicsMapResources(1, &reg.graphicsResource, 0),
		"Failed to map texture image graphics resource");

	CUdeviceptr devicePtr;
	size_t sizeOfBuffer;
	OPENGL_THROW(cuGraphicsResourceGetMappedPointer(&devicePtr, &sizeOfBuffer, reg.graphicsResource),
		"Failed to get device pointer to texture image graphics resource");

// 	CUarray textureArray;
// 	OPENGL_THROW(cuGraphicsSubResourceGetMappedArray(&textureArray, reg.graphicsResource, 0, 0),
// 		"Failed to get mapped array to texture image graphics resource");
// 
// 	// Encode
// 	EncodeArray(textureArray, width, height, iFrame, buffer);

	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, reg.graphicsResource, NV_ENC_BUFFER_FORMAT_ARGB, 1, width, height, iFrame, buffer);

	// Unmap texture
	OPENGL_THROW(cuGraphicsUnmapResources(1, &reg.graphicsResource, 0),
		"Failed to unmap texture image graphics resource");

#endif
}