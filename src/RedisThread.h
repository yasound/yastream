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

  RedisThread(const nuiNetworkHost& rHost, Mode mode, const nglString rID, int db);
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
  void Pong();
  void Test(const nglString& rInfo);
  void UserAuthentication(const nglString& rRadioId, const nglString& rAuthToken);
  void UserAuthentication(const nglString& rRadioId, const nglString& rUserName, const nglString& rAPIKey);
  void PlayRadio(const nglString& rRadioID);
  void StopRadio(const nglString& rRadioID);
  void RegisterListener(const nglString& rRadioID, const nglString& rSessionID, int32 UserID);
  void UnregisterListener(const nglString& rRadioID, const nglString& rSessionID, int32 UserID);

private:
  RedisClient* mpClient;
  bool mOnline;
  nuiNetworkHost mHost;
  Mode mMode;
  nuiMessageQueue mMessageQueue;
  nuiFastDelegate1<const RedisReply&> mMessageHandlerDelegate;
  nglString mID;
  int mDB;
};

