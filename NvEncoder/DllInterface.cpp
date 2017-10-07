#include "DllInterface.h"
#include <memory>
#include "EncoderCUDA.h"
#include "EncoderOpenGL.h"
#include "EncoderDX11.h"

// A shared pointer to the base class.
std::shared_ptr<Encoder>			frameEncoder;

HMODULE hEncodeDLL = nullptr;

__declspec(dllexport) bool InitNVENC()
{
#if defined(_WIN32)
#if defined(_WIN64)
	hEncodeDLL = LoadLibrary(TEXT("nvEncodeAPI64.dll"));
#else
	hEncodeDLL = LoadLibrary(TEXT("nvEncodeAPI.dll"));
#endif
#else
	hEncodeDLL = dlopen("libnvidia-encode.so.1", RTLD_LAZY);
#endif

	// FIXME
	if (!hEncodeDLL)
		return false;//throw std::runtime_error("nvEncodeAPI library file is not found");

	return true;
}

__declspec(dllexport) bool SetConcurrentEncodes(unsigned int encodes)
{

}

__declspec(dllexport) bool InitOpenGLEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{
	if (hEncodeDLL == nullptr)
	{
		return false;
	}

	//TODO: Fix horrible colorConversion.ptx load!
	cudaEncoder = std::make_shared<EncoderOpenGL>(encodeWidth, encodeHeight, hevc, bitrate);

	return true;
}

__declspec(dllexport) bool InitCUDAEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{
	if (hEncodeDLL == nullptr)
	{
		return false;
	}

	return false;
}

__declspec(dllexport) bool InitDX11Encoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc)
{
	if (hEncodeDLL == nullptr)
	{
		return false;
	}

	return false;
}

__declspec(dllexport) void* EncodeOpenGLFrame(unsigned int texture /*GLUint*/, unsigned int target /*GLEnum*/, unsigned int width, unsigned int height, bool iFrame, void* buffer, int bufferSize)
{
	if (hEncodeDLL == nullptr)
	{
		return nullptr;
	}
	// Dude!

	oglEncoder->EncodeTexture(texture, target, width, height, iFrame, )
	{
	}
	)

	return true;
}
