#pragma once

#include <unordered_map>
#include "nvEncodeAPI.h"
#include "EncoderCUDA.h"

//TODO: Need another class for ENCODER_OPENGL_DIRECT_TEXTURE_ACCESS

/**
* @brief Encoder for OpenGL texture and PBO input. Assumes an existing OpenGL context.

  This version ONLY falls back to CUDA, and doesn't attempt to feed the textures directly to NVENC.

*/
class EncoderOpenGL : public EncoderCUDA
{
public:
	EncoderOpenGL();
	virtual ~EncoderOpenGL();

// 	void EncodePBO(GLuint pbo, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);
// 	void EncodeTexture(GLuint texture, GLenum target, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

	std::shared_ptr<NV_ENC_LOCK_BITSTREAM> EncodeFrame(unsigned int texture, unsigned int target, uint32_t width, uint32_t height, bool iFrame);

private:
	struct RegisteredPBO
	{
		size_t size = 0;
		CUgraphicsResource graphicsResource = nullptr;
	};
	std::unordered_map<unsigned int, RegisteredPBO> m_registeredPBOs;

	struct RegisteredTexture
	{
		uint32_t width = 0;
		uint32_t height = 0;
		CUgraphicsResource graphicsResource = nullptr;
	};
	std::unordered_map<unsigned int, RegisteredTexture> m_registeredTextures;

};