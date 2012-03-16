//
//  RedisClient.cpp
//  yastream
//
//  Created by Sebastien Metrot on 3/12/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#include "nui.h"
#include "RedisClient.h"

//class RedisClient
RedisClient::RedisClient()
{
  mpClient = NULL;
}

RedisClient::~RedisClient()
{
  Disconnect();
}

// Server connection:
bool RedisClient::Connect(const nuiNetworkHost& rHost)
{
  mpClient = new nuiTCPClient();
  return mpClient->Connect(rHost);
}

void RedisClient::Disconnect()
{
  delete mpClient;
  mpClient = NULL;
}

bool RedisClient::IsConnected()
{
  return mpClient && mpClient->IsConnected();
}

// Send commands:
void RedisClient::StartCommand(const nglString& rCommand)
{
  mRequest.clear();
  mRequest.push_back(rCommand);
}

void RedisClient::AddArg(const nglString& rArg)
{
  mRequest.push_back(rArg);
}

RedisClient::ReplyType RedisClient::SendCommand()
{
  mReply.clear();
  mError.Nullify();
  mStatus.Nullify();
  mInteger = 0;
  
  nglString eol = "\r\n";
  nglString str;
  str.CFormat("*%d\r\n", mRequest.size());
  for (int i = 0; i < mRequest.size(); i++)
  {
    const nglString& arg(mRequest[i]);
    nglString args;
    args.CFormat("$%d\r\n", arg.GetLength());
    args += arg;
    args += eol;
    str += args;
  }

  mpClient->Send(str);
  
  ReplyType type;
  std::vector<uint8> data;
  nglChar cur = 0;
  data.resize(1024);
  nglString line;

  int replycount = 1;
  while (mpClient->Receive(data))
  {
    size_t index = 0;
    while (index < data.size())
    {
      cur = data[index];
        index++;
        
      if (cur == 10)
      {
        // skip...
      }
      else if (cur == 13)
      {
        // found a line:
        switch (line[0])
        {
        case '+':
          {
            mStatus = line.Extract(1, line.GetLength() - 3);
            return mReplyType = eRedisStatus;
          }
          break;
        case '-':
          {
            mError = line.Extract(1, line.GetLength() - 3);
            return mReplyType = eRedisError;
          }
          break;
        case ':':
          {
            mInteger = line.Extract(1, line.GetLength() - 3).GetCInt64();
            return mReplyType = eRedisInteger;
          }
          break;
        case '$':
          {
            int64 s = line.Extract(1, line.GetLength() - 3).GetCInt64();
            std::vector<uint8> d;
            d.resize(s);
            int64 done = mpClient->Receive(d);
            NGL_ASSERT(done == s);

            nglString res((const nglChar*)&d[0], d.size(), eEncodingInternal);
            mReply.push_back(res);
            d.resize(2);
            res = mpClient->Receive(d);
            NGL_ASSERT(d[0] == 10 && d[1] == 13);

            replycount--;
            if (!replycount)
              return mReplyType = eRedisBulk;
          }
          break;
        case '*':
          {
            int64 s = line.Extract(1, line.GetLength() - 3).GetCInt64();
            replycount = s;
          }
          break;
        }
        line.Wipe();
      }
      else
      {
        line.Append(cur);
      }
    }
  }
}

RedisClient::ReplyType RedisClient::GetReply() const
{
  return mReplyType;
}

const nglString& RedisClient::GetError() const
{
  return mError;
}

const nglString& RedisClient::GetStatus() const
{
  return mStatus;
}

int64 RedisClient::GetCount() const
{
  return mReply.size();
}

const nglString& RedisClient::GetReply(size_t index) const
{
  return mReply[index];
}

