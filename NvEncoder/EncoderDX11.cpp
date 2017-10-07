#include "EncoderDX11.h"

/**
* @brief DirectX-based encoder constructor.
* @param device
* @param width
* @param height
* @param hevc
* @param bitrate
* @param ptxSearchPath
*/
EncoderDX11::EncoderDX11(ID3D11Device* device, uint32_t width, uint32_t height, bool hevc, uint32_t bitrate)
{
	Encoder::Init(NV_ENC_DEVICE_TYPE_DIRECTX, device, width, height, hevc, bitrate);
}



/**
* @brief DirectX-based encoder destructor.
*/
EncoderDX11::~EncoderDX11()
{
	if (m_texture)
		m_texture->Release();
}



/**
* @brief Encodes the given Direct3D texture.
* @param texture
* @param width
* @param height
* @param iFrame
* @param buffer
*/
void EncoderDX11::EncodeTexture(ID3D11Texture2D* texture, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
	if (texture != m_texture)
	{
		m_texture->Release();
		texture->AddRef();
		m_texture = texture;
	}

	Encoder::Encode(NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX, (void*)texture, NV_ENC_BUFFER_FORMAT_ABGR, 0, width, height, iFrame, buffer);
}
