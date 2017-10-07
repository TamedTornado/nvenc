#pragma once

extern "C" __declspec(dllexport) bool InitNVENC();

extern "C" __declspec(dllexport) bool SetConcurrentEncodes(unsigned int encodes);

extern "C" __declspec(dllexport) bool InitOpenGLEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc);

extern "C" __declspec(dllexport) bool InitCUDAEncoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc);

extern "C" __declspec(dllexport) bool InitDX11Encoder(void* device, unsigned int encodeWidth, unsigned int encodeHeight, unsigned int bitrate, bool hevc);

//************************************
// Method:    EncodeOpenGLFrame
// FullName:  EncodeOpenGLFrame
// Access:    public 
// Returns:   bool
// Qualifier:
// Parameter: unsigned int texture - GLUint handle for the texture
// Parameter: unsigned int target - GLEnum for target (GL_TEXTURE_2D for example)
// Parameter: int width
// Parameter: int height
// Parameter: bool iFrame - if true, set encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
// Parameter: void * buffer - output buffer
// Parameter: int bufferSize - size of output buffer
//************************************
extern "C" __declspec(dllexport) void* EncodeOpenGLFrame(unsigned int texture /*GLUint*/, unsigned int target /*GLEnum*/, unsigned int width, unsigned int height, bool iFrame, void* buffer, int bufferSize);

extern "C" __declspec(dllexport) bool ReleaseEncodedFrame(void *frameHandle);