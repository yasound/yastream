//
//  RedisThread.h
//  yastream
//
//  Created by Sébastien Métrot on 10/20/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#pragma once

#include "nui.h"
#include "RedisClient.h"
#include "nuiJson.h"

class RedisThread : public nglThread
{
public:
  enum Mode
  {
    MessagePump,
    Broadcaster
  };

  RedisThread(const nuiNetworkHost& rHost, Mode mode);
  virtual ~RedisThread();
  void Stop();
  void OnStart();
  void Broadcast();
  void PumpMessages();
  void HandleMessage(const RedisReply& rReply);
  bool Post(const RedisRequest& request);
  bool Post(nuiNotification* notif);
  void Post(const nuiJson::Value& val);
  void SetMessageHandler(nuiFastDelegate1<const RedisReply&> rDelegate);

  // API
  void RegisterStreamer(const nglString& rStreamerID);
  void UnregisterStreamer(const nglString& rStreamerID);
  void Pong(const nglString& rStreamerID);
  void Test(const nglString& rStreamerID, const nglString& rInfo);
  void UserAuthentication(const nglString& rStreamerID, const nglString& rAuthToken);
  void UserAuthentication(const nglString& rStreamerID, const nglString& rUserName, const nglString& rAPIKey);
  void PlayRadio(const nglString& rStreamerID, const nglString& rRadioID);
  void StopRadio(const nglString& rStreamerID, const nglString& rRadioID);
  void RegisterListener(const nglString& rStreamerID, const nglString& rRadioID, const nglString& rSessionID, const nglString& rUserID);
  void UnregisterListener(const nglString& rStreamerID, const nglString& rRadioID, const nglString& rSessionID, const nglString& rUserID);

private:
  RedisClient* mpClient;
  bool mOnline;
  nuiNetworkHost mHost;
  Mode mMode;
  nuiMessageQueue mMessageQueue;
  nuiFastDelegate1<const RedisReply&> mMessageHandlerDelegate;
};

