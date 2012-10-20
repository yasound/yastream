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
#include "nuiJSON.h"

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

  // API
  void RegisterStreamer();
  void SetMessageHandler(nuiFastDelegate1<const RedisReply&> rDelegate);

private:
  RedisClient* mpClient;
  bool mOnline;
  nuiNetworkHost mHost;
  Mode mMode;
  nuiMessageQueue mMessageQueue;
  nuiFastDelegate1<const RedisReply&> mMessageHandlerDelegate;
};

