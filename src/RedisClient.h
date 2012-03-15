//
//  RedisClient.h
//  yastream
//
//  Created by Sebastien Metrot on 3/12/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#ifndef yastream_RedisClient_h
#define yastream_RedisClient_h

class RedisClient
{
public:
  RedisClient();
  virtual ~RedisClient();
  
  bool Connect(const nuiNetworkHost& rHost);
  void Disconnect();
  
  bool SendCommand(const nglString& rCommand
  
};

#endif
