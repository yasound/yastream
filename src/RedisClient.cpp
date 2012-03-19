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
  delete mpClient;
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

printf("Redis Command:\n%s\n", str.GetChars());
  mpClient->Send(str);
  
  std::vector<uint8> data;
  nglChar cur = 0;
  data.resize(1);
  nglString line;

  int replycount = 1;
  bool eolfound = false;
  while (mpClient->Receive(data))
  {
    size_t index = 0;
    while (index < data.size())
    {
      cur = data[index];
      index++;
        
      if (cur == 13)
      {
        // skip...
        eolfound = true;
      }
      else if (eolfound && cur == 10)
      {
  printf("redis line\n");
        eolfound = false;
        // found a line:
        switch (line[0])
        {
        case '+':
          {
            mStatus = line.Extract(1, line.GetLength() - 1);
  printf("status\n");
            return mReplyType = eRedisStatus;
          }
          break;
        case '-':
          {
            mError = line.Extract(1, line.GetLength() - 1);
  printf("error\n");
            return mReplyType = eRedisError;
          }
          break;
        case ':':
          {
  printf("int\n");
            mInteger = line.Extract(1, line.GetLength() - 1).GetCInt64();
            return mReplyType = eRedisInteger;
          }
          break;
        case '$':
          {
  printf("bulk\n");
            int64 s = line.Extract(1, line.GetLength() - 1).GetCInt64();
            if (s < 0)
            {
              mReply.push_back(nglString::Null);
              return mReplyType = eRedisBulk;
            }
            
            std::vector<uint8> d;
            d.resize(s);
            int64 done = mpClient->Receive(d);
            NGL_ASSERT(done == s);

            nglString res((const nglChar*)&d[0], d.size(), eEncodingInternal);
            if (res == "$-1")
              res.Nullify();
            mReply.push_back(res);
            d.resize(2);
            res = mpClient->Receive(d);
            NGL_ASSERT(d[0] == 13 && d[1] == 10);

            replycount--;
            if (!replycount)
              return mReplyType = eRedisBulk;
          }
          break;
        case '*':
          {
            int64 s = line.Extract(1, line.GetLength() - 3).GetCInt64();
            replycount = s;
  printf("realbulk %lld\n", s);
            if (replycount == 0)
              return mReplyType = eRedisBulk;
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

RedisClient::ReplyType RedisClient::PrintSendCommand()
{
  printf("Redis command: %s\n", mRequest[0].GetChars());
  ReplyType reply = SendCommand();
  // Optionnaly print output:
  switch (reply)
  {
    case RedisClient::eRedisError:
      printf("error: %s\n", GetError().GetChars());
      break;
    case RedisClient::eRedisStatus:
      printf("status: %s\n", GetStatus().GetChars());
      break;
    case RedisClient::eRedisInteger:
      printf("integer: %ld\n", GetInteger());
      break;
    case RedisClient::eRedisBulk:
      for (int i = 0; i < GetCount(); i++)
        printf("[%d]: %s\n", i, GetReply(i).GetChars());
      break;
  }

  return reply;
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

int64 RedisClient::GetInteger() const
{
  return mInteger;
}

int64 RedisClient::GetCount() const
{
  return mReply.size();
}

const nglString& RedisClient::GetReply(size_t index) const
{
  return mReply[index];
}

