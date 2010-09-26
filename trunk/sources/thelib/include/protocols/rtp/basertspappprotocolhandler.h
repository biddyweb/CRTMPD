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
#ifndef _BASERTSPAPPPROTOCOLHANDLER_H
#define	_BASERTSPAPPPROTOCOLHANDLER_H

#include "application/baseappprotocolhandler.h"
#include "streaming/streamcapabilities.h"

class RTSPProtocol;
class BaseInNetStream;
class OutboundConnectivity;

class DLLEXP BaseRTSPAppProtocolHandler
: public BaseAppProtocolHandler {
public:
	BaseRTSPAppProtocolHandler(Variant &configuration);
	virtual ~BaseRTSPAppProtocolHandler();

	virtual void RegisterProtocol(BaseProtocol *pProtocol);
	virtual void UnRegisterProtocol(BaseProtocol *pProtocol);

	virtual bool PullExternalStream(URI uri, Variant streamConfig);
	static bool SignalProtocolCreated(BaseProtocol *pProtocol,
			Variant &parameters);

	virtual bool HandleRTSPRequest(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &content);
	virtual bool HandleRTSPResponse(RTSPProtocol *pFrom, Variant &responseHeaders,
			string &content);
protected:
	//handle requests routines
	virtual bool HandleRTSPRequestOptions(RTSPProtocol *pFrom,
			Variant &requestHeaders);
	virtual bool HandleRTSPRequestDescribe(RTSPProtocol *pFrom,
			Variant &requestHeaders);
	virtual bool HandleRTSPRequestSetup(RTSPProtocol *pFrom,
			Variant &requestHeaders);
	virtual bool HandleRTSPRequestPlay(RTSPProtocol *pFrom,
			Variant &requestHeaders);
	virtual bool HandleRTSPRequestTearDown(RTSPProtocol *pFrom,
			Variant &requestHeaders);

	//handle response routines
	virtual bool HandleRTSPResponse(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse200(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse404(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse200Options(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse200Describe(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse200Setup(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse200Play(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);
	virtual bool HandleRTSPResponse404Play(RTSPProtocol *pFrom, Variant &requestHeaders,
			string &requestContent, Variant &responseHeaders,
			string &responseContent);

	//operations
	virtual bool Play(RTSPProtocol *pFrom);
private:
	OutboundConnectivity *GetOutboundConnectivity(RTSPProtocol *pFrom);
	BaseInNetStream *GetInboundStream(string streamName);
	StreamCapabilities *GetInboundStreamCapabilities(string streamName);
	string GetAudioTrack(RTSPProtocol *pFrom,
			StreamCapabilities *pCapabilities);
	string GetVideoTrack(RTSPProtocol *pFrom,
			StreamCapabilities *pCapabilities);
};


#endif	/* _BASERTSPAPPPROTOCOLHANDLER_H */
#endif /* HAS_PROTOCOL_RTP */
