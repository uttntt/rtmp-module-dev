## RTMP 구현

### 목차
1. RTMP (리얼 타임 메시징 프로토콜(Real Time Messaging Protocol) 란?
2. RTMP 특징
3. RTMP Protocol 
4. RTMP Message sequence

### 1. RTMP (리얼 타임 메시징 프로토콜(Real Time Messaging Protocol) 란?

Adobe(Macromedia)에서 Flash player와 server 간의 오디오, 비디오 및 기타 데이터 스트리밍을 위해서 만들었습니다.
audio 전송 메시지, video 전송 메시지, command 메시지, shared object 전송 메시지, metadata 전송 메시지, user control 메시지로 이루어져 있습니다.
실시간 방송에서 전송규격으로 많이 사용된다고 합니다.
아프리카 TV, YouTube, Facebook live-video, AWS Elemental MediaLive, Twitch 등에서 사용된다고 합니다.

![live streaming](./live_streaming_architecture_thumb.png)

### 2. RTMP 특징

스트리밍 관련하여 
publish (라이브 방송) command,
play command, seek command, pause command
play2 command (play with new bitrate) 등의 명령어들이 규격에 포함되어있습니다. 
오디오, 비디오 및 기타 데이터를 chunk로 만들어서 전송할 수 있습니다.

규격 문서에는 protocol과 일부 command sequence flow가 포함되어있고, 서버나 클라이언트가 각 명령을 어떻게 구현할 지에 대한 내용은 포함되어있지 않습니다.

play 시에 bitrate를 변경 기능 구현의 경우, 서버에 서로 다른 bitrate로 인코딩 된 비디오 파일 3-4개를 저장해두고, 클라이언트가 bitrate 변경 play를 요청하면 스트리밍 중에 다른 bitrate를 가진 파일을 스트리밍하는 방식으로 구현한다고 합니다.

### 3. RTMP Protocol 

https://www.adobe.com/devnet/rtmp.html
adobe가 2012년에 1.0 규격문서를 release 하였습니다. 완전한 버전은 아니라고 하는데, 이 문서 이후에 release 된 문서는 없는 것 같습니다.

RTMP 종류 :

기본 RTMP 이외에 여러 종이 있습니다. 
RTMP와 RTMPS 가 주로 사용되는 것 같습니다.

RTMP (기본): TCP, 1935번 포트 사용합니다.
RTMPT (RTMP Tunneled): RTMP 데이터를 HTTP로 감싼 것
RTMPS (RTMP Secure): RTMP 데이터를 HTTPS로 감싼 것
RTMPE (Encrypted RTMP): 128비트로 암호화된 RTMP
RTMPTE (Encrypted RTMP Tunneled): RTMPT, RTMPE 섞어 놓은 형태
RTMFP (Real Time Media Flow Protocol): UDP에서 동작.항상 암호화 된 상태로 데이터 전송

#### RTMP 구조 :

문서에서는 RTMP Chunk Stream 과 RTMP를 구분해서 이야기하고 있고, protocol format 이 다르기 때문에 다른 용도로 이용되는 것으로 예상을 했지만, 어떻게 구분하는지, 언제 어떤 것을 이용하는 것인지 파악하지 못했습니다.
nginx 등에서는 chunk stream 기반의 RTMP 프로토콜을 구현하고 있습니다.

접속 후, 서버와 클라이언트 간에 handshake protocol 교환이 일어나고, 
성공하면 rtmp chunk stream 기반의 통신이 이루어집니다.

##### Handshake seqence : 

클라이언트는 C0, C1을 보냅니다.
S0, S1 을 받으면 S1의 응답으로 C2를 보냅니다.
C1의 응답인 S2를 받으면 handshake 과정을 끝내고 chunk stream 을 보냅니다.

서버는 C0을 받으면 (또는 C0, C1까지 받으면) S0, S1을 보냅니다.
C1을 받으면, C1의 응답으로 S2를 보냅니다.
S1의 응답인 C2를 받으면 handshake 과정을 끝내고 데이터를 받습니다.


```puml
title : client #1
client -> server : C0
client -> server : C1
server -> client : S2
client -> server : chunk stream
```
```puml
title : client #2
client <- server : S0
client <- server : S1
client -> server : C2
```

```puml
title : server #1
client -> server : C0
client <- server : S0
client <- server : S1

client -> server : C1
client <- server : S2
```

```puml
title : server #2
client -> server : C2
client -> server : chunk stream
```

```puml
client -> network : C0
network -> server : C0
activate server
client -> network : C1
server -> network : S0
server -> network : S1
deactivate server
network -> client : S0
network -> client : S1
activate client
network -> server : C1
activate server
client -> network : C2
deactivate client
server -> network : S2
deactivate server
network -> client : S2
activate client
network -> server : C2
activate server
client -> network : chunk
deactivate client
network -> server : chunk
deactivate server
```

##### Handshake Format, Value : 

Handshake for client : C0(1 byte), C1(1536 byte), C2(1536 byte)
Handshake for server : S0(1 byte), S1(1536 byte), S2(1536 byte)

CO(1 byte) : rtmp version(1 byte)
SO(1 byte) : rtmp version(1 byte)

C1(1536 byte) : time(4 byte) | zero(4 byte) | random bytes(1528 byte)
S1(1536 byte) : time(4 byte) | zero(4 byte) | random bytes(1528 byte)

C2(1536byte) : time(4 byte) | time2(4 byte) | random echo(1528 byte)
S2(1536byte) : time(4 byte) | time2(4 byte) | random echo(1528 byte)

randomBytes can be : key(764byte) digest(764byte)

C0값(1byte)으로 rtmp protocol version 을 보냅니다. 규격에서는 client 와 server 모두 version 값으로 3 을 사용해야합니다. vesion 값 1,2 는 예전에 사용했었고, 4 이후 값은 나중에 사용할 값으로 정의하였습니다. ffmpeg 의 경우 version값으로 3을 사용합니다.

C1, S1 의 time 값은 시간 epoch 값을 보냅니다. (client 가 C1을 보낸 시간, server가 S1 을 보낸 시간 값인 것 같습니다.)
이 값은 0 일 수도 있고, 임의의 값일 수도 있고, chunkstream 의 timestamp 값일 수도 있습니다. 
다음으로 0 값(4byte) 과 random값(1528byte)을 보냅니다. 

C1의 timestamp 에 대한 내용은 원문 해석이 잘 안되어 인용합니다.
``` 
Time (4 bytes) : This field contains a timestamp, which SHOULD be 
used as the epoch for all future chunks sent from this endpoint. 
This may be 0, or some arbitary value. To synchronize multiple 
chunkstreams, endpoint may wish to send the current value of 
the other chunkstreams's timestamp.
```

C2: S1의 응답입니다.
time 은 S1의 time 값,
time2 는 S1을 읽은 time 값,
random echo값은 s1의 random 값이 들어갑니다.

S2: C1의 응답
time 은 C1의 time 값,
time2 는 C1을 읽은 time 값,
random echo값은 C1의 random 값이 들어갑니다.

handshake 의 timestamp 값을 어디에 사용해야하는 지에 대해서
예시만 나올 뿐 꼭 필요한 부분을 밝히진 않고 있습니다. 

스펙에 나온 예시 입니다.
C2의 time2 - time 은 
client가 S1을 읽은 시간 - server가 S1 을 보낸 시간이 되서 client 의 처리 시간을 알 수도 있습니다만, 사용될 것 같지는 않습니다.

S2 의 time2 - time 은
server가 C1을 읽은 시간 - client가 C1을 보낸 시간이 되어 server의 처리 시간을 알 수도 있습니다. 사용될 것 같지는 않습니다.

* S2의 예에서 time 은 client 가 C1를 보낼 때의 time 값이고, time2는 server가 C1을 읽은 시간인데 client 와 server 의 시간 기준이 갖지 않으면 의미가 없습니다. 
둘 간의 시간을 맞추든지, S2의 time2 를 C1 의 time을 기준으로 해서 차이값을 
계산해서 보낼 수도 있을 것 같습니다.


그리고, timestamp 에 대해서 구현 시에 주의할 점이 spec 에 나와 있습니다.
Timezone 에 대한 이야기는 없습니다. 상대적인 시간 값으로 생각하는 것 같습니다.
이 부분도 원문 해석이 잘 안되는 부분이 있어서 인용합니다.
```
Timestamps in RTMP are given as an integer number of milliseconds
relative to an unspecified epoch. Typically, each stream will start
with a timestamp of 0, but this is not required, as log as the two
endopoints agree on the epoch. Note that this means that any
synchronization across multiple streams (especially from separate
hosts) requires some additional mechanism outside of RTMP.

Because timestamps are 32 bits long, they roll over every 49 datys, 17 
hours, 2 minutes and 47.296 seconds. Because streams are allowed to 
continuously, potentially for years on end, an RTMP application
SHOULD use serial number arithmetic [RFC1982] when processing
timestamps, and SHOLD be capable of handling wraparound. For 
example, an application assumes that alll adjacent timestamps are 
within 2^31-1 milliseconds of each other, so 10000 comes after 
4,000,000,000 ,and 3,000,000,000 comes before 4,000,000,000
```

##### Handshake 구현

ffmpeg 의 경우 C1의 time 값으로 0을 사용하고 있습니다.
그러나, zero 필드에는 version 정보(9, 0, 124, 2)를 그대로 보내고 있습니다. spec 에는 없는 내용으로 googling 해보았으나 아직 관련 내용을 못찾았습니다.
ffmpeg 의 경우에 C1 의 random data 값에 암호화 한 값을 보내고 있는 것 같습니다.
이 부분도 spec 에는 없는 내용입니다. ffmpeg 의 소스 코드의 comment 를 보았을 때,
RTMPE 와 관련이 있어보입니다만,  RTMPE 에 대해서는 찾아보지 않았습니다.
그렇지만 S1, S2 를 spec 대로 구현하는 경우에도 연동이 되고, 
nginx-rtmp-module 식으로 구현해도 연동이 됩니다.

nginx-rtmp-module 의 handshake 구현은 얼핏 보기엔 handshake 구현이 spec 과 좀 다릅니다.
client 가 보낸 C1 을 S1, S2 에 그대로 복사해서 보내는 것 같습니다.
(spec 상으로는 이렇게 해도 별로 상관없어 보입니다.)

ffmpeg 에도 nginx-rtmp-moduel 과 같은 handshake 방식도 구현이 되어있는 것 같습니다만,
관련 spec 을 찾지는 못햇습니다.

goCoder client 의 경우, C1의 time 값에 0이 아닌 값을 보내고 있고 zero 필드에도 
0 이 아닌 값을 보내고 있습니다. 
goCoder 의 경우에는 C1의 random data 값으로 암호화한 값을 보내고 있지 않습니다.
(즉, ffmpeg 과 연동할 때 처럼 암호화를 검사하면 에러가 납니다.)
그러나 S0, S1, S2 를 보내도 응답이 없어서 연동에 실패했습니다.

##### RTMP Chunk Stream Format :

ChunkStream : ChunkHeader ChunkData
ChunkHeader : BasicHeader MessageHeader ExtendedTimestamp
BasicHeader(1b or 2b or 3b) : ChunkType(1b) ChunckStreamID(0b or 1b or 2b)
ChunkType0_MessageHeader(11b): Timestamp(3b) MessageLength(3b) MessageTypeId(1b) MessageStreamId(4b)
ChunkType1_MessageHeader(7b): TimestampDelta(3b) MessageLength(3b) MessageTypeId(1b) 
ChunkType2_MessageHeader(3b) : TimestampDelta(3b)
ChunkType3_MessageHeader(0) : NULL

ChunkType 0 Header : 11 byte
1. chunk stream 의 제일 처음에 사용됩니다.
2. stream 의 timestamp 가 뒤로 갈 때 사용됩니다. (예를 들어 backward seek 때문에)

ChunkType 1 Header: 7 byte
1. message stream id 가 생략됩니다. 
2. message length, message type 은 다르고, message stream id 는 이전 chunk 의 것과 같은 경우에 사용됩니다.

ChunkType 2 Header: 3 byte
1. message length, message type, message stream id 와  가 생략됩니다. 이전 chunk 와 같은 length, type, id 를 갖는 경우에 사용됩니다.

ChunkType 3 Header: 0 byte
1. 따로 추가 header 정보가 없습니다. 이전 stream 과 모든 정보가 같은 경우에 사용됩니다.

##### two examples in spec 
1. chunck size 보다 작은 같은 length, type 의 message 를 보내는 경우

|  | message stream id | message type | time | length |
|----|-------------------|--------------|------|--------|
|msg1| 12345 | 8 | 1000 | 32 |
|msg2| 12345 | 8 | 1020 | 32 |
|msg3| 12345 | 8 | 1040 | 32 |
|msg4| 12345 | 8 | 1060 | 32 |

---->
chunk
|  | chunk id | chunk type | data | payload length |
|----|----------|------------|------|----------------------|
|chunk1 | 3 | 0 | delta:1000, msg_length:32, msg_type:8, msg_stream id : 12345 | 32 |
|chunk2 | 3 | 2 | delta:20 | 32 |
|chunk3 | 3 | 3 |  | 32 |
|chunk4 | 3 | 3 |  | 32 |

2. chunck size (128 byte) 를 넘는 크기의 message 를 보내는 경우

|  | message stream id | message type | time | length |
|----|-------------------|--------------|------|--------|
|msg1| 12345 | 9 | 1000 | 307 |

---->
chunk
|  | chunk id | chunk type | data | payload length |
|----|----------|------------|------|----------------------|
|chunk1 | 3 | 0 | delta:1000, msg_length:307, msg_type:9, msg_stream id : 12345 | 128 |
|chunk2 | 3 | 3 |  | 128 |
|chunk3 | 3 | 3 |  | 51 |


##### RTMP Message Format :

Message : MessageHeader(11b) Payload
MessageHeader : MesageTypeId(1b) PayloadLength(3b) Timestamp(4b) MessageStreamId(3b)

##### messageTypeId 에 사용되는 값의 예: 
- 0x01 = Set Chunk Size Message
- 0x02 = Abort Message
- 0x03 = Acknowledgement Message
- 0x04 = User control Message ( StreamBegin, StreamEOF, PingReuest, PingResponse,  etc.)
- 0x05 = Window Acknowledgement Size Message
- 0x06 = Set Peer Bandwidth Message
- 0x08 = Audio Data
- 0x09 = Video Data
- 0x0F(15) = AMF3 type Data Message (Metadata, etc.)
- 0x10(16) = AMF3 type Shared object Message
- 0x11(17) = AMF3 type command Message
- 0x12(18) = AMF0 type Data Message (Metadata, etc.)
- 0x13(19) = AMF0 type Shared object Message
- 0x14(20) = AMF0 type command Message
- 0x16(22) = Aggregate Message

##### Protocol Control Message :
Set Chunk Size, Abort Message, Acknowledgement Message, Window Acknowledgement Size Message 은 control message 라고 합니다.
ChunkStreamId:2로 MessageStreamId:0 으로 정해져있습니다.
해당 message 의 경우, ChunkData 에 들어가는 값에 대한 설명은 규격에 정해져있습니다.

##### User Control Message : messageTypeId : 4
ChunkStreamId:2로 MessageStreamId:0 으로 정해져있습니다.

##### RTMP Command Message : messageTypeId : 20, 17
AMF 규격을 사용합니다.
AMF type Message의 경우 CommandName, TransactionID, Parameter 등의 정보가 payload 에 AFM 규격에 따라 인코딩되어서 들어갑니다.

##### Data Message : messageTypeId : 15, 18
Metadata, user data 를 보낼 때 사용합니다.
AMF 규격을 사용합니다.

##### AMF규격 (AMF0, AMF3 규격) : 
ActionMessageFormat:Adobe Actionscript Object serialization 규격, string, number, obejct(=struct)를 serialize 하기위한 규격입니다.

##### MessageStreamId 의 값:
RTMP ChunkStream Format에 들어갈 때는 little-endian order 입니다.
RTMP Message Format에 들어갈 때는 big-endian order 입니다.

##### Wireshark RTMP 지원:
RTMP : 지원되는 것 같습니다. 전부 지원하는 것 같지는 않음. parsing 을 제대로 못하는 경우가 있음
AMF : 지원되는 것 같습니다. parsing 을 제대로 못하는 경우가 있는 것으로 보임

### 4. RTMP Message sequence

RTMP 통신은 다음과 같은 sequence 로 진행됩니다.

#### Play, Play2
1. TCP connect
2. Handshake
3. Connect 
4. CreateStream 
5. Play 
6. Audio / Video Data 
7. Play2
8. Audio / Video Data 
7. CloseStream 

#### Publish recorded video
1. TCP connect
2. Handshake
3. Connect 
4. CreateStream 
5. Publish
6. Audio / Video Data 
7. CloseStream 

#### Publish metadata from recorded stream  
1. TCP connect
2. Handshake
3. Connect 
4. CreateStream
5. Publish 
6. Metadata Data
7. CloseStream 

#### BroadCast a shared object 
1. TCP connect
2. Handshake
3. Connect 
4. Shared object event

### RTMP 상세 message sequence :

#### Handshake
![handshake](./rtmp_handshake.png)

#### Connect
![connect](./rtmp_connect.png)

#### Play, Play2
![Play](./rtmp_play.png)

#### Play2
![Play2](./rtmp_play2.png)

#### Publish recorded video
![Publish](./rtmp_publish.png)

#### Publish metadata from recorded stream  
![Publish metadata](./rtmp_publish_metadata.png)

#### 5. RTMP 구현

5.1 nginx-rtmp-module :
성능 튜닝??? 등은 없었습니다.

1. ffmpeg 과 연동:
요청 :
ffmpeg -re -i /data/750k.mp4 -acodec aac -vcodec copy -f flv rtmp://localhost/myapp/test
- myapp 이 ngix 설정에 들어 있어야 함
- stream name 이 hls 요청에 사용됨

hls url:
http://localhost:8080/hls/test.m3u8

2. OBS 과 연동:
- 지연은 15~20초 정도입니다. (note book test 환경. 무선 인터넷)

요청:
rtmp://localhost/myapp
stream key: pc

hls url:
http://localhost:8080/hls/pc.m3u8

3. gocoder 와 연동: 
연동 실패 합니다. 
server가 S0, S1, S2 를 보내고, 
gocoder 로 부터 C2 응답이 오지 않습니다.
- rtmp 모듈과 같은 현상입니다.

요청:
rtmp://localhost/
application: myapp
stream name: gocoder

hls url:
http://localhost:8080/hls/gocoder.m3u8


5.2 rtmp 연동 모듈: 
지연은 15 ~ 20 초 정도입니다. ngix-rtmp-module 과 비슷한 수준인 것 같습니다.

OBS 와 연동
요청:
rtmp://172.16.33.52/myapp
stream key: pc

hls url:
http://172.16.33.52:8080/brokering/live/pc/master.m3u8

brokering 은 streamer 설정에 필요
현재 brokering 은 rtmp-module 에 
hard-coding 되어 있는데, 
ngix-rtmp-moduel 처럼
rtmp app 으로 받은 값을 streamer 의 provider 값으로 사용할 수 있도록 
수정이 필요할 것 같습니다.


OBS 연동의 경우, message sequence 

1. Handshake
  C1 의 zero 필드에 0x00000000 을 보냅니다. 
  handshake 는 ffmpeg 방식으로 해도되고, spec 에 있는 방식으로 해도 됩니다.
2. SetChunkSize 
  SetChunkSize Message 가 제일 먼저 옵니다. 
  default size 는 128 인데, OBS 의 경우, 이 값을 4096 으로 setting 하고 시작합니다.
3. Connect(app)
  RTMP url 의 경우, 
  rtmp://ip:poort/app/stream_name 식으로 사용합니다.
  여러 필드 값이 있지만, app 필드 값이 중요한 값인 것 같습니다. 
  url 의 app 값이 Connect parameter 로 넘어옵니다.
  nginx-rtmp-module 과 연동할 때는 app 값이 서버 설정에 있어야 연동이 됩니다.

4. server : WindowAcknowledgementSize
5. server : SetPeerBandwidth
6. server : StreamBegin
7. server : ConnectResult
8. ReleaseStream (stream name)
  stream name 값으로 rtmp url 의 leaf 값이 전달됩니다. 
9. server : SimpleResult
10. FCPublish (stream name)
11. server : SimpleResult
12. CreateStream(stream name)
13. server : SimpleResult(stream id)
14. server : OnFCPublish
15. Publish(stream name) 
16. server : SimpleResult
17. server : OnStatus(...,clientid)
18. @setDataFrame, OnMetadata
19. Audio
20. video
21. FCUnpublish(stream name)
22. server : SimpleResult
23. DeleteStream(stream id) 
24. server : SimpleResult

chunk stream id 는 client 가 정해서 보냅니다만, 
message stream id 의 경우는 
CreateStream 요청에 대한 답으로 server 가 steram id 값을 보내면, 
client 가 그 이후의 message 의 stream id 를 server 가 보낸 값으로 보냅니다.








