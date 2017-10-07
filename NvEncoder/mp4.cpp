// Copyright 2017 NVIDIA Corporation. All Rights Reserved.

#include "mp4.h"

#include <cstddef>
#include <string>
#include <fstream>


template<typename T>
inline void write(std::vector<uint8_t>& buffer, T v)
{
    uint8_t* p = (uint8_t*) &v;
    for (int i = sizeof(T) - 1; i >= 0; --i)
        buffer.push_back(p[i]);
}

inline void writeString(std::vector<uint8_t>& buffer, const std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        buffer.push_back((uint8_t) s[i]);
    buffer.push_back(0);
}



/**
 * @brief General definition of a box (also called atom).
 */
class Mp4_box
{
public:

    /// Constructor for box (32-bit size)
    ///
    /// \param type The box's type
    /// \param size The box's size
    Mp4_box(uint32_t type, uint32_t size = s_mp4_box_header_size)
        : m_type(type)
        , m_size(size)
        , m_size_64(0)
        , m_is_full(false)
        , m_bits(0)
        , m_parent(nullptr)
    {
    }

    /// Constructor for full box (32-bit size)
    ///
    /// \param type The box's type
    /// \param size The box's size
    /// \param version The box's version
    /// \param flags The box's flags
    Mp4_box(uint32_t type, uint32_t size, uint8_t version, uint32_t flags)
        : m_type(type)
        , m_size(size)
        , m_size_64(0)
        , m_is_full(true)
        , m_bits((static_cast<uint32_t>(version) << 24) | (flags & 0x00ffffff))
        , m_parent(nullptr)
    {
    }

    /// Constructor for box (32-bit size)
    ///
    /// \param type The box's type
    /// \param size The box's size
    Mp4_box(uint32_t type, uint64_t size)
        : m_type(type)
        , m_size(1)
        , m_size_64(size)
        , m_is_full(false)
        , m_bits(0)
        , m_parent(nullptr)
    {
    }

    /// Constructor for full box (64-bit size)
    ///
    /// \param type The box's type
    /// \param size The box's size
    /// \param version The box's version
    /// \param flags The box's flags
    Mp4_box(uint32_t type, uint64_t size, uint8_t version, uint32_t flags)
        : m_type(type)
        , m_size(1)
        , m_size_64(size)
        , m_is_full(true)
        , m_bits((static_cast<uint32_t>(version) << 24) | (flags & 0x00ffffff))
        , m_parent(nullptr)
    {
    }


    /// Destructor
    virtual ~Mp4_box()
    {
        for (size_t i = 0; i < m_boxes.size(); ++i)
            delete m_boxes[i];
    }

    /// Get the box's header size
    /// \return the box's header size
    inline uint32_t get_header_size() const
    {
        if (m_size == 1)
            return m_is_full ? s_mp4_full_box_header_size_64 :  s_mp4_box_header_size_64;
        else
            return m_is_full ? s_mp4_full_box_header_size :  s_mp4_box_header_size;
    }

    /// Get the box's size
    /// \return the box's size
    inline uint64_t get_size() const
    {
        return (m_size == 1) ? m_size_64 : m_size;
    }

    /// Set the box's size
    /// \param size The box's size
    inline void set_size(uint64_t size)
    {
        if (size >= ((uint64_t) 1) << 32)
        {
            m_size = 1;
            m_size_64 = size;
        }
        else
            m_size = static_cast<uint32_t>(size);
    }

    /// Serialize the box
    void serialize(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_size);
        write(outputBuffer, m_type);

        if (m_size == 1)
            write(outputBuffer, m_size_64);

        if (m_is_full)
            write(outputBuffer, m_bits);

        serialize_body(outputBuffer);
    }

    /// Serialize the box's body
    virtual void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        for (size_t i = 0; i < m_boxes.size(); ++i)
            m_boxes[i]->serialize(outputBuffer);
    }

    /// add a child box
    /// \param child A child box
    void add_box(Mp4_box* child)
    {
        child->m_parent = this;
        m_boxes.push_back(child);

        // Adjust the size of all the boxes recursively until we reach the root
        Mp4_box* p = this;
        uint64_t add_size = child->get_size();
        while (p)
        {
            p->set_size(p->get_size() + add_size);
            p = p->m_parent;
        }
    }

    static const uint32_t s_mp4_box_header_size         = 8;
    static const uint32_t s_mp4_full_box_header_size    = 12;
    static const uint32_t s_mp4_box_header_size_64      = 16;
    static const uint32_t s_mp4_full_box_header_size_64 = 20;

    static inline uint32_t chars_to_type(uint8_t u1, uint8_t u2, uint8_t u3, uint8_t u4)
    {
        return (static_cast<uint32_t>(u1) << 24) | (static_cast<uint32_t>(u2) << 16) | (static_cast<uint32_t>(u3) << 8) | static_cast<uint32_t>(u4);
    }

    static const uint32_t s_mp4_box_type_dinf;
    static const uint32_t s_mp4_box_type_hdlr;
    static const uint32_t s_mp4_box_type_mdhd;
    static const uint32_t s_mp4_box_type_mdia;
    static const uint32_t s_mp4_box_type_minf;
    static const uint32_t s_mp4_box_type_moov;
    static const uint32_t s_mp4_box_type_moof;
    static const uint32_t s_mp4_box_type_mvex;
    static const uint32_t s_mp4_box_type_stbl;
    static const uint32_t s_mp4_box_type_tkhd;
    static const uint32_t s_mp4_box_type_traf;
    static const uint32_t s_mp4_box_type_trak;

protected:
    uint32_t m_type;
    uint32_t m_size;
    uint64_t m_size_64;
    bool m_is_full;
    uint32_t m_bits;
    Mp4_box* m_parent;

    std::vector<Mp4_box*> m_boxes;
};

const uint32_t Mp4_box::s_mp4_box_type_dinf(Mp4_box::chars_to_type('d','i','n','f'));
const uint32_t Mp4_box::s_mp4_box_type_hdlr(Mp4_box::chars_to_type('h','d','l','r'));
const uint32_t Mp4_box::s_mp4_box_type_mdhd(Mp4_box::chars_to_type('m','d','h','d'));
const uint32_t Mp4_box::s_mp4_box_type_mdia(Mp4_box::chars_to_type('m','d','i','a'));
const uint32_t Mp4_box::s_mp4_box_type_minf(Mp4_box::chars_to_type('m','i','n','f'));
const uint32_t Mp4_box::s_mp4_box_type_moof(Mp4_box::chars_to_type('m','o','o','f'));
const uint32_t Mp4_box::s_mp4_box_type_moov(Mp4_box::chars_to_type('m','o','o','v'));
const uint32_t Mp4_box::s_mp4_box_type_mvex(Mp4_box::chars_to_type('m','v','e','x'));
const uint32_t Mp4_box::s_mp4_box_type_stbl(Mp4_box::chars_to_type('s','t','b','l'));
const uint32_t Mp4_box::s_mp4_box_type_tkhd(Mp4_box::chars_to_type('t','k','h','d'));
const uint32_t Mp4_box::s_mp4_box_type_traf(Mp4_box::chars_to_type('t','r','a','f'));
const uint32_t Mp4_box::s_mp4_box_type_trak(Mp4_box::chars_to_type('t','r','a','k'));




/**
 * @brief mvhd (movie header box).
 */
class Mp4_mvhd : public Mp4_box
{
public:

    /// Constructor
    /// \param creation_time creation time
    /// \param mod_time modification time
    /// \param time_scale Time scale of the media
    /// \param duration duration of the media
    Mp4_mvhd(uint32_t creation_time, uint32_t mod_time, uint32_t time_scale, uint32_t duration)
        : Mp4_box(s_mp4_box_type_mvhd,
                  s_mp4_full_box_header_size + s_mp4_mvhd_body_size, 0, 0)
        , m_creation_time(creation_time)
        , m_mod_time(mod_time)
        , m_time_scale(time_scale)
        , m_duration(duration)
    {

    }

    /// Serialize mvhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_creation_time);

        write(outputBuffer, m_mod_time);

        write(outputBuffer, m_time_scale);

        write(outputBuffer, m_duration);

        // rate
        write<uint32_t>(outputBuffer, 0x00010000);

        // volume and reserved bits
        write<uint32_t>(outputBuffer, 0x01000000);

        // reserved bits
        for (int i = 0; i < 2; ++i)
            write<uint32_t>(outputBuffer, 0);

        // unity matrix
        write<uint32_t>(outputBuffer, 0x00010000);

        for (int i = 0; i < 3; ++i)
            write<uint32_t>(outputBuffer, 0);

        write<uint32_t>(outputBuffer, 0x00010000);

        for (int i = 0; i < 3; ++i)
            write<uint32_t>(outputBuffer, 0);

        write<uint32_t>(outputBuffer, 0x40000000);

        // predefined
        for (int i = 0; i < 6; ++i)
            write<uint32_t>(outputBuffer, 0);

        // next track ID (0xffffffff means that an unused track ID will be taken)
        write<uint32_t>(outputBuffer, 0xffffffff);
    }

private:
    static const uint32_t s_mp4_box_type_mvhd;
    static const uint32_t s_mp4_mvhd_body_size = 96;

    uint32_t m_creation_time;
    uint32_t m_mod_time;
    uint32_t m_time_scale;
    uint32_t m_duration;
};

const uint32_t Mp4_mvhd::s_mp4_box_type_mvhd(Mp4_box::chars_to_type('m','v','h','d'));




/**
 * @brief mehd (movie extends header box).
 */
class Mp4_mehd : public Mp4_box
{
public:

    /// Constructor
    /// \param fragment_duration The fragment's duration
    Mp4_mehd(uint32_t fragment_duration)
        : Mp4_box(s_mp4_box_type_mehd,
                  s_mp4_full_box_header_size + s_mp4_mehd_body_size, 0, 0)
        , m_fragment_duration(fragment_duration)
    {
    }

    /// Serialize mehd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_fragment_duration);
    }

private:
    static const uint32_t s_mp4_box_type_mehd;
    static const uint32_t s_mp4_mehd_body_size = 4;

    uint32_t m_fragment_duration;
};

const uint32_t Mp4_mehd::s_mp4_box_type_mehd(Mp4_box::chars_to_type('m','e','h','d'));



/**
 * @brief trex (track extends box).
 */
class Mp4_trex : public Mp4_box
{
public:

    /// Constructor
    /// \param track_id                 Track ID
    /// \param sample_description_index Default sample description index
    /// \param sample_duration          Default sample duration
    /// \param sample_size              Default sample size
    /// \param sample_flags             Default sample flags
    Mp4_trex(uint32_t track_id, uint32_t sample_description_index,
             uint32_t sample_duration, uint32_t sample_size, uint32_t sample_flags)
        : Mp4_box(s_mp4_box_type_trex,
                  s_mp4_full_box_header_size + s_mp4_trex_body_size, 0, 0)
        , m_track_id(track_id)
        , m_default_sample_description_index(sample_description_index)
        , m_default_sample_duration(sample_duration)
        , m_default_sample_size(sample_size)
        , m_default_sample_flags(sample_flags)
    {

    }

    /// Serialize trex's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_track_id);

        write(outputBuffer, m_default_sample_description_index);

        write(outputBuffer, m_default_sample_duration);

        write(outputBuffer, m_default_sample_size);

        write(outputBuffer, m_default_sample_flags);
    }

private:

    static const uint32_t s_mp4_box_type_trex;
    static const uint32_t s_mp4_trex_body_size = 20;

    uint32_t m_track_id;
    uint32_t m_default_sample_description_index;
    uint32_t m_default_sample_duration;
    uint32_t m_default_sample_size;
    uint32_t m_default_sample_flags;
};

const uint32_t Mp4_trex::s_mp4_box_type_trex(Mp4_box::chars_to_type('t','r','e','x'));



/**
 * @brief tkhd (track header box).
 */
class Mp4_tkhd : public Mp4_box
{
public:

    /// Constructor
    /// \param track_id Track ID
    /// \param width video's width
    /// \param height video's height
    Mp4_tkhd(uint32_t track_id, uint32_t width, uint32_t height)
        : Mp4_box(s_mp4_box_type_tkhd,
                  s_mp4_full_box_header_size + s_mp4_tkhd_body_size, 1, 0x007)
        , m_track_id(track_id)
        , m_width(width)
        , m_height(height)
    {

    }

    /// Serialize tkhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // creation time
        write<uint64_t>(outputBuffer, 0);

        // modification time
        write<uint64_t>(outputBuffer, 0);

        write(outputBuffer, m_track_id);

        // reserved
        write<uint32_t>(outputBuffer, 0);

        // duration
        write<uint64_t>(outputBuffer, 0);

        // int(32)[2] reserved
        for (int i = 0; i < 2; ++i)
            write<uint32_t>(outputBuffer, 0);

        // int(16) layer
        // int(16) alternate_group
        // int(16) volume
        // int(16) reserved
        for (size_t i = 0; i < 4; ++i)
            write<uint16_t>(outputBuffer, 0);

        // unity matrix
        write<uint32_t>(outputBuffer, 0x00010000);

        for (int i = 0; i < 3; ++i)
            write<uint32_t>(outputBuffer, 0);

        write<uint32_t>(outputBuffer, 0x00010000);

        for (int i = 0; i < 3; ++i)
            write<uint32_t>(outputBuffer, 0);

        write<uint32_t>(outputBuffer, 0x40000000);

        // shift the bytes to get fixed point values (encoded as 16 bits for
        // decimal and fractional parts)
        write<uint32_t>(outputBuffer, m_width << 16);
        write<uint32_t>(outputBuffer, m_height << 16);
    }

private:

    static const uint32_t s_mp4_box_type_tkhd;
    static const uint32_t s_mp4_tkhd_body_size = 92;

    uint32_t m_track_id;
    uint32_t m_width;
    uint32_t m_height;
};

const uint32_t Mp4_tkhd::s_mp4_box_type_tkhd(Mp4_box::chars_to_type('t','k','h','d'));



/**
 * @brief mdhd (media header box).
 */
class Mp4_mdhd : public Mp4_box
{
public:

    /// Constructor
    /// \param time_scale time scale
    /// \param language language code as specified in ISO-639-2/T
    Mp4_mdhd(uint32_t time_scale, const std::string& language)
        : Mp4_box(s_mp4_box_type_mdhd,
                  s_mp4_full_box_header_size + s_mp4_mdhd_body_size, 1, 0)
        , m_time_scale(time_scale)
    {
        size_t lsize = language.size();
        for (size_t i = 0; i < 3; ++i)
        {
            if (i < lsize)
                m_language[i] = language[i];
            else
                m_language[i] = 'a';
        }
    }

    /// Serialize mdhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // creation time (8 bytes)
        write<uint64_t>(outputBuffer, 0);

        // modification time (8 bytes)
        write<uint64_t>(outputBuffer, 0);

        write(outputBuffer, m_time_scale);

        // duration (8 bytes)
        write<uint64_t>(outputBuffer, 0xffffffffffffffff);

        // bit(1) pad = 0
        // unsigned int(5)[3] language;
        // unsigned int(16) pre_defined = 0
        // language is encoded as 3 characters according to ISO-639-2/T
        uint32_t temp = 0;
        for (size_t i = 0; i < 3; ++i) {
            temp = (temp << 5) | (m_language[i] - 0x60);
        }
        temp <<= 16;
        write(outputBuffer, temp);
    }

private:
    static const uint32_t s_mp4_box_type_mdhd;
    static const uint32_t s_mp4_mdhd_body_size = 32;

    uint32_t m_time_scale;
    uint32_t  m_language[3];
};

const uint32_t Mp4_mdhd::s_mp4_box_type_mdhd(Mp4_box::chars_to_type('m','d','h','d'));



/**
 * @brief hdlr (handler reference box).
 */
class Mp4_hdlr : public Mp4_box
{
public:

    /// Constructor
    /// \param handler_type The handler type (video, audio, or hint)
    /// \param handler_name The handler name (for debugging and inspection)
    Mp4_hdlr(const uint32_t handler_type, const std::string& handler_name)
        : Mp4_box(s_mp4_box_type_hdlr,
                  static_cast<uint32_t>(s_mp4_full_box_header_size + s_mp4_hdlr_body_size +
                                        handler_name.size() + 1), 0, 0), m_handler_type(handler_type),
          m_handler_name(handler_name)
    {
    }

    /// Serialize hdlr's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // pre-defined
        write<uint32_t>(outputBuffer, 0);

        // handler type
        write(outputBuffer, m_handler_type);

        // reserved
        for (int i = 0; i < 3; ++i)
            write<uint32_t>(outputBuffer, 0);

        writeString(outputBuffer, m_handler_name);
    }

    static const uint32_t s_mp4_hdlr_video_handler;
    static const uint32_t s_mp4_hdlr_audio_handler;

private:
    static const uint32_t s_mp4_box_type_hdlr;
    static const uint32_t s_mp4_hdlr_body_size = 20;

    uint32_t m_handler_type;
    std::string m_handler_name;
};

const uint32_t Mp4_hdlr::s_mp4_box_type_hdlr(Mp4_box::chars_to_type('h','d','l','r'));
const uint32_t Mp4_hdlr::s_mp4_hdlr_video_handler(Mp4_box::chars_to_type('v','i','d','e'));



/**
 * @brief Data entry base type.
 */
class Mp4_data_entry : public Mp4_box
{
public:
    Mp4_data_entry(uint32_t type, uint32_t size, uint8_t version, uint32_t flags)
        : Mp4_box(type, s_mp4_full_box_header_size + size, version, flags)
    {
    }
};



/**
 * @brief URL data entry.
 */
class Mp4_data_entry_url : public Mp4_data_entry
{
public:
    Mp4_data_entry_url(const std::string& location)
        : Mp4_data_entry(Mp4_data_entry_url::s_mp4_box_type_data_entry_url, static_cast<uint32_t>(location.size()), 0, 1), m_location(location)
    {
    }

    uint64_t get_size() const
    {
        size_t location_length = m_location.size();
        if (((m_bits & 0x01) == 0) || location_length)
            return s_mp4_full_box_header_size + location_length + 1;
        else
            return s_mp4_full_box_header_size;
    }

    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        size_t location_length = m_location.size();
        if (((m_bits & 0x01) == 0) || location_length)
            writeString(outputBuffer, m_location);
    }

private:
    static const uint32_t s_mp4_box_type_data_entry_url;

    std::string m_location;
};

const uint32_t Mp4_data_entry_url::s_mp4_box_type_data_entry_url(Mp4_box::chars_to_type('u','r','l',' '));



/**
 * @brief URN data entry.
 */
class Mp4_data_entry_urn : public Mp4_data_entry
{
public:
    Mp4_data_entry_urn(const std::string& name, const std::string& location)
        : Mp4_data_entry(Mp4_data_entry_urn::s_mp4_box_type_data_entry_urn, static_cast<uint32_t>(name.size() + location.size()), 0, 0), m_name(name), m_location(location)
    {
    }

    uint64_t get_size() const
    {
        return s_mp4_full_box_header_size + m_name.size() + m_location.size() + 2;
    }

    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        writeString(outputBuffer, m_name);

        writeString(outputBuffer, m_location);
    }

private:
    static const uint32_t s_mp4_box_type_data_entry_urn;

    std::string m_name;
    std::string m_location;
};

const uint32_t Mp4_data_entry_urn::s_mp4_box_type_data_entry_urn(Mp4_box::chars_to_type('u','r','n',' '));




/**
 * @brief dref (data reference box).
 */
class Mp4_dref : public Mp4_box
{
public:

    /// Constructor
    Mp4_dref()
        : Mp4_box(s_mp4_box_type_dref,
                  s_mp4_full_box_header_size + s_mp4_dref_body_size, 0, 0)
    {

    }

    /// Serialize dref's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // entry count
        size_t nr_children = m_boxes.size();
        write<uint32_t>(outputBuffer, nr_children);

        for (size_t i = 0; i < nr_children; ++i)
            m_boxes[i]->serialize(outputBuffer);
    }


private:
    static const uint32_t s_mp4_box_type_dref;
    static const uint32_t s_mp4_dref_body_size = 4;
};

const uint32_t Mp4_dref::s_mp4_box_type_dref(Mp4_box::chars_to_type('d','r','e','f'));




/**
 * @brief vmhd (video media header).
 */
class Mp4_vmhd : public Mp4_box
{
public:

    /// Constructor
    ///
    /// \param flags vmhd's flags
    Mp4_vmhd(uint32_t flags)
        : Mp4_box(s_mp4_box_type_vmhd, s_mp4_full_box_header_size + s_mp4_vmhd_body_size, 0, flags)
    {
    }

    /// Serialize vmhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // graphicsmode = 0 (2 bytes)
        write<uint16_t>(outputBuffer, 0);

        // opcolor = {0, 0, 0} (3x2 bytes)
        for (int i = 0; i < 3; ++i)
            write<uint16_t>(outputBuffer, 0);
    }

private:
    static const uint32_t s_mp4_box_type_vmhd;
    static const uint32_t s_mp4_vmhd_body_size = 8;

    uint32_t m_time_scale;
    uint32_t m_language[3];
};

const uint32_t Mp4_vmhd::s_mp4_box_type_vmhd(Mp4_box::chars_to_type('v','m','h','d'));



/**
 * @brief AVC decoder configuration.
 */
class Mp4_avc_decoder_configuration : public Mp4_box
{
public:

    Mp4_avc_decoder_configuration()
        : Mp4_box(s_mp4_avc_decoder_configuration_type, s_mp4_box_header_size + s_mp4_avc_decoder_configuration_body_size)
    {
    }

    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // config version (1 byte)
        write<uint8_t>(outputBuffer, 1);

        // profile indication (1 byte)
        write<uint8_t>(outputBuffer, 0);

        // profile compatibility (1 byte)
        write<uint8_t>(outputBuffer, 0);

        // AVC level indication (1 byte)
        write<uint8_t>(outputBuffer, 0);

        // reserved (6 bit)
        // lengthSizeMinusOne (2 bit)
        write<uint8_t>(outputBuffer, 0xff);

        // reserved (3 bit)
        // numberOfSequenceParameterSets (5 bit)
        write<uint8_t>(outputBuffer, 0xe0);

        // numberOfPictureParameterSets (5 bit)
        write<uint8_t>(outputBuffer, 0);
    }

private:
    static const uint32_t s_mp4_avc_decoder_configuration_type;
    static const uint32_t s_mp4_avc_decoder_configuration_body_size = 7;
};

const uint32_t Mp4_avc_decoder_configuration::s_mp4_avc_decoder_configuration_type(Mp4_box::chars_to_type('a','v','c','C'));



/**
 * @brief Visual sample entry.
 */
class Mp4_visual_sample_entry : public Mp4_box
{
public:

    Mp4_visual_sample_entry(uint16_t width, uint16_t height)
        : Mp4_box(s_mp4_visual_sample_entry_avc1, s_mp4_box_header_size + s_mp4_visual_sample_entry_body_size), m_width(width), m_height(height)
    {
        Mp4_avc_decoder_configuration* decoder_config = new Mp4_avc_decoder_configuration();
        add_box(decoder_config);
    }

    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // reserved (6 bytes)
        for (int i = 0; i < 6; ++i)
            write<uint8_t>(outputBuffer, 0);

        // data_reference_index
        write<uint16_t>(outputBuffer, 1);

        // pre-defined (2 bytes)
        // resreved (2 bytes)
        // pre-defined (3 x 4 bytes)
        for (int i = 0; i < 16; ++i)
            write<uint8_t>(outputBuffer, 0);

        // width
        write(outputBuffer, m_width);

        // height
        write(outputBuffer, m_height);

        // horizresolution (4 bytes): 72 dpi
        // vertresolution (4 bytes): 72 dpi
        for (int i = 0; i < 2; ++i)
            write<uint32_t>(outputBuffer, 0x00480000);

        // reserved (4 bytes)
        for (int i = 0; i < 4; ++i)
            write<uint8_t>(outputBuffer, 0);

        // frame_count (2 bytes)
        write<uint16_t>(outputBuffer, 1);

        // compressorname (32 bytes)
        for (int i = 0; i < 32; ++i)
            write<uint8_t>(outputBuffer, 0);

        // depth (2 bytes)
        write<uint16_t>(outputBuffer, 0x003c);

        // pre-defined = -1 (2 bytes)
        write<uint16_t>(outputBuffer, 0xffff);

        size_t nr_children = m_boxes.size();
        for (size_t i = 0; i < nr_children; ++i)
            m_boxes[i]->serialize(outputBuffer);
    }

private:
    static const uint32_t s_mp4_visual_sample_entry_avc1;
    static const uint32_t s_mp4_visual_sample_entry_body_size = 78;
    uint16_t m_width;
    uint16_t m_height;
};

const uint32_t Mp4_visual_sample_entry::s_mp4_visual_sample_entry_avc1(Mp4_box::chars_to_type('a','v','c','1'));



/**
 * @brief stsd (sample description box).
 */
class Mp4_stsd : public Mp4_box
{
public:

    /// Constructor
    /// \param width video's width
    /// \param height video's height
    Mp4_stsd(uint16_t width, uint16_t height)
        : Mp4_box(s_mp4_box_type_stsd, s_mp4_full_box_header_size + s_mp4_stsd_body_size, 0, 0)
    {
        Mp4_visual_sample_entry* sample_entry = new Mp4_visual_sample_entry(width, height);
        add_box(sample_entry);
    }

    /// Serialize stsd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        size_t nr_children = m_boxes.size();
        write<uint32_t>(outputBuffer, nr_children);

        for (size_t i = 0; i < nr_children; ++i)
            m_boxes[i]->serialize(outputBuffer);
    }

private:
    static const uint32_t s_mp4_box_type_stsd;
    static const uint32_t s_mp4_stsd_body_size = 4;

};

const uint32_t Mp4_stsd::s_mp4_box_type_stsd(Mp4_box::chars_to_type('s','t','s','d'));



/**
 * @brief stsz (sample size box).
 */
class Mp4_stsz : public Mp4_box
{
public:

    /// Constructor
    /// \param sample_size The default sample size
    /// \param sample_count The number of samples in the track
    Mp4_stsz(uint32_t sample_size, uint32_t sample_count)
        : Mp4_box(s_mp4_box_type_stsz, s_mp4_full_box_header_size + s_mp4_stsz_body_size, 0, 0), m_sample_size(sample_size), m_sample_count(sample_count)
    {
    }

    /// Serialize stsz's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_sample_size);

        write(outputBuffer, m_sample_count);
    }

private:

    static const uint32_t s_mp4_box_type_stsz;
    static const uint32_t s_mp4_stsz_body_size = 8;

    uint32_t m_sample_size;
    uint32_t m_sample_count;
};

const uint32_t Mp4_stsz::s_mp4_box_type_stsz(Mp4_box::chars_to_type('s','t','s','z'));



/**
 * @brief stsc (sample to chunk box).
 */
class Mp4_stsc : public Mp4_box
{
public:

    /// Constructor
    /// \param entry_count Number of entries
    Mp4_stsc(uint32_t entry_count)
        : Mp4_box(s_mp4_box_type_stsc, s_mp4_full_box_header_size + s_mp4_stsc_body_size, 0, 0), m_entry_count(entry_count)
    {
    }

    /// Serialize stsc's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_entry_count);
    }

private:

    static const uint32_t s_mp4_box_type_stsc;
    static const uint32_t s_mp4_stsc_body_size = 4;

    uint32_t m_entry_count;
};

const uint32_t Mp4_stsc::s_mp4_box_type_stsc(Mp4_box::chars_to_type('s','t','s','c'));



/**
 * @brief stts (decoding time to sample box).
 */
class Mp4_stts : public Mp4_box
{
public:

    /// Constructor
    /// \param entry_count entry count
    Mp4_stts(uint32_t entry_count)
        : Mp4_box(s_mp4_box_type_stts, s_mp4_full_box_header_size + s_mp4_stts_body_size, 0, 0), m_entry_count(entry_count)
    {
    }

    /// Serialize stts's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_entry_count);
    }


private:

    static const uint32_t s_mp4_box_type_stts;
    static const uint32_t s_mp4_stts_body_size = 4;

    uint32_t m_entry_count;
};

const uint32_t Mp4_stts::s_mp4_box_type_stts(Mp4_box::chars_to_type('s','t','t','s'));




/**
 * @brief stco (chunk offset box).
 */
class Mp4_stco : public Mp4_box
{
public:

    /// Constructor
    /// \param entry_count Number of entries
    Mp4_stco(uint32_t entry_count)
        : Mp4_box(s_mp4_box_type_stco, s_mp4_full_box_header_size + s_mp4_stco_body_size, 0, 0), m_entry_count(entry_count)
    {
    }

    /// Serialize stco's body
    /// \param data_pointer points to the serialized buffer for stco's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_entry_count);
    }

private:

    static const uint32_t s_mp4_box_type_stco;
    static const uint32_t s_mp4_stco_body_size = 4;

    uint32_t m_entry_count;
};

const uint32_t Mp4_stco::s_mp4_box_type_stco(Mp4_box::chars_to_type('s','t','c','o'));



/**
 * @brief mfhd (movie fragment header box).
 */
class Mp4_mfhd : public Mp4_box
{
public:

    /// Constructor
    /// \param sequence_number The sequence number of this fragment
    Mp4_mfhd(uint32_t sequence_number)
        : Mp4_box(s_mp4_box_type_mfhd, s_mp4_full_box_header_size + s_mp4_mfhd_body_size, 0, 0), m_sequence_number(sequence_number)
    {
    }

    /// Serialize mfhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_sequence_number);
    }

private:

    static const uint32_t s_mp4_box_type_mfhd;
    static const uint32_t s_mp4_mfhd_body_size = 4;

    uint32_t m_sequence_number;
};

const uint32_t Mp4_mfhd::s_mp4_box_type_mfhd(Mp4_box::chars_to_type('m','f','h','d'));



/**
 * @brief tfhd (track fragment header box).
 */
class Mp4_tfhd : public Mp4_box
{
public:

    /// Constructor
    /// \param flags tfhd's flags
    /// \param track_id Track ID
    /// \param base_data_offset base data offset
    /// \param sample_description_index sample description index
    /// \param default_sample_duration sample duration
    /// \param default_sample_size sample size
    /// \param default_sample_flags sample flags
    Mp4_tfhd(uint32_t flags, uint32_t track_id, uint64_t base_data_offset, uint32_t sample_description_index, uint32_t default_sample_duration, uint32_t default_sample_size, uint32_t default_sample_flags)
        : Mp4_box(s_mp4_box_type_tfhd, s_mp4_full_box_header_size + s_mp4_tfhd_body_size + get_optional_size(flags), 0, flags),
          m_track_id(track_id),
          m_base_data_offset(base_data_offset),
          m_sample_description_index(sample_description_index),
          m_default_sample_duration(default_sample_duration),
          m_default_sample_size(default_sample_size),
          m_default_sample_flags(default_sample_flags)
    {
    }

    /// Set the default sample size
    /// \param default_sample_size sample size
    void set_default_sample_size(uint32_t default_sample_size)
    {
        m_default_sample_size = default_sample_size;
    }

    /// Set the default sample flags
    /// \param default_sample_flags sample flags
    void set_default_sample_flags(uint32_t default_sample_flags)
    {
        m_default_sample_flags = default_sample_flags;
    }

    /// Serialize tfhd's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_track_id);

        if (m_bits & s_mp4_tfhd_flag_base_data_offset_present)
            write(outputBuffer, m_base_data_offset);

        if (m_bits & s_mp4_tfhd_flag_sample_description_index_present)
            write(outputBuffer, m_sample_description_index);

        if (m_bits & s_mp4_tfhd_flag_default_sample_duration_present)
            write(outputBuffer, m_default_sample_duration);

        if (m_bits & s_mp4_tfhd_flag_default_sample_size_present)
            write(outputBuffer, m_default_sample_size);

        if (m_bits & s_mp4_tfhd_flag_default_sample_flags_present)
            write(outputBuffer, m_default_sample_flags);
    }

    /// Get the optional size
    /// \return the optional size
    uint32_t get_optional_size(uint32_t flags) const
    {
        uint32_t optional_size = 0;

        optional_size += (flags & s_mp4_tfhd_flag_base_data_offset_present) ? 8 : 0;
        optional_size += (flags & s_mp4_tfhd_flag_sample_description_index_present) ? 4 : 0;
        optional_size += (flags & s_mp4_tfhd_flag_default_sample_duration_present) ? 4 : 0;
        optional_size += (flags & s_mp4_tfhd_flag_default_sample_size_present) ? 4 : 0;
        optional_size += (flags & s_mp4_tfhd_flag_default_sample_flags_present) ? 4 : 0;

        return optional_size;
    }

    static const uint32_t s_mp4_tfhd_flag_base_data_offset_present         = 0x00001;
    static const uint32_t s_mp4_tfhd_flag_sample_description_index_present = 0x00002;
    static const uint32_t s_mp4_tfhd_flag_default_sample_duration_present  = 0x00008;
    static const uint32_t s_mp4_tfhd_flag_default_sample_size_present      = 0x00010;
    static const uint32_t s_mp4_tfhd_flag_default_sample_flags_present     = 0x00020;
    static const uint32_t s_mp4_tfhd_flag_duration_is_empty                = 0x10000;
    static const uint32_t s_mp4_tfhd_flag_default_base_is_moof             = 0x20000;

private:

    static const uint32_t s_mp4_box_type_tfhd;
    static const uint32_t s_mp4_tfhd_body_size = 4;

    uint32_t m_track_id;

    uint64_t m_base_data_offset;
    uint32_t m_sample_description_index;
    uint32_t m_default_sample_duration;
    uint32_t m_default_sample_size;
    uint32_t m_default_sample_flags;
};

const uint32_t Mp4_tfhd::s_mp4_box_type_tfhd(Mp4_box::chars_to_type('t','f','h','d'));



/**
 * @brief tfdt (track fragment decode time box).
 */
class Mp4_tfdt : public Mp4_box
{
public:

    /// Constructor
    /// \param base_media_decode_time base media decode time
    Mp4_tfdt(uint64_t base_media_decode_time)
        : Mp4_box(s_mp4_box_type_tfdt, s_mp4_full_box_header_size + s_mp4_tfdt_body_size, 1, 0), m_base_media_decode_time(base_media_decode_time)
    {
    }

    /// Serialize tfdt's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_base_media_decode_time);
    }

    /// Set the base media decode time
    /// \param base_media_decode_time base media decode time
    void set_media_decode_time(uint64_t media_decode_time)
    {
        m_base_media_decode_time = media_decode_time;
    }

private:

    static const uint32_t s_mp4_box_type_tfdt;
    static const uint32_t s_mp4_tfdt_body_size = 8;

    uint64_t m_base_media_decode_time;
};

const uint32_t Mp4_tfdt::s_mp4_box_type_tfdt(Mp4_box::chars_to_type('t','f','d','t'));



/**
 * @brief trun (track fragment run box).
 */
class Mp4_trun : public Mp4_box
{
public:

    /// Constructor
    /// \param flags trun's flags
    /// \param sample_count The number of samples in this fragment
    /// \param data_offset The data offset
    /// \param first_sample_flags The first sample flags
    Mp4_trun(uint32_t flags, uint32_t sample_count, uint32_t data_offset, uint32_t first_sample_flags)
        : Mp4_box(s_mp4_box_type_trun, s_mp4_full_box_header_size + s_mp4_trun_body_size, 0, flags), m_sample_count(sample_count), m_data_offset(data_offset), m_first_sample_flags(first_sample_flags)
    {
        m_size +=
                ((m_bits & s_mp4_trun_data_offset_present) ? 4 : 0) +
                ((m_bits & s_mp4_trun_first_sample_flags_present) ? 4: 0) +
                ((m_bits & s_mp4_trun_sample_duration_present) ? 4 : 0) +
                ((m_bits & s_mp4_trun_sample_size_present) ? 4 : 0) +
                ((m_bits & s_mp4_trun_sample_flags_present) ? 4 : 0) +
                ((m_bits & s_mp4_trun_sample_composition_time_offsets_present) ? 4 : 0);
    }

    /// Serialize trun's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        write(outputBuffer, m_sample_count);

        if (m_bits & s_mp4_trun_data_offset_present)
            write(outputBuffer, m_data_offset);

        if (m_bits & s_mp4_trun_first_sample_flags_present)
            write(outputBuffer, m_first_sample_flags);

        if (m_bits & s_mp4_trun_sample_duration_present)
            write(outputBuffer, m_sample_duration);

        if (m_bits & s_mp4_trun_sample_size_present)
            write(outputBuffer, m_sample_size);

        if (m_bits & s_mp4_trun_sample_flags_present)
            write(outputBuffer, m_sample_flags);

        if (m_bits & s_mp4_trun_sample_composition_time_offsets_present)
            write(outputBuffer, m_sample_composition_time_offset);
    }

    /// Set data offset
    /// \param data_offset The data offset
    void set_data_offset(uint32_t data_offset)
    {
        m_data_offset = data_offset;
    }

    /// Set first sample flags
    /// \param first_sample_flags The first sample flags
    void set_first_sample_flags(uint32_t first_sample_flags)
    {
        m_first_sample_flags = first_sample_flags;
    }

    /// Set sample duration
    /// \param sample_duration The sample duration
    void set_sample_duration(uint32_t sample_duration)
    {
        m_sample_duration = sample_duration;
    }

    /// Set sample size
    /// \param sample_size The sample size
    void set_sample_size(uint32_t sample_size)
    {
        m_sample_size = sample_size;
    }

    /// Set sample flags
    /// \param sample_flags The sample flags
    void set_sample_flags(uint32_t sample_flags)
    {
        m_sample_flags = sample_flags;
    }

    /// Set sample composition time offset
    /// \param sample_composition_time_offset The sample composition time offset
    void set_sample_composition_time_offset(uint32_t sample_composition_time_offset)
    {
        m_sample_composition_time_offset = sample_composition_time_offset;
    }

    static const uint32_t s_mp4_trun_data_offset_present                     = 0x00001;
    static const uint32_t s_mp4_trun_first_sample_flags_present              = 0x00004;
    static const uint32_t s_mp4_trun_sample_duration_present                 = 0x00100;
    static const uint32_t s_mp4_trun_sample_size_present                     = 0x00200;
    static const uint32_t s_mp4_trun_sample_flags_present                    = 0x00400;
    static const uint32_t s_mp4_trun_sample_composition_time_offsets_present = 0x00800;

private:

    static const uint32_t s_mp4_box_type_trun;
    static const uint32_t s_mp4_trun_body_size = 4;

    uint32_t m_sample_count;

    // Optional fields
    uint32_t m_data_offset;
    uint32_t m_first_sample_flags;

    uint32_t m_sample_duration;
    uint32_t m_sample_size;
    uint32_t m_sample_flags;
    uint32_t m_sample_composition_time_offset;
};

const uint32_t Mp4_trun::s_mp4_box_type_trun(Mp4_box::chars_to_type('t','r','u','n'));



/**
 * @brief mdat (media data box).
 */
class Mp4_mdat : public Mp4_box
{
public:

    /// Constructor
    /// \param data Pointer to the data buffer
    /// \param data_size The data buffer's size
    Mp4_mdat(uint8_t* data, uint32_t data_size)
        : Mp4_box(s_mp4_box_type_mdat, s_mp4_box_header_size + data_size), m_data(data)
    {
    }

    /// Serialize mdat's body
    void serialize_body(std::vector<uint8_t>& outputBuffer) const
    {
        // Hint from Stefan Schoenefeld: Skip the first four bytes to work with Chrome
        int offset = 4;
        int data_size = static_cast<int>(get_size()) - s_mp4_box_header_size;

        if (m_data && data_size >= offset)
        {
            uint32_t s = data_size - offset;
            write<uint32_t>(outputBuffer, s);

            for (uint32_t i = 0; i < s; ++i)
                write<uint8_t>(outputBuffer, m_data[offset + i]);
        }
    }

private:

    static const uint32_t s_mp4_box_type_mdat;

    uint8_t* m_data;
};

const uint32_t Mp4_mdat::s_mp4_box_type_mdat(Mp4_box::chars_to_type('m','d','a','t'));




/**
 * @brief Destructor.
 */
MP4::~MP4()
{
    if (m_moof)
        delete m_moof;
}


/**
 * @brief Wraps a single frame.
 * @param inputFrameH264
 * @param width
 * @param height
 * @param outputBuffer
 */
void MP4::Wrap(const std::vector<uint8_t>& inputFrameH264, uint32_t width, uint32_t height, std::vector<uint8_t>& outputBuffer)
{
    /*
     * First frame initialization
     */
    if (!initialized)
    {
        Mp4_box moov(Mp4_box::s_mp4_box_type_moov);

        moov.add_box(new Mp4_mvhd(0, 0, 1000, 0));

        Mp4_box* mvex = new Mp4_box(Mp4_box::s_mp4_box_type_mvex);
        mvex->add_box(new Mp4_mehd(0));
        mvex->add_box(new Mp4_trex(m_track_id, 1, 0, 0, 0)); // sample description id: 1
        moov.add_box(mvex);

        Mp4_box* trak = new Mp4_box(Mp4_box::s_mp4_box_type_trak);
        trak->add_box(new Mp4_tkhd(m_track_id, width, height));

        Mp4_box* mdia = new Mp4_box(Mp4_box::s_mp4_box_type_mdia);
        mdia->add_box(new Mp4_mdhd(120, "eng"));
        mdia->add_box(new Mp4_hdlr(Mp4_hdlr::s_mp4_hdlr_video_handler, "NVIDIA MPEG4 container"));

        Mp4_dref* dref = new Mp4_dref();
        dref->add_box(new Mp4_data_entry_url(""));

        Mp4_box* dinf = new Mp4_box(Mp4_box::s_mp4_box_type_dinf);
        dinf->add_box(dref);

        Mp4_box* minf = new Mp4_box(Mp4_box::s_mp4_box_type_minf);
        minf->add_box(new Mp4_vmhd(0x000001));
        minf->add_box(dinf);

        Mp4_box* stbl = new Mp4_box(Mp4_box::s_mp4_box_type_stbl);
        stbl->add_box(new Mp4_stsd(width, height));
        stbl->add_box(new Mp4_stsz(0, 0));
        stbl->add_box(new Mp4_stsc(0));
        stbl->add_box(new Mp4_stts(0));
        stbl->add_box(new Mp4_stco(0));

        minf->add_box(stbl);
        mdia->add_box(minf);

        trak->add_box(mdia);
        moov.add_box(trak);

        m_moof = new Mp4_box(Mp4_box::s_mp4_box_type_moof);
        Mp4_mfhd* mfhd = new Mp4_mfhd(m_seqno);
        m_moof->add_box(mfhd);

        Mp4_box* traf = new Mp4_box(Mp4_box::s_mp4_box_type_traf);

        uint32_t flags = Mp4_tfhd::s_mp4_tfhd_flag_default_base_is_moof |
                         Mp4_tfhd::s_mp4_tfhd_flag_default_sample_flags_present |
                         Mp4_tfhd::s_mp4_tfhd_flag_default_sample_size_present |
                         Mp4_tfhd::s_mp4_tfhd_flag_default_sample_duration_present;
        m_tfhd = new Mp4_tfhd(flags, m_track_id, 0, 1, 1, 0, 0x01010000);
        traf->add_box(m_tfhd);

        m_tfdt = new Mp4_tfdt(m_seqno);
        traf->add_box(m_tfdt);

        flags = Mp4_trun::s_mp4_trun_data_offset_present |
                Mp4_trun::s_mp4_trun_sample_size_present |
                Mp4_trun::s_mp4_trun_first_sample_flags_present;
        m_trun = new Mp4_trun(flags, 1, 0x0000008, 0x2000000);
        traf->add_box(m_trun);
        m_moof->add_box(traf);

        moov.serialize(outputBuffer);
        this->initialized = true;
    }



    /*
     * Encode frame
     */
    uint64_t moof_size = m_moof->get_size();
    uint64_t data_size = inputFrameH264.size();

    Mp4_mdat* mdat = new Mp4_mdat((uint8_t*) inputFrameH264.data(), static_cast<uint32_t>(data_size));

    m_tfdt->set_media_decode_time(m_seqno++);
    m_trun->set_data_offset(static_cast<uint32_t>(moof_size + 8));
    m_trun->set_sample_size(static_cast<uint32_t>(data_size));

    m_tfhd->set_default_sample_size(static_cast<uint32_t>(inputFrameH264.size()));
    m_tfhd->set_default_sample_flags(0x01010000);

    outputBuffer.reserve(moof_size + data_size); // TODO enough?

    m_moof->serialize(outputBuffer);
    mdat->serialize(outputBuffer); // TODO this copies the raw H.264 data into the wrapped buffer... in a real application we should avoid unnecessary copying

    delete mdat;
}




/**
 * @brief Encode and append output to file.
 * @param inputFrameH264
 * @param width
 * @param height
 * @param path
 */
void MP4::WrapToFile(const std::vector<uint8_t>& inputFrameH264, uint32_t width, uint32_t height, const std::string& path)
{
    std::vector<uint8_t> buffer;
    this->Wrap(inputFrameH264, width, height, buffer);

    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::app);
    out.write((char*) buffer.data(), buffer.size());
    out.close();
}

