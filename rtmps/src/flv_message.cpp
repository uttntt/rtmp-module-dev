#include <algorithm>
#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>

#include "flv_message.h"
#include "flv_util.h"
#include "parsingflv.h"

#include "rtmpmodulelogger.h"

using namespace castis::streamer;
using namespace flv;
using namespace parsingflv;
using namespace flv_util;

namespace castis {
namespace streamer {

std::string to_string(MediaPublishEs* es) {
  std::vector<std::string> media_type{"unknown", "video", "audio"};

  std::ostringstream oss;
  oss << "frameNumber[" << es->frameNumber << "],";
  oss << "type[" << media_type.at(static_cast<uint16_t>(es->type)) << "],";
  oss << "isKeyFrame[" << static_cast<uint16_t>(es->isKeyFrame) << "],";
  oss << "dts[" << es->dts << "],";
  oss << "pts[" << es->pts << "],";
  oss << "size[" << es->size << "],";
  oss << std::endl << "media_data[" << to_hex(*es->data, 16) << "]";
  return oss.str();
}

std::string to_string(media_publish_es_context_ptr& context) {
  std::vector<std::string> publish_state{"begin", "init", "publishing", "end"};

  std::ostringstream oss;
  oss << "publish_id[" << context->publish_id_ << "],";
  oss << "publish_state["
      << publish_state.at(static_cast<uint16_t>(context->publish_state_))
      << "],";
  oss << "frame_number[" << context->frame_number_ << "],";
  oss << "video_frame_number[" << context->video_frame_number_ << "],";
  oss << "audio_frame_number[" << context->audio_frame_number_ << "],";
  if (context->media_es_.back().get()) {
    oss << "es_frame_data exist";
  } else {
    oss << "no es_frame_data";
  }

  return oss.str();
}

bool ready_to_send(media_publish_es_context_ptr& context) {
  if (not context->audio_init_es_.codecInfo.empty() and
      not context->video_init_es_.codecInfo.empty() and
      context->video_frame_number_ >= READY_TO_SEND_VIDEO_FRAME_COUNT and
      context->audio_frame_number_ >= READY_TO_SEND_AUDIO_FRAME_COUNT) {
    return true;
  }

  return false;
}

bool end_of_video_es(media_publish_es_context_ptr& context) {
  if (not context->video_eos_.empty()) {
    return true;
  }
  return false;
}

media_publish_es_ptr make_media_es(media_es_type_t type,
                                   flv_util::buffer_t* const data,
                                   uint64_t frame_number, uint8_t is_key_frame,
                                   uint64_t pts, uint64_t dts, uint32_t size,
                                   int& ec) {
  ec = 0;
  if (type == video) {
    media_publish_es_ptr es = std::make_shared<MediaPublishEs>(data);
    es->type = video;
    es->frameNumber = frame_number;
    es->trackId = static_cast<uint8_t>(video);
    es->pts = pts;
    es->dts = dts;
    es->size = size;

    return es;
  } else if (type == audio) {
    media_publish_es_ptr es = std::make_shared<MediaPublishEs>(data);
    es->type = audio;
    es->frameNumber = frame_number;
    es->trackId = static_cast<uint8_t>(audio);
    es->pts = pts;
    es->dts = dts;
    es->size = size;

    return es;
  }

  ec = 1;
  return media_publish_es_ptr(nullptr);
}

}  // namespace streamer
}  // namespace castis

namespace flv_message {

bool process_flv_es_message(media_publish_es_context_ptr& context,
                            uint8_t message_type, uint64_t timestamp,
                            uint32_t message_length,
                            unsigned char* const buffer,
                            std::size_t const buffer_size, int& ec) {
  ec = 0;
  unsigned char* bf = buffer;
  std::size_t len = buffer_size;

  if (message_type == FLV_AUDIO_ES) {
    // if (is_aac_sequence_header(bf)) {
    if (IsAACSequenceHeader(bf, buffer_size)) {
      RTMPLOGF(info, "audiospecificconfig[%1%]", to_hex(buffer, len));

      unsigned char* testbf = buffer;
      std::size_t testlen = buffer_size;
      flv::AACAudioData audio_data;
      flv::AudioSpecificConfig audio_spec_config;
      bool test = ParseAACSequenceHeader(testbf, audio_data, audio_spec_config,
                                         testlen);
      if (test) {
        RTMPLOGF(debug, "flv_audio_data[%1%],flv_audio_spec_config[%2%]",
                 audio_data.ToString(), audio_spec_config.ToString());
      }

      uint32_t object_type, sample_rate, channel_count;
      if (get_aac_header(bf, object_type, sample_rate, channel_count, len)) {
        RTMPLOGF(debug,
                 "audiospecificconfig ,object_type[%1%], "
                 "sample_rate[%2%],channel_count[%3%]",
                 object_type, sample_rate, channel_count);

        context->audio_init_es_.sample_rate = sample_rate;
        context->audio_init_es_.channel_count = channel_count;

        std::vector<unsigned char> codec_info;
        std::vector<unsigned char> sv = to_vector(sample_rate);
        std::vector<unsigned char> cv = to_vector(channel_count);

        codec_info.insert(codec_info.begin(), cv.begin(), cv.end());
        codec_info.insert(codec_info.begin(), sv.begin(), sv.end());
        context->audio_init_es_.codecInfo = codec_info;
      } else {
        ec = 1;
        RTMPLOGF(error,
                 "failed to get aac sequence header(AudioSpecificConfig) ");
        return false;
      }
      // } else if (is_aac_audio(bf)) {
    } else if (IsAACRaw(bf, buffer_size)) {
      // experimental
      {
        unsigned char* testbf = buffer;
        std::size_t testlen = buffer_size;
        flv::AACAudioData audio_data;
        buffer_t audio_raw(len);
        bool test = ParseAACRaw(testbf, audio_data, audio_raw, testlen);
        if (test) {
          RTMPLOGF(debug, "flv_audio_data[%1%],flv_audio_raw[%2%]",
                   audio_data.ToString(), to_hex(audio_raw));
        }
      }

      buffer_t* audio_es = new buffer_t(len);
      flv::audio_frame_t frame;
      if (get_audio(bf, *audio_es, frame, len)) {
        uint64_t dts = timestamp * 90;
        uint64_t pts = dts;

        media_publish_es_ptr media_es = make_media_es(
            castis::streamer::audio, audio_es, context->frame_number_ + 1, 1,
            pts, dts, audio_es->len_, ec);

        context->media_es_.push_back(media_es);
        ++context->frame_number_;
        ++context->audio_frame_number_;
      } else {
        ec = 1;
        RTMPLOGF(error, "failed to get aac data ");
        delete audio_es;
        return false;
      }
    } else {
      ec = 1;
      RTMPLOGF(error,
               "failed to parse aac data. messge_type[%1%],. "
               "timestamp[%2%],message_length[%3%], messge[%4%]",
               to_hex(message_type), timestamp, message_length,
               to_hex(buffer, buffer_size));
      return false;
    }
  } else if (message_type == FLV_VIDEO_ES) {
    if (IsAVCSequenceHeader(buffer, buffer_size)) {
      // if (is_avc_sequence_header(bf)) {
      RTMPLOGF(info, "avc sequence header(AVCDecoderConfigurationRecord)[%1%]",
               to_hex(buffer, len));

      //experimental
      // {
      //   unsigned char* testbf = buffer;
      //   std::size_t testlen = buffer_size;

      //   flv::AVCVideoData video_data;
      //   flv::AVCDecoderConfigurationRecord avc_decoder_config;
      //   bool test = ParseAVCSequenceHeader(testbf, video_data,
      //                                      avc_decoder_config, testlen);
      //   if (test) {
      //     RTMPLOGF(debug, "flv_video_data[%1%],flv_avc_decorder_config[%2%]",
      //              video_data.ToString(), avc_decoder_config.ToString());
      //   }
      // }

      buffer_t sps(0xFFFF);
      buffer_t pps(0xFFFF);
      uint8_t nal_startcode_len;
      if (get_first_sps_and_first_pps_from_avc_sequence_header(
              bf, sps, pps, nal_startcode_len, len)) {
        context->video_init_es_.sps = to_vector(sps);
        context->video_init_es_.pps = to_vector(pps);
        context->nal_startcode_len_ = nal_startcode_len;

        std::vector<unsigned char> codec_info;

        unsigned char d[] = {0x00, 0x00, 0x00, 0x01};
        codec_info.insert(codec_info.begin(),
                          context->video_init_es_.pps.begin(),
                          context->video_init_es_.pps.end());
        codec_info.insert(codec_info.begin(), d, d + 4);
        codec_info.insert(codec_info.begin(),
                          context->video_init_es_.sps.begin(),
                          context->video_init_es_.sps.end());
        codec_info.insert(codec_info.begin(), d, d + 4);

        context->video_init_es_.codecInfo = codec_info;

      } else {
        ec = 1;
        RTMPLOGF(error, "failed to get avc sequence header");

        return false;
      }
      //} else if (is_avc_video(bf)) {
    } else if (IsAVCNALU(bf, buffer_size)) {
      
      // experimental
      // {
      //   unsigned char* testbf = buffer;
      //   std::size_t testlen = buffer_size;

      //   flv::AVCVideoData video_data;
      //   buffer_t video_es = buffer_t(len);
      //   bool test = ParseAVCNALU(testbf, video_data, video_es, testlen);
      //   if (test) {
      //     RTMPLOGF(debug, "flv_video_data[%1%],video_nalu[%2%]",
      //              video_data.ToString(), to_hex(video_es));
      //   }
      // }

      buffer_t* video_es = new buffer_t(len);
      flv::video_frame_t frame;
      if (get_video(bf, *video_es, frame, len)) {
        RTMPLOGF(debug, "video data ,timestamp[%1%], cts[%2%],keyframe[%3%]",
                 timestamp, frame.cts_, static_cast<int>(frame.keyframe_));

        parsingflv::replace_nalu_start_code_from_mp4_to_ts(4, *video_es);
        uint64_t dts = timestamp * 90;
        uint64_t pts = dts + frame.cts_ * 90;
        uint8_t key = frame.keyframe_;

        RTMPLOGF(debug, "video data ,dts[%1%], cts[%2%], pts[%3%], key[%4%]",
                 dts, frame.cts_ * 90, pts, static_cast<int>(key));

        if (key != 1) {
          if (context->first_video_key_frame_sent_ == false) {
            ec = 1;
            RTMPLOGF(error, "failed to get first key avc data");
            delete video_es;
            return false;
          }
        } else {
          context->first_video_key_frame_sent_ = true;
        }

        media_publish_es_ptr media_es = make_media_es(
            castis::streamer::video, video_es, context->frame_number_ + 1, key,
            pts, dts, video_es->len_, ec);

        context->media_es_.push_back(media_es);
        ++context->frame_number_;
        ++context->video_frame_number_;

        // std::cout << "avc_video parsed" << std::endl;
      } else {
        ec = 1;
        RTMPLOGF(error, "failed to get avc data");
        delete video_es;
        return false;
      }
    } else if (is_avc_end_of_sequence(bf)) {
      buffer_t eos(len);
      get_avc_end_of_sequence(bf, eos, len);
      context->video_eos_ = to_vector(eos);
    } else {
      ec = 1;
      RTMPLOGF(error,
               "failed to parse video data. messge_type[%1%],. "
               "timestamp[%2%],message_length[%3%], messge[%4%]",
               to_hex(message_type), timestamp, message_length,
               to_hex(buffer, buffer_size));
      return false;
    }
  } else {
    ec = 1;
    RTMPLOGF(error,
             "unknown message. messge_type[%1%],. "
             "timestamp[%2%],message_length[%3%], messge[%4%]",
             to_hex(message_type), timestamp, message_length,
             to_hex(buffer, buffer_size));

    return false;
  }

  return true;
}

bool read_flv_es_dump_file(media_publish_es_context_ptr& context,
                           std::string const& file_path) {
  std::cout << "open >" << file_path << std::endl;

  std::ifstream binfile(file_path, std::ios::binary);
  if (not binfile) {
    std::cout << "> open false" << std::endl;
    return false;
  }

  // get length of file:
  binfile.seekg(0, binfile.end);
  std::size_t length = binfile.tellg();
  binfile.seekg(0, binfile.beg);
  std::cout << "> file length[" << length << "]" << std::endl;
  std::size_t const buffer_size = length;
  unsigned char* buffer = new unsigned char[buffer_size];

  binfile.read(reinterpret_cast<char*>(buffer), buffer_size);
  if (not binfile) {
    std::cout << "> read false" << std::endl;
    return false;
  }

  int ec;
  bool ret = process_flv_es_dump_message(context, buffer, buffer_size, ec);

  std::cout << "process_flv_es_message. ec[" << ec << "]"
            << ",ret[" << ret << "],";
  std::cout << "context[" << to_string(context) << "]";
  std::cout << std::endl;

  binfile.close();
  delete[] buffer;
  return true;
}

// FIXME:
// flv 의 32 bit timestamp 값도 64 bit 로 변환이 필요함
bool process_flv_es_dump_message(media_publish_es_context_ptr& context,
                                 unsigned char* const buffer,
                                 std::size_t const buffer_size, int& ec) {
  ec = 0;
  unsigned char* bf = buffer;
  std::size_t len = buffer_size;

  uint8_t message_type{0};
  uint32_t timestamp{0};
  uint32_t message_length{0};
  read_1byte(bf, message_type, len);
  read_4byte(bf, timestamp, len);
  read_4byte(bf, message_length, len);

  if (message_type == FLV_AUDIO_ES) {
    if (is_aac_sequence_header(bf)) {
      uint32_t object_type;
      uint32_t sample_rate;
      uint32_t channel_count;
      if (get_aac_header(bf, object_type, sample_rate, channel_count, len)) {
        context->audio_init_es_.sample_rate = sample_rate;
        context->audio_init_es_.channel_count = channel_count;

        std::vector<unsigned char> codec_info;
        std::vector<unsigned char> sv = to_vector(sample_rate);
        std::vector<unsigned char> cv = to_vector(channel_count);

        codec_info.insert(codec_info.begin(), cv.begin(), cv.end());
        codec_info.insert(codec_info.begin(), sv.begin(), sv.end());
        context->audio_init_es_.codecInfo = codec_info;

        // std::cout << "aac_sequence_header parsed" << std::endl;
      } else {
        ec = 1;
        std::cout << "parsing aac_sequence_header fail" << std::endl;
      }
    } else if (is_aac_audio(bf)) {
      buffer_t* audio_es = new buffer_t(len);
      flv::audio_frame_t frame;
      if (get_audio(bf, *audio_es, frame, len)) {
        uint64_t dts = timestamp * 90;
        uint64_t pts = dts;
        media_publish_es_ptr media_es = make_media_es(
            castis::streamer::audio, audio_es, context->frame_number_ + 1, 1,
            pts, dts, audio_es->len_, ec);

        context->media_es_.push_back(media_es);
        ++context->frame_number_;
        ++context->audio_frame_number_;

        // std::cout << "aac_data parsed" << std::endl;
      } else {
        ec = 1;
        std::cout << "getting aac_data fail" << std::endl;
        delete audio_es;
      }
    } else {
      ec = 1;
      std::cout << "parsing audio data fail" << std::endl;
      std::cout << "message_type[" << to_hex(message_type) << "],";
      std::cout << "timestamp[" << timestamp << "],";
      std::cout << "message_length[" << message_length << "]]" << std::endl;
      std::cout << "message dump > " << to_hex(buffer, buffer_size)
                << std::endl;
      ;
    }
  } else if (message_type == FLV_VIDEO_ES) {
    if (is_avc_sequence_header(bf)) {
      buffer_t sps(0xFFFF);
      buffer_t pps(0xFFFF);
      uint8_t nal_startcode_len;
      if (get_first_sps_and_first_pps_from_avc_sequence_header(
              bf, sps, pps, nal_startcode_len, len)) {
        context->video_init_es_.sps = to_vector(sps);
        context->video_init_es_.pps = to_vector(pps);
        context->nal_startcode_len_ = nal_startcode_len;

        std::vector<unsigned char> codec_info;

        unsigned char d[] = {0x00, 0x00, 0x00, 0x01};
        codec_info.insert(codec_info.begin(),
                          context->video_init_es_.pps.begin(),
                          context->video_init_es_.pps.end());
        codec_info.insert(codec_info.begin(), d, d + 4);
        codec_info.insert(codec_info.begin(),
                          context->video_init_es_.sps.begin(),
                          context->video_init_es_.sps.end());
        codec_info.insert(codec_info.begin(), d, d + 4);

        context->video_init_es_.codecInfo = codec_info;

        // std::cout << "avc_sequence_header parsed" << std::endl;
      } else {
        ec = 1;
        std::cout << "parsing avc_sequence_header fail" << std::endl;
      }
    } else if (is_avc_video(bf)) {
      buffer_t* video_es = new buffer_t(len);
      flv::video_frame_t frame;
      if (get_video(bf, *video_es, frame, len)) {
        uint64_t dts = timestamp * 90;
        uint64_t pts = dts + frame.cts_ * 90;
        uint8_t key = frame.keyframe_;
        parsingflv::replace_nalu_start_code_from_mp4_to_ts(4, *video_es);
        media_publish_es_ptr media_es = make_media_es(
            castis::streamer::video, video_es, context->frame_number_ + 1, key,
            pts, dts, video_es->len_, ec);

        context->media_es_.push_back(media_es);
        ++context->frame_number_;
        ++context->video_frame_number_;

        // std::cout << "avc_video parsed" << std::endl;
      } else {
        ec = 1;
        std::cout << "parsing avc_video fail" << std::endl;
        delete video_es;
      }
    } else if (is_avc_end_of_sequence(bf)) {
      buffer_t eos(len);
      get_avc_end_of_sequence(bf, eos, len);
      context->video_eos_ = to_vector(eos);
    } else {
      ec = 1;
      std::cout << "parsing video data fail" << std::endl;
      std::cout << "message_type[" << to_hex(message_type) << "],";
      std::cout << "timestamp[" << timestamp << "],";
      std::cout << "message_length[" << message_length << "]]" << std::endl;
      std::cout << "message dump > " << to_hex(buffer, buffer_size)
                << std::endl;
      ;
    }
  } else {
    ec = 1;
    std::cout << "unknown message type[";
    std::cout << "message_type[" << to_hex(message_type) << "],";
    std::cout << "timestamp[" << timestamp << "],";
    std::cout << "message_length[" << message_length << "]]" << std::endl;
    std::cout << "message dump > " << to_hex(buffer, buffer_size) << std::endl;
    ;
  }

  return true;
}

void read_flv_es_dump(media_publish_es_context_ptr& context, unsigned int start,
                      unsigned int end) {
  std::string directory = "/data/project/git/rtmp-module/build/dump/flv_es";
  std::string file_name = "flv_es.dump";
  std::string file_path;

  for (unsigned index = start; index <= end; ++index) {
    file_path = directory + "/" + file_name + "." + std::to_string(index);

    read_flv_es_dump_file(context, file_path);
  }
}

}  // namespace flv_message
