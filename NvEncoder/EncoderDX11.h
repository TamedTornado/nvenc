#pragma once

#include <d3d11.h>
#include "nvEncodeAPI.h"
#include <vector>
#include "encoder.h"

/**
* @brief Encoder for DirectX texture input.
*/
class EncoderDX11 : public Encoder
{
public:
	EncoderDX11(ID3D11Device* device, uint32_t width, uint32_t height, bool hevc, uint32_t bitrate);
	virtual ~EncoderDX11();

	void EncodeTexture(ID3D11Texture2D* texture, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

private:
	ID3D11Texture2D* m_texture = nullptr;
};
