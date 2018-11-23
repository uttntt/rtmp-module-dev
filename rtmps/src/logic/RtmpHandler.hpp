#ifndef RTMPHANDLER_HPP_
#define RTMPHANDLER_HPP_
#include "../network/RequestHandler.hpp"
#include "../protocol/control_message/ControlMessage.hpp"
#include "../protocol/cmd_message/ConnectMessage.hpp"
#include "../protocol/cmd_message/ReleaseStream.hpp"
#include "../protocol/cmd_message/FCPublish.hpp"
#include "../protocol/cmd_message/CreateStream.hpp"
#include "../protocol/cmd_message/Publish.hpp"
#include "../protocol/cmd_message/Play.hpp"
#include "../protocol/media_message/MediaMessage.hpp"
#include <list>

namespace rtmp_logic {
// TODO: implement StreamManager logic
//class MessageStream {
// public:
//  std::string simple_name_;
//  std::string full_name_;
//  unsigned int id_;
//  bool empty() {
//    return simple_name_.empty() && full_name_.empty();
//  }
//  bool is_same_name(const std::string& stream_name) {
//    return full_name_.compare(stream_name) == 0
//        || simple_name_.compare(stream_name) == 0;
//  }
//};
//typedef boost::shared_ptr<MessageStream> MessageStream_ptr;

class RtmpHandler : public rtmp_network::RequestHandler {
 public:
  RtmpHandler(rtmp_network::MessageSender_wptr sender, unsigned int id);
  void handle_request(rtmp_network::Message_ptr request);
  void notify_disconnect();

 private:
  unsigned int get_id();
  // TODO: implement StreamManager logic
//  MessageStream_ptr get_new_stream();
//  void remove_stream(const std::string & stream_name);

  void handle_connect_message(rtmp_protocol::ConnectMessage_ptr request);
  void handle_release_stream(rtmp_protocol::ReleaseStream_ptr request);
  void handle_fc_publish(rtmp_protocol::FCPublish_ptr request);
  void handle_fc_unpublish(rtmp_protocol::FCUnPublish_ptr request);
  void handle_create_stream(rtmp_protocol::CreateStream_ptr request);
  void handle_delete_stream(rtmp_protocol::DeleteStream_ptr request);
  void handle_publish(rtmp_protocol::Publish_ptr request);
  void handle_play(rtmp_protocol::Play_ptr request);
  void handle_media_message(rtmp_protocol::MediaMessage_ptr request);
  void handle_set_chunk_size(rtmp_protocol::SetChunkSize_ptr request);
  void handle_on_metadata();

  unsigned int chunk_size_;
  unsigned int id_;
  // TODO: implement StreamManager logic
  // bit flag => check for used stream.
  // this program allows only 256 stream.
  // Crtmpserver allows only 256 stream.
  // RTMP spec allows 4294967295(4-byte) streams.
//  std::list<MessageStream_ptr> msg_streams_;
//  bool used_msg_stream_flag_[256];
};
typedef boost::shared_ptr<RtmpHandler> RtmpHandler_ptr;

class RtmpHandlerFactory : public rtmp_network::RequestHandlerFactory {
 public:
  RtmpHandlerFactory() {
  }
  virtual ~RtmpHandlerFactory() {
  }
  virtual rtmp_network::RequestHandler_ptr get_request_handler(
      rtmp_network::MessageSender_wptr sender, unsigned int id) {
    return RtmpHandler_ptr(new RtmpHandler(sender, id));
  }
};
typedef boost::shared_ptr<RtmpHandlerFactory> RtmpHandlerFactory_ptr;
}  // namespace rtmp_logic

#endif /* RTMPHANDLER_HPP_ */
