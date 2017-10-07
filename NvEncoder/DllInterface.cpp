#include "DllInterface.h"
#include <memory>
#include "EncoderCUDA.h"
#include "EncoderOpenGL.h"
#include "EncoderDX11.h"

std::shared_ptr<EncoderCUDA>		cudaEncoder;
std::shared_ptr<EncoderOpenGL>		oglEncoder;
std::shared_ptr<EncoderDX11>		dx11Encoder;

__declspec(dllexport) bool InitOpenGLEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{
	//TODO: Fix horrible colorConversion.ptx load!
	cudaEncoder = std::make_shared<EncoderOpenGL>(width, height, hevc, bitrate)
}

__declspec(dllexport) bool InitCUDAEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{

}

__declspec(dllexport) bool InitDX11Encoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{

}

__declspec(dllexport) bool EncodeOpenGLFrame(unsigned int texture /*GLUint*/, unsigned int target /*GLEnum*/, int width, int height, bool iFrame, void* buffer, int bufferSize)
{
	// Dude!

	return true;
}
