//
//  RedisThread.cpp
//  yastream
//
//  Created by Sébastien Métrot on 10/20/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#include "nui.h"
#include "RedisThread.h"

//class RedisThread : public nglThread
RedisThread::RedisThread(const nuiNetworkHost& rHost, Mode mode, const nglString rID)
: mHost(rHost), mMode(mode), mID(rID)
{
  mpClient = new RedisClient();
  mOnline = true;
}

RedisThread::~RedisThread()
{
  delete mpClient;
}

void RedisThread::Stop()
{
  mOnline = false;
  mpClient->Disconnect();
}

void RedisThread::OnStart()
{
  switch (mMode)
  {
    case MessagePump:
      PumpMessages();
      break;
    case Broadcaster:
      Broadcast();
      break;
  }
}

void RedisThread::Broadcast()
{
  while (mOnline)
  {
    nuiNotification* pNotif = mMessageQueue.Get(1);
    if (pNotif)
    {
      if (pNotif->GetName() == "RedisRequest")
      {
        nuiToken<RedisRequest>* pToken = dynamic_cast<nuiToken<RedisRequest>*>(pNotif->GetToken());
        if (pToken)
        {
          RedisRequest Request = pToken->Token;

          int t = 1;
          // Make sure we're connected to redis:
          while  (mOnline && !mpClient->IsConnected())
          {
            NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connection\n");
            if (mpClient->Connect(mHost) && mpClient->IsConnected())
            {
              NGL_LOG("radio", NGL_LOG_INFO, "Redis message broadcaster connected\n");
            }
            else
            {
              nglThread::MsSleep(t);
              t = MIN(10000, t * 2);
            }
          }

          // Send request:
          mpClient->SendCommand(Request);
        }
      }
      pNotif->Release();
    }
  }
}

void RedisThread::PumpMessages()
{
  while (mOnline)
  {
    int t = 1;
    while  (mOnline && !mpClient->IsConnected())
    {
      NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connection\n");
      if (mpClient->Connect(mHost) && mpClient->IsConnected())
      {
        NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connected\n");
        RedisRequest request;
        nglString channel("yascheduler.");
        channel += mID;
        request.SUBSCRIBE(channel);
        RedisReplyType res = mpClient->SendCommand(request);
        if (res == eRedisError)
        {
          NGL_LOG("radio", NGL_LOG_INFO, "Redis message unable to subscribe to yascheduler channel\n");
          mpClient->Disconnect();
        }
        else
        {
          NGL_LOG("radio", NGL_LOG_INFO, "Redis message subscribed to yascheduler channel\n");
        }
      }
      else
      {
        nglThread::MsSleep(t);
        t = MIN(10000, t * 2);
      }
    }

    if (mOnline && mpClient->IsConnected())
    {
      RedisReply reply;
      RedisReplyType res = mpClient->GetReply(reply);

      if (res == eRedisBulk)
      {
        //          nglString str;
        //          for (int i = 0; i < reply.GetCount(); i++)
        //            str.Add("'").Add(reply.GetReply(i)).Add("'  ");
        //
        //          NGL_LOG("radio", NGL_LOG_INFO, "Redis message: %s\n", str.GetChars());
        HandleMessage(reply);
      }
      else
      {
        NGL_LOG("radio", NGL_LOG_INFO, "Redis reply\n");
      }
    }
  }
  NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump offline\n");
}


void RedisThread::HandleMessage(const RedisReply& rReply)
{
  if (mMessageHandlerDelegate)
    mMessageHandlerDelegate(rReply);
}

bool RedisThread::Post(const RedisRequest& request)
{
  nuiToken<RedisRequest>* pToken = new nuiToken<RedisRequest>(request);
  nuiNotification* notif = new nuiNotification("RedisRequest");
  notif->Acquire();
  notif->SetToken(pToken);
  return mMessageQueue.Post(notif);
}

bool RedisThread::Post(nuiNotification* notif)
{
  return mMessageQueue.Post(notif);
}

void RedisThread::Post(const nuiJson::Value& val)
{
  RedisRequest request;
  nuiJson::FastWriter writer;
  nglString json = writer.write(val);
  request.PUBLISH("yascheduler", json);

  Post(request);
}

// API
void RedisThread::RegisterStreamer(const nglString& rStreamerID)
{
  nuiJson::Value val;
  val["type"] = "register_streamer";
  val["streamer"] = mID.GetChars();
  Post(val);
}

void RedisThread::UnregisterStreamer(const nglString& rStreamerID)
{
  nuiJson::Value val;
  val["type"] = "unregister_streamer";
  val["streamer"] = mID.GetChars();
  Post(val);
}

void RedisThread::Pong()
{
  nuiJson::Value val;
  val["type"] = "pong";
  val["streamer"] = mID.GetChars();
  Post(val);
}

void RedisThread::Test(const nglString& rInfo)
{
  nuiJson::Value val;
  val["type"] = "test";
  val["streamer"] = mID.GetChars();
  val["info"] = rInfo.GetChars();
  Post(val);
}

void RedisThread::UserAuthentication(const nglString& rAuthToken)
{
  nuiJson::Value val;
  val["type"] = "user_authentication";
  val["streamer"] = mID.GetChars();
  val["auth_token"] = rAuthToken.GetChars();
  Post(val);
}

void RedisThread::UserAuthentication(const nglString& rUserName, const nglString& rAPIKey)
{
  nuiJson::Value val;
  val["type"] = "user_authentication";
  val["streamer"] = mID.GetChars();
  val["username"] = rUserName.GetChars();
  val["api_key"] = rAPIKey.GetChars();
  Post(val);
}

void RedisThread::PlayRadio(const nglString& rRadioID)
{
  nuiJson::Value val;
  val["type"] = "play_radio";
  val["streamer"] = mID.GetChars();
  val["radio_uuid"] = rRadioID.GetChars();
  Post(val);
}

void RedisThread::StopRadio(const nglString& rRadioID)
{
  nuiJson::Value val;
  val["type"] = "stop_radio";
  val["streamer"] = mID.GetChars();
  val["radio_uuid"] = rRadioID.GetChars();
  Post(val);
}

void RedisThread::RegisterListener(const nglString& rRadioID, const nglString& rSessionID, const nglString& rUserID)
{
  nuiJson::Value val;
  val["type"] = "stop_radio";
  val["streamer"] = mID.GetChars();
  val["radio_uuid"] = rRadioID.GetChars();
  val["session_id"] = rSessionID.GetChars();
  val["user_id"] = rUserID.GetChars();
  Post(val);
}

void RedisThread::UnregisterListener(const nglString& rRadioID, const nglString& rSessionID, const nglString& rUserID)
{
  nuiJson::Value val;
  val["type"] = "stop_radio";
  val["streamer"] = mID.GetChars();
  val["radio_uuid"] = rRadioID.GetChars();
  val["session_id"] = rSessionID.GetChars();
  val["user_id"] = rUserID.GetChars();
  Post(val);
}


void RedisThread::SetMessageHandler(nuiFastDelegate1<const RedisReply&> rDelegate)
{
  mMessageHandlerDelegate = rDelegate;
}

