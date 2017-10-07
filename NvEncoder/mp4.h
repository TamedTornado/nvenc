// Copyright 2017 NVIDIA Corporation. All Rights Reserved.
#pragma once

#include <vector>
#include <cstdint>
#include <string>


class Mp4_box;
class Mp4_tfhd;
class Mp4_tfdt;
class Mp4_trun;


/**
 * @brief MP4 container for H.264 streaming.
 */
class MP4
{
public:
	virtual ~MP4();

	void Wrap(const std::vector<uint8_t>& inputFrameH264, uint32_t width, uint32_t height, std::vector<uint8_t>& outputBuffer);

	void WrapToFile(const std::vector<uint8_t>& inputFrameH264, uint32_t width, uint32_t height, const std::string& path);

private:
	bool initialized = false;

	Mp4_box* m_moof = nullptr;
	Mp4_tfhd* m_tfhd = nullptr;
	Mp4_tfdt* m_tfdt = nullptr;
	Mp4_trun* m_trun = nullptr;

	uint32_t m_track_id = 1;
	uint32_t m_seqno = 0;
};


