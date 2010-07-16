/* 
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifdef HAS_PROTOCOL_RTP
#include "protocols/rtp/rtspprotocol.h"
#include "application/baseclientapplication.h"
#include "protocols/rtp/basertspappprotocolhandler.h"
#include "protocols/protocolmanager.h"
#include "streaming/streamstypes.h"
#include "streaming/baseinnetstream.h"
#include "protocols/rtp/streaming/outnetrtpudph264stream.h"
#include "protocols/rtp/streaming/innetrtpstream.h"
#include "protocols/protocolfactorymanager.h"
#include "protocols/rtp/inboundrtpprotocol.h"
#include "netio/netio.h"
#include "protocols/rtp/connectivity/outboundconnectivity.h"
#include "protocols/rtp/connectivity/inboundconnectivity.h"

#define RTSP_STATE_HEADERS 0
#define RTSP_STATE_PAYLOAD 1
#define RTSP_MAX_LINE_LENGTH 256
#define RTSP_MAX_HEADERS_COUNT 64
#define RTSP_MAX_HEADERS_SIZE 2048
#define RTSP_MAX_CHUNK_SIZE 1024*128

RTSPProtocol::RTSPProtocol()
: BaseProtocol(PT_RTSP) {
	_state = RTSP_STATE_HEADERS;
	_contentLength = 0;
	_requestSequence = 0;
	_pOutboundConnectivity = NULL;
	_pInboundConnectivity = NULL;
}

RTSPProtocol::~RTSPProtocol() {
	CloseOutboundConnectivity();
	CloseInboundConnectivity();
}

IOBuffer * RTSPProtocol::GetOutputBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_outputBuffer))
		return &_outputBuffer;
	return NULL;
}

bool RTSPProtocol::Initialize(Variant &parameters) {
	GetCustomParameters() = parameters;
	return true;
}

void RTSPProtocol::SetApplication(BaseClientApplication *pApplication) {
	BaseProtocol::SetApplication(pApplication);
	_pProtocolHandler = (BaseRTSPAppProtocolHandler *)
			_pApplication->GetProtocolHandler(GetType());
	if (_pProtocolHandler == NULL) {
		FATAL("Protocol handler not found");
		EnqueueForDelete();
	}
}

bool RTSPProtocol::AllowFarProtocol(uint64_t type) {
	return type == PT_TCP;
}

bool RTSPProtocol::AllowNearProtocol(uint64_t type) {
	return type == PT_INBOUND_RTP;
}

bool RTSPProtocol::SignalInputData(int32_t recvAmount) {
	NYIR;
}

SDP &RTSPProtocol::GetInboundSDP() {
	return _inboundSDP;
}

bool RTSPProtocol::SignalInputData(IOBuffer &buffer) {
	while (GETAVAILABLEBYTESCOUNT(buffer) > 0) {
		switch (_state) {
			case RTSP_STATE_HEADERS:
			{
				if (!ParseHeaders(buffer)) {
					FATAL("Unable to read response headers");
					return false;
				}
				if (_state != RTSP_STATE_PAYLOAD) {
					//FINEST("Not enough data to parse the headers");
					return true;
				}
			}
			case RTSP_STATE_PAYLOAD:
			{
				if ((bool)_inboundHeaders["rtpData"]) {
					if (_pNearProtocol == NULL) {
						FATAL("This is the near most protocol");
						return false;
					}
					if (!_pNearProtocol->SignalInputData(buffer)) {
						FATAL("Unable to signal upper protocols");
						return false;
					}
				} else {
					if (!HandleRTSPMessage(buffer)) {
						FATAL("Unable to handle content");
						return false;
					}
				}
				_state = RTSP_STATE_HEADERS;
				break;
			}
			default:
			{
				ASSERT("Invalid RTSP state");
				return false;
			}
		}
	}

	return true;
}

void RTSPProtocol::ClearRequestMessage() {
	_requestHeaders.Reset();
	_requestContent = "";
}

void RTSPProtocol::PushRequestFirstLine(string method, string url,
		string version) {
	_requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD] = method;
	_requestHeaders[RTSP_FIRST_LINE][RTSP_URL] = url;
	_requestHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = version;
}

void RTSPProtocol::PushRequestHeader(string name, string value) {
	_requestHeaders[RTSP_HEADERS][name] = value;
}

void RTSPProtocol::PushRequestContent(string outboundContent, bool append) {
	if (append)
		_requestContent += "\r\n" + outboundContent;
	else
		_requestContent = outboundContent;
}

bool RTSPProtocol::SendRequestMessage() {
	//1. Put the first line
	_outputBuffer.ReadFromString(format("%s %s %s\r\n",
			STR(_requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD]),
			STR(_requestHeaders[RTSP_FIRST_LINE][RTSP_URL]),
			STR(_requestHeaders[RTSP_FIRST_LINE][RTSP_VERSION])));

	//2. Put our request sequence in place
	_requestHeaders[RTSP_HEADERS][RTSP_HEADERS_CSEQ] = format("%u", ++_requestSequence);

	//3. send the mesage
	return SendMessage(_requestHeaders, _requestContent);
}

uint32_t RTSPProtocol::LastRequestSequence() {
	return _requestSequence;
}

Variant &RTSPProtocol::GetRequestHeaders() {
	return _requestHeaders;
}

string &RTSPProtocol::GetRequestContent() {
	return _requestContent;
}

void RTSPProtocol::ClearResponseMessage() {
	_responseHeaders.Reset();
	_responseContent = "";
}

void RTSPProtocol::PushResponseFirstLine(string version, uint32_t code,
		string reason) {
	_responseHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = version;
	_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE] = code;
	_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON] = reason;
}

void RTSPProtocol::PushResponseHeader(string name, string value) {
	_responseHeaders[RTSP_HEADERS][name] = value;
}

void RTSPProtocol::PushResponseContent(string outboundContent, bool append) {
	if (append)
		_responseContent += "\r\n" + outboundContent;
	else
		_responseContent = outboundContent;
}

bool RTSPProtocol::SendResponseMessage() {
	//1. Put the first line
	_outputBuffer.ReadFromString(format("%s %d %s\r\n",
			STR(_responseHeaders[RTSP_FIRST_LINE][RTSP_VERSION]),
			(uint32_t) _responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE],
			STR(_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON])));

	//2. send the mesage
	return SendMessage(_responseHeaders, _responseContent);
}

OutboundConnectivity * RTSPProtocol::GetOutboundConnectivity(
		BaseInNetStream *pInNetStream) {
	if (_pOutboundConnectivity == NULL) {
		vector<BaseOutStream *> outStreams = pInNetStream->GetOutStreams();

		BaseOutNetRTPUDPStream *pOutStream = NULL;

		FOR_VECTOR(outStreams, i) {
			if (outStreams[i]->GetType() == ST_OUT_NET_RTP) {
				pOutStream = (BaseOutNetRTPUDPStream *) outStreams[i];
				break;
			}
		}

		if (pOutStream == NULL) {
			FINEST("Create stream");
			pOutStream = new OutNetRTPUDPH264Stream(this,
					GetApplication()->GetStreamsManager(), pInNetStream->GetName());
			if (!pInNetStream->Link(pOutStream)) {
				FATAL("Unable to link streams");
				delete pOutStream;
				return false;
			}

			_pOutboundConnectivity = new OutboundConnectivity();
			if (!_pOutboundConnectivity->Initialize()) {
				FATAL("Unable to initialize outbound connectivity");
				delete pOutStream;
				delete _pOutboundConnectivity;
				_pOutboundConnectivity = NULL;
				return false;
			}
			pOutStream->SetConnectivity(_pOutboundConnectivity);
			_pOutboundConnectivity->SetOutStream(pOutStream);
		} else {
			_pOutboundConnectivity = pOutStream->GetConnectivity();
		}
	}

	return _pOutboundConnectivity;
}

void RTSPProtocol::CloseOutboundConnectivity() {
	if (_pOutboundConnectivity != NULL) {
		_pOutboundConnectivity->UnRegisterClient(GetId());
		if (!_pOutboundConnectivity->HasClients()) {
			delete _pOutboundConnectivity;
		}
		_pOutboundConnectivity = NULL;
	}
}

InboundConnectivity *RTSPProtocol::GetInboundConnectivity(Variant &videoTrack,
		Variant &audioTrack) {
	CloseInboundConnectivity();
	_pInboundConnectivity = new InboundConnectivity(this);
	if (!_pInboundConnectivity->Initialize(videoTrack, audioTrack)) {
		FATAL("Unable to initialize inbound connectivity");
		CloseInboundConnectivity();
		return false;
	}
	return _pInboundConnectivity;
}

void RTSPProtocol::CloseInboundConnectivity() {
	if (_pInboundConnectivity != NULL) {
		delete _pInboundConnectivity;
		_pInboundConnectivity = NULL;
	}
}

string RTSPProtocol::GetTransportHeaderLine(bool isAudio) {
	if (_pInboundConnectivity == NULL)
		return "";
	return format("RTP/AVP;unicast;client_port=%s",
			isAudio ? STR(_pInboundConnectivity->GetAudioClientPorts())
			: STR(_pInboundConnectivity->GetVideoClientPorts()));
}

//bool RTSPProtocol::CreateUDPInboundProtocols(Variant &parameters) {
//	//1. cleanup first
//	CloseInProtocols();
//
//	//2. create the inbound video protocol
//	_pInboundVideoRTPProtocol =
//			(InboundRTPProtocol *) ProtocolFactoryManager::CreateProtocolChain(
//			CONF_PROTOCOL_INBOUND_UDP_RTP, parameters);
//	if (_pInboundVideoRTPProtocol == NULL) {
//		FATAL("Unable to create the protocol chain");
//		CloseInProtocols();
//		return false;
//	}
//	_pInboundVideoRTPProtocol->SetApplication(GetApplication());
//
//	//3. create the inbound audio protocol
//	_pInboundAudioRTPProtocol =
//			(InboundRTPProtocol *) ProtocolFactoryManager::CreateProtocolChain(
//			CONF_PROTOCOL_INBOUND_UDP_RTP, parameters);
//	if (_pInboundAudioRTPProtocol == NULL) {
//		FATAL("Unable to create the protocol chain");
//		CloseInProtocols();
//		return false;
//	}
//	_pInboundAudioRTPProtocol->SetApplication(GetApplication());
//
//	//4. create the inbound video carrier and link it to the protocol
//	if (UDPCarrier::Create("0.0.0.0", 0, _pInboundVideoRTPProtocol) == NULL) {
//		FATAL("Unable to create UDP carrier");
//		return false;
//	}
//
//	//5. create the inbound audio carrier and link it to the protocol
//	if (UDPCarrier::Create("0.0.0.0", 0, _pInboundAudioRTPProtocol) == NULL) {
//		FATAL("Unable to create UDP carrier");
//		return false;
//	}
//
//	//6. Create the inbound stream
//	_pInStream = new InNetRTPStream(this, GetApplication()->GetStreamsManager(),
//			"__gigica",
//			unb64((string) SDP_VIDEO_CODEC_H264_SPS(parameters)),
//			unb64((string) SDP_VIDEO_CODEC_H264_PPS(parameters)));
//
//	//ASSERT("%s", STR(parameters.ToString()));
//
//	//7. make the stream known to inbound RTP protocols
//	_pInboundVideoRTPProtocol->SetStream(_pInStream);
//	_pInboundAudioRTPProtocol->SetStream(_pInStream);
//
//	//8. Make the RTSP protocol known to RTP protocols
//	_pInboundVideoRTPProtocol->SetRTSPProtocol(this);
//	_pInboundAudioRTPProtocol->SetRTSPProtocol(this);
//
//	//8. Done
//	return true;
//}

//bool RTSPProtocol::CreateRTSPInboundProtocols() {
//	CloseInProtocols();
//	NYIR;
//}

//void RTSPProtocol::CloseInProtocols() {
//	if (_pInboundVideoRTPProtocol != NULL) {
//		_pInboundVideoRTPProtocol->EnqueueForDelete();
//		_pInboundVideoRTPProtocol = NULL;
//	}
//
//	if (_pInboundAudioRTPProtocol != NULL) {
//		_pInboundAudioRTPProtocol->EnqueueForDelete();
//		_pInboundAudioRTPProtocol = NULL;
//	}
//
//	if (_pInStream != NULL) {
//		delete _pInStream;
//		_pInStream = NULL;
//	}
//}

bool RTSPProtocol::SendMessage(Variant &headers, string &content) {
	//1. Add info about us
	headers[RTSP_HEADERS][RTSP_HEADERS_SERVER] = RTSP_HEADERS_SERVER_US;
	headers[RTSP_HEADERS][RTSP_HEADERS_X_POWERED_BY] = RTSP_HEADERS_X_POWERED_BY_US;

	//2. Add the content length if required
	if (content.size() > 0) {
		headers[RTSP_HEADERS][RTSP_HEADERS_CONTENT_LENGTH] = format("%d", content.size());
	}

	//3. Write the headers

	FOR_MAP(headers[RTSP_HEADERS], string, Variant, i) {
		_outputBuffer.ReadFromString(MAP_KEY(i) + ": " + (string) MAP_VAL(i) + "\r\n");
	}
	_outputBuffer.ReadFromString("\r\n");

	//4. Write the content
	_outputBuffer.ReadFromString(content);


	//	string aaa = string((char *) GETIBPOINTER(_outputBuffer), GETAVAILABLEBYTESCOUNT(_outputBuffer));
	//	FINEST("\n`%s`", STR(aaa));

	//5. Enqueue for outbound
	return EnqueueForOutbound();
}

bool RTSPProtocol::ParseHeaders(IOBuffer& buffer) {
	if (GETAVAILABLEBYTESCOUNT(buffer) < 1) {
		FINEST("Not enough data");
		return true;
	}

	if (GETIBPOINTER(buffer)[0] == '$') {
		return ParseInterleavedHeaders(buffer);
	} else {
		return ParseNormalHeaders(buffer);
	}
}

bool RTSPProtocol::ParseInterleavedHeaders(IOBuffer &buffer) {
	_inboundHeaders["rtpData"] = true;
	NYIR;
}

bool RTSPProtocol::ParseNormalHeaders(IOBuffer &buffer) {
	_inboundHeaders.Reset();
	_inboundContent = "";
	//FINEST("buffer:\n%s", STR(buffer));
	//1. We have to have at least 4 bytes (double \r\n)
	if (GETAVAILABLEBYTESCOUNT(buffer) < 4) {
		return true;
	}

	//2. Detect the headers boundaries
	uint32_t headersSize = 0;
	bool markerFound = false;
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	for (uint32_t i = 0; i <= GETAVAILABLEBYTESCOUNT(buffer) - 4; i++) {
		if ((pBuffer[i] == 0x0d)
				&& (pBuffer[i + 1] == 0x0a)
				&& (pBuffer[i + 2] == 0x0d)
				&& (pBuffer[i + 3] == 0x0a)) {
			markerFound = true;
			headersSize = i;
			break;
		}
		if (i >= RTSP_MAX_HEADERS_SIZE) {
			FATAL("Headers section too long");
			return false;
		}
	}

	//3. Are the boundaries correct?
	//Do we have enough data to parse the headers?
	if (headersSize == 0) {
		if (markerFound)
			return false;
		else
			return true;
	}

	//4. Get the raw headers and plit it into lines
	string rawHeaders = string((char *) GETIBPOINTER(buffer), headersSize);
	vector<string> lines;
	split(rawHeaders, "\r\n", lines);
	if (lines.size() == 0) {
		FATAL("Incorrect RTSP request");
		return false;
	}

	//4. Get the fisrt line and parse it. This is either a status code
	//for a previous request made by us, or the request that we just received
	if (!ParseFirstLine(lines[0])) {
		FATAL("Unable to parse the first line");
		return false;
	}

	//5. Consider the rest of the lines as key: value pairs and store them
	//0. Reset the headers
	//	for (uint32_t i = 1; i < lines.size(); i++) {
	//		FINEST("line %d: `%s`", i, STR(lines[i]));
	//	}
	_inboundHeaders[RTSP_HEADERS].IsArray(false);
	for (uint32_t i = 1; i < lines.size(); i++) {
		string line = lines[i];
		string::size_type splitterPos = line.find(": ");

		if ((splitterPos == string::npos)
				|| (splitterPos == 0)
				|| (splitterPos == line.size() - 2)) {
			WARN("Invalid header line: %s", STR(line));
			continue;
		}
		_inboundHeaders[RTSP_HEADERS][line.substr(0, splitterPos)] = line.substr(splitterPos + 2, string::npos);
	}

	//6. default a transfer type to Content-Length: 0 if necessary
	if (!_inboundHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CONTENT_LENGTH, false)) {
		_inboundHeaders[RTSP_HEADERS][RTSP_HEADERS_CONTENT_LENGTH] = "0";
	}

	//7. read the transfer type and set this request or response flags
	string contentLengthString = _inboundHeaders[RTSP_HEADERS].GetValue(
			RTSP_HEADERS_CONTENT_LENGTH, false);
	replace(contentLengthString, " ", "");
	if (!isNumeric(contentLengthString)) {
		FATAL("Invalid RTSP headers:\n%s", STR(_inboundHeaders.ToString()));
		return false;
	}
	_contentLength = atoi(STR(contentLengthString));

	//7. Advance the state and ignore the headers part from the buffer
	_state = RTSP_STATE_PAYLOAD;
	buffer.Ignore(headersSize + 4);

	_inboundHeaders["rtpData"] = false;

	return true;
}

bool RTSPProtocol::ParseFirstLine(string &line) {
	vector<string> parts;
	split(line, " ", parts);
	if (parts.size() < 3) {
		FATAL("Incorrect first line: %s", STR(line));
		return false;
	}

	if (parts[0] == RTSP_VERSION_1_0) {
		if (!isNumeric(parts[1])) {
			FATAL("Invalid RTSP code: %s", STR(parts[1]));
			return false;
		}

		string reason = "";
		for (uint32_t i = 2; i < parts.size(); i++) {
			reason += parts[i];
			if (i != parts.size() - 1)
				reason += " ";
		}

		_inboundHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = parts[0];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE] = (uint32_t) atoi(STR(parts[1]));
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON] = reason;
		_inboundHeaders["isRequest"] = false;

		//FINEST("_inboundHeaders:\n%s", STR(_inboundHeaders.ToString()));

		return true;

	} else if ((parts[0] == RTSP_METHOD_DESCRIBE)
			|| (parts[0] == RTSP_METHOD_OPTIONS)
			|| (parts[0] == RTSP_METHOD_PAUSE)
			|| (parts[0] == RTSP_METHOD_PLAY)
			|| (parts[0] == RTSP_METHOD_SETUP)
			|| (parts[0] == RTSP_METHOD_TEARDOWN)) {
		if (parts[2] != RTSP_VERSION_1_0) {
			FATAL("RTSP version not supported: %s", STR(parts[2]));
			return false;
		}

		_inboundHeaders[RTSP_FIRST_LINE][RTSP_METHOD] = parts[0];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_URL] = parts[1];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = parts[2];
		_inboundHeaders["isRequest"] = true;

		return true;
	} else {
		FATAL("Incorrect first line: %s", STR(line));
		return false;
	}
}

bool RTSPProtocol::HandleRTSPMessage(IOBuffer &buffer) {
	//1. Get the content
	if (_contentLength > 0) {
		if (GETAVAILABLEBYTESCOUNT(buffer) < _contentLength) {
			WARN("Not enough data");
			return true;
		}
		_inboundContent = string((char *) GETIBPOINTER(buffer), _contentLength);
		buffer.Ignore(_contentLength);
	}

	//2. Call the protocol handler
	if ((bool)_inboundHeaders["isRequest"]) {
		return _pProtocolHandler->HandleRTSPRequest(this, _inboundHeaders, _inboundContent);
	} else {
		return _pProtocolHandler->HandleRTSPResponse(this, _inboundHeaders, _inboundContent);
	}
}

#endif /* HAS_PROTOCOL_RTP */
