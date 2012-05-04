//
//  RedisClient.cpp
//  yastream
//
//  Created by Sebastien Metrot on 3/12/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#include "nui.h"
#include "RedisClient.h"

//class RedisRequest:
RedisRequest::RedisRequest()
{
}

RedisRequest::~RedisRequest()
{
}


RedisReplyType RedisRequest::GetReply() const
{
  return mReplyType;
}

const nglString& RedisRequest::GetError() const
{
  return mError;
}

const nglString& RedisRequest::GetStatus() const
{
  return mStatus;
}

int64 RedisRequest::GetInteger() const
{
  return mInteger;
}

int64 RedisRequest::GetCount() const
{
  return mReply.size();
}

const nglString& RedisRequest::GetReply(size_t index) const
{
  if (mReply.size() <= index)
    return nglString::Null;
  return mReply[index];
}

void RedisRequest::StartCommand(const nglString& rCommand)
{
  mRequest.clear();
  mRequest.push_back(rCommand);
}

void RedisRequest::AddArg(const nglString& rArg)
{
  mRequest.push_back(rArg);
}

void RedisRequest::AddArg(int64 arg)
{
  nglString t;
  t.SetCInt(arg);
  AddArg(t);
}

void RedisRequest::AddArg(float arg)
{
  nglString t;
  t.SetCFloat(arg);
  AddArg(t);
}

nglString RedisRequest::GetCommandString() const
{
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

  return str;
}



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
  return mpClient && mpClient->IsWriteConnected();
}

// Send commands:
RedisReplyType RedisClient::SendCommand(RedisRequest& rRequest)
{
  nglCriticalSectionGuard guard(mLock);
  rRequest.mReply.clear();
  rRequest.mError.Nullify();
  rRequest.mStatus.Nullify();
  rRequest.mInteger = 0;

  //printf("Redis Command:\n%s\n", str.GetChars());
  mpClient->Send(rRequest.GetCommandString());

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
  //printf("redis line\n");
        eolfound = false;
        // found a line:
        switch (line[0])
        {
        case '+':
          {
            rRequest.mStatus = line.Extract(1, line.GetLength() - 1);
  //printf("status\n");
            return rRequest.mReplyType = eRedisStatus;
          }
          break;
        case '-':
          {
            rRequest.mError = line.Extract(1, line.GetLength() - 1);
  //printf("error\n");
            NGL_LOG("radio", NGL_LOG_ERROR, "Redis error '%s'", rRequest.mError.GetChars());
            return rRequest.mReplyType = eRedisError;
          }
          break;
        case ':':
          {
  //printf("int\n");
            rRequest.mInteger = line.Extract(1, line.GetLength() - 1).GetCInt64();
            return rRequest.mReplyType = eRedisInteger;
          }
          break;
        case '$':
          {
  //printf("bulk\n");
            int64 s = line.Extract(1, line.GetLength() - 1).GetCInt64();
            if (s < 0)
            {
              rRequest.mReply.push_back(nglString::Null);
              return rRequest.mReplyType = eRedisBulk;
            }

            std::vector<uint8> d;
            d.resize(s);
            int64 done = mpClient->Receive(d);
            NGL_ASSERT(done == s);

            nglString res((const nglChar*)&d[0], d.size(), eEncodingInternal);
            if (res == "$-1")
              res.Nullify();
            rRequest.mReply.push_back(res);
            d.resize(2);
            res = mpClient->Receive(d);
            NGL_ASSERT(d[0] == 13 && d[1] == 10);

            replycount--;
            if (!replycount)
              return rRequest.mReplyType = eRedisBulk;
          }
          break;
        case '*':
          {
            int64 s = line.Extract(1, line.GetLength() - 3).GetCInt64();
            replycount = s;
  //printf("realbulk %lld\n", s);
            if (replycount == 0)
              return rRequest.mReplyType = eRedisBulk;
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

  return rRequest.mReplyType;
}

RedisReplyType RedisClient::PrintSendCommand(RedisRequest& rRequest)
{
  printf("Redis command: %s\n", rRequest.mRequest[0].GetChars());
  RedisReplyType reply = SendCommand(rRequest);
  // Optionally print output:
  switch (reply)
  {
    case eRedisError:
      printf("error: %s\n", rRequest.GetError().GetChars());
      break;
    case eRedisStatus:
      printf("status: %s\n", rRequest.GetStatus().GetChars());
      break;
    case eRedisInteger:
      printf("integer: %ld\n", rRequest.GetInteger());
      break;
    case eRedisBulk:
      for (int i = 0; i < rRequest.GetCount(); i++)
        printf("[%d]: %s\n", i, rRequest.GetReply(i).GetChars());
      break;
  }

  return reply;
}


// Shortcuts:
void RedisRequest::DECR(const nglString& key) ///< Decrement the integer value of a key by one
{
  StartCommand("DECR");
  AddArg(key);
}

void RedisRequest::DEL(const nglString& key)
{
  StartCommand("DEL");
  AddArg(key);
}

void RedisRequest::DEL(const std::vector<nglString>& keys)
{
  StartCommand("DEL");
  for (size_t i = 0; i < keys.size(); i++)
    AddArg(keys[i]);
}

void RedisRequest::DISCARD() ///< Discard all commands issued after MULTI
{
  StartCommand("DISCARD");
}

void RedisRequest::EXEC() ///< Execute all commands issued after MULTI
{
  StartCommand("EXEC");
}

void RedisRequest::EXISTS(const nglString& key) ///< Determine if a key exists
{
  StartCommand("EXISTS");
  AddArg(key);
}

void RedisRequest::FLUSHALL() ///< Remove all keys from all databases
{
  StartCommand("FLUSHALL");
}

void RedisRequest::FLUSHDB() ///< Remove all keys from the current database
{
  StartCommand("FLUSHDB");
}

void RedisRequest::GET(const nglString& key) ///< Get the value of a key
{
  StartCommand("GET");
  AddArg(key);
}


void RedisRequest::INCR(const nglString& key) ///< Increment the integer value of a key by one
{
  StartCommand("INCR");
  AddArg(key);
}

void RedisRequest::INCRBY(const nglString& key, int64 increment) ///< Increment the integer value of a key by the given amount
{
  StartCommand("INCRBY");
  AddArg(key);
  AddArg(increment);
}

void RedisRequest::INCRBYFLOAT(const nglString& key, float increment) ///< Increment the float value of a key by the given amount
{
  StartCommand("INCRBYFLOAT");
  AddArg(key);
  AddArg(increment);
}


void RedisRequest::KEYS(const nglString& pattern) ///< Find all keys matching the given pattern
{
  StartCommand("KEYS");
  AddArg(pattern);
}

void RedisRequest::MULTI() ///< Mark the start of a transaction block
{
  StartCommand("MULTI");
}

void RedisRequest::PING() ///< Ping the server
{
  StartCommand("PING");
}

void RedisRequest::QUIT() ///< Close the connection
{
  StartCommand("QUIT");
}

void RedisRequest::SADD(const nglString& key, const nglString& member) ///< Add one or more members to a set
{
  StartCommand("SADD");
  AddArg(key);
  AddArg(member);
}

void RedisRequest::SADD(const std::map<nglString, nglString>& pairs) ///< Add one or more members to a set
{
  StartCommand("SADD");

  std::map<nglString, nglString>::const_iterator it = pairs.begin();
  std::map<nglString, nglString>::const_iterator end = pairs.end();

  while (it != end)
  {
    AddArg(it->first);
    AddArg(it->second);
    ++it;
  }
}

void RedisRequest::SELECT(int64 index) ///< Change the selected database for the current connection
{
  StartCommand("SELECT");
  AddArg(index);
}

void RedisRequest::SET(const nglString& key, const nglString& value)
{
  StartCommand("SET");
  AddArg(key);
  AddArg(value);
}

void RedisRequest::SMEMBERS(const nglString& key) ///< Get all the members in a set
{
  StartCommand("SMEMBERS");
  AddArg(key);
}

void RedisRequest::SREM(const nglString& key, const nglString& member)
{
  StartCommand("SREM");
  AddArg(key);
  AddArg(member);
}

void RedisRequest::SREM(const std::map<nglString, nglString>& pairs)
{
  StartCommand("SREM");

  std::map<nglString, nglString>::const_iterator it = pairs.begin();
  std::map<nglString, nglString>::const_iterator end = pairs.end();

  while (it != end)
  {
    AddArg(it->first);
    AddArg(it->second);
    ++it;
  }
}

