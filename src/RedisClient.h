//
//  RedisClient.h
//  yastream
//
//  Created by Sebastien Metrot on 3/12/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#pragma once

#include "nuiNetworkHost.h"
#include "nuiTCPClient.h"

class RedisClient
{
public:
  enum ReplyType
  {
    eRedisError = 0,
    eRedisStatus,
    eRedisInteger,
    eRedisBulk
  };

  RedisClient();
  virtual ~RedisClient();

  // Server connection:
  bool Connect(const nuiNetworkHost& rHost);
  void Disconnect();
  bool IsConnected();
  
  // Send commands:
  void StartCommand(const nglString& rCommand);
  void AddArg(const nglString& rArg);
  ReplyType SendCommand();
  ReplyType PrintSendCommand();
  
  // Get Replies:
  
  ReplyType GetReply() const;
  const nglString& GetError() const;
  const nglString& GetStatus() const;
  int64 GetInteger() const;
  int64 GetCount() const;
  const nglString& GetReply(size_t index) const;
  
private:
  std::vector<nglString> mRequest;
  std::vector<nglString> mReply;
  ReplyType mReplyType;
  nglString mError;
  nglString mStatus;
  int64 mInteger;
  nuiTCPClient* mpClient;
};

