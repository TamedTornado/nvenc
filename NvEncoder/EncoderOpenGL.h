#pragma once

#include <unordered_map>
#include "nvEncodeAPI.h"
#include "EncoderCUDA.h"
#include "GL/glew.h"

/**
* @brief Encoder for OpenGL texture and PBO input. Assumes an existing OpenGL context.
*/
class EncoderOpenGL : public EncoderCUDA
{
public:
	EncoderOpenGL(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate);
	virtual ~EncoderOpenGL();

	void EncodePBO(GLuint pbo, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);
	void EncodeTexture(GLuint texture, GLenum target, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

	std::shared_ptr<NV_ENC_LOCK_BITSTREAM> EncodeFrame(GLuint texture, GLenum target, uint32_t width, uint32_t height, bool iFrame);

private:
	struct RegisteredPBO
	{
		size_t size = 0;
		CUgraphicsResource graphicsResource = nullptr;
	};
	std::unordered_map<GLuint, RegisteredPBO> m_registeredPBOs;

	struct RegisteredTexture
	{
		uint32_t width = 0;
		uint32_t height = 0;
		CUgraphicsResource graphicsResource = nullptr;
	};
	std::unordered_map<GLuint, RegisteredTexture> m_registeredTextures;
};