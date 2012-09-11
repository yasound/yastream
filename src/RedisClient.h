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

enum RedisReplyType
{
  eRedisError = 0,
  eRedisStatus,
  eRedisInteger,
  eRedisBulk
};


class RedisRequest
{
public:
  RedisRequest();
  virtual ~RedisRequest();

  // Send commands:
  void StartCommand(const nglString& rCommand);
  void AddArg(const nglString& rArg);
  void AddArg(int64 arg);
  void AddArg(float arg);

  // Get Replies:
  RedisReplyType GetReply() const;
  const nglString& GetError() const;
  const nglString& GetStatus() const;
  int64 GetInteger() const;
  int64 GetCount() const;
  const nglString& GetReply(size_t index) const;

  nglString GetCommandString() const;

  // Shortcuts:
  //void APPEND(const nglString& key, const nglString& value); ///< Append a value to a key
  //void AUTH(const nglString& password); ///< Authenticate to the server
  //void BGREWRITEAOF(); ///< Asynchronously rewrite the append-only file
  //void BGSAVE(); ///< Asynchronously save the dataset to disk
  //void BLPOP(const nglString& key, [const nglString& key, ...] int64 timeout); ///< Remove and get the first element in a list, or block until one is available
  //void BRPOP(const nglString& key, [const nglString& key, ...] int64 timeout); ///< Remove and get the last element in a list, or block until one is available
  //void BRPOPLPUSH(const nglString& source, const nglString& destination, int64 timeout); ///< Pop a value from a list, push it to another list and return it; or block until one is available
  //void CONFIG_GET(const nglString& parameter); ///< Get the value of a configuration parameter
  //void CONFIG_SET(const nglString& parameter, const nglString& value); ///< Set a configuration parameter to the given value
  //void CONFIG_RESETSTAT(); ///< Reset the stats returned by INFO
  //void DBSIZE(); ///< Return the number of keys in the selected database
  //void DEBUG_OBJECT(const nglString& key); ///< Get debugging information about a key
  //void DEBUG_SEGFAULT(); ///< Make the server crash
  void DECR(const nglString& key); ///< Decrement the integer value of a key by one
  //void DECRBY(const nglString& key, int64 decrement); ///< Decrement the integer value of a key by the given number
  void DEL(const nglString& key); ///< Delete a key
  void DEL(const std::vector<nglString>& keys); ///< Delete a vector of keys
  void DISCARD(); ///< Discard all commands issued after MULTI
  //void DUMP(const nglString& key); ///< Return a serialized verison of the value stored at the specified key.
  //void ECHO(message); ///< Echo the given string
  //void EVAL(const nglString& script, int64 numkeys, const nglString& key, [const nglString& key, ...] arg [arg ...]); ///< Execute a Lua script server side
  void EXEC(); ///< Execute all commands issued after MULTI
  void EXISTS(const nglString& key); ///< Determine if a key exists
  //void EXPIRE(const nglString& key, int64 seconds); ///< Set a key's time to live in seconds
  //void EXPIREAT(const nglString& key, int64 timestamp); ///< Set the expiration for a key as a UNIX timestamp
  void FLUSHALL(); ///< Remove all keys from all databases
  void FLUSHDB(); ///< Remove all keys from the current database
  void GET(const nglString& key); ///< Get the value of a key
  //void GETBIT(const nglString& key, int64 offset); ///< Returns the bit value at offset in the string value stored at key
  //void GETRANGE(const nglString& key, int64 start, int64 end); ///< Get a substring of the string stored at a key
  //void GETSET(const nglString& key, const nglString& value); ///< Set the string value of a key and return its old value
  //void HDEL(const nglString& key, const nglString& field [field ...]); ///< Delete one or more hash fields
  //void HEXISTS(const nglString& key, const nglString& field); ///< Determine if a hash field exists
  //void HGET(const nglString& key, const nglString& field); ///< Get the value of a hash field
  //void HGETALL(const nglString& key); ///< Get all the fields and values in a hash
  //void HINCRBY(const nglString& key, const nglString& field, int64 increment); ///< Increment the integer value of a hash field by the given number
  //void HINCRBYFLOAT(const nglString& key, const nglString& field, int64 increment); ///< Increment the float value of a hash field by the given amount
  //void HKEYS(const nglString& key); ///< Get all the fields in a hash
  //void HLEN(const nglString& key); ///< Get the number of fields in a hash
  //void HMGET(const nglString& key, const nglString& field [const nglString& field ...]); ///< Get the values of all the given hash fields
  //void HMSET(const nglString& key, const nglString& field, const nglString& value, [const nglString& field, const nglString& value ...]); ///< Set multiple hash fields to multiple values
  //void HSET(const nglString& key, const nglString& field, const nglString& value); ///< Set the string value of a hash field
  //void HSETNX(const nglString& key, const nglString& field, const nglString& value); ///< Set the value of a hash field, only if the field does not exist
  //void HVALS(const nglString& key); ///< Get all the values in a hash
  void INCR(const nglString& key); ///< Increment the integer value of a key by one
  void INCRBY(const nglString& key, int64 increment); ///< Increment the integer value of a key by the given amount
  void INCRBYFLOAT(const nglString& key, float increment); ///< Increment the float value of a key by the given amount
  //void INFO(); ///< Get information and statistics about the server
  void KEYS(const nglString& pattern); ///< Find all keys matching the given pattern
  //void LASTSAVE(); ///< Get the UNIX time stamp of the last successful save to disk
  //void LINDEX(const nglString& key, int64 index); ///< Get an element from a list by its index
  //void LINSERT(const nglString& key, BEFORE|AFTER, const nglString& pivot, const nglString& value); ///< Insert an element before or after another element in a list
  //void LLEN(const nglString& key); ///< Get the length of a list
  //void LPOP(const nglString& key); ///< Remove and get the first element in a list
  //void LPUSH(const nglString& key, const nglString& value [const nglString& value ...]); ///< Prepend one or multiple values to a list
  //void LPUSHX(const nglString& key, const nglString& value); ///< Prepend a value to a list, only if the list exists
  //void LRANGE(const nglString& key, int64 start, int64 stop); ///< Get a range of elements from a list
  //void LREM(const nglString& key, int64 count, const nglString& value); ///< Remove elements from a list
  //void LSET(const nglString& key, int64 index, const nglString& value); ///< Set the value of an element in a list by its index
  //void LTRIM(const nglString& key, int64 start, int64 stop); ///< Trim a list to the specified range
  //void MGET(const nglString& key, [const nglString& key, ...]); ///< Get the values of all the given keys
  //void MIGRATE(const nglString& host, uint16 port, const nglString& key, const nglString& destination-db, int64 timeout); ///< Atomically transfer a key from a Redis instance to another one.
  //void MONITOR(); ///< Listen for all requests received by the server in real time
  //void MOVE(const nglString& key, const nglString& db); ///< Move a key to another database
  //void MSET(const nglString& key, const nglString& value [const nglString& key, const nglString& value ...]); ///< Set multiple keys to multiple values
  //void MSETNX(const nglString& key, const nglString& value, [const nglString& key, const nglString& value ...]); ///< Set multiple keys to multiple values, only if none of the keys exist
  void MULTI(); ///< Mark the start of a transaction block
  //void OBJECT(const nglString& subcommand, [const nglString& arguments [const nglString& arguments ...]]); ///< Inspect the internals of Redis objects
  //void PERSIST(const nglString& key); ///< Remove the expiration from a key
  //void PEXPIRE(const nglString& key, int64 milliseconds); ///< Set a key's time to live in milliseconds
  //void PEXPIREAT(const nglString& key, int64 milliseconds_timestamp); ///< Set the expiration for a key as a UNIX timestamp specified in milliseconds
  void PING(); ///< Ping the server
  //void PSETEX(const nglString& key, int64 milliseconds, const nglString& value); ///< Set the value and expiration in milliseconds of a key
  void PSUBSCRIBE(const nglString& pattern); ///< Listen for messages published to channels matching the given patterns
  void PSUBSCRIBE(const std::vector<nglString>& rPatterns); ///< Listen for messages published to channels matching the given patterns
  //void PTTL(const nglString& key); ///< Get the time to live for a key in milliseconds
  void PUBLISH(const nglString& channel, const nglString& message); ///< Post a message to a channel
  void PUNSUBSCRIBE(const nglString& pattern); ///< Stop listening for messages posted to channels matching the given patterns
  void PUNSUBSCRIBE(const std::vector<nglString>& rPatterns); ///< Stop listening for messages posted to channels matching the given patterns
  void QUIT(); ///< Close the connection
  //void RANDOMKEY(); ///< Return a random key from the keyspace
  //void RENAME(const nglString& key, const nglString& newkey); ///< Rename a key
  //void RENAMENX(const nglString& key, const nglString& newkey); ///< Rename a key, only if the new key does not exist
  //void RESTORE(const nglString& key, int64 ttl, const nglString& serialized_value); ///< Create a key using the provided serialized value, previously obtained using DUMP.
  //void RPOP(const nglString& key); ///< Remove and get the last element in a list
  //void RPOPLPUSH(const nglString& source, const nglString& destination); ///< Remove the last element in a list, append it to another list and return it
  //void RPUSH(const nglString& key, const nglString& value, [const nglString& value ...]); ///< Append one or multiple values to a list
  //void RPUSHX(const nglString& key, const nglString& value); ///< Append a value to a list, only if the list exists
  void SADD(const nglString& key, const nglString& member); ///< Add one or more members to a set
  void SADD(const std::map<nglString, nglString>& pairs); ///< Add one or more members to a set
  //void SAVE(); ///< Synchronously save the dataset to disk
  //void SCARD(const nglString& key); ///< Get the number of members in a set
  //void SCRIPT_EXISTS(const nglString& script, [const nglString& script ...]); ///< Check existence of scripts in the script cache.
  //void SCRIPT_FLUSH(); ///< Remove all the scripts from the script cache.
  //void SCRIPT_KILL(); ///< Kill the script currently in execution.
  //void SCRIPT_LOAD(const nglString& script); ///< Load the specified Lua script into the script cache.
  //void SDIFF(const nglString& key, [const nglString& key ...]); ///< Subtract multiple sets
  //void SDIFFSTORE(const nglString& destination, const nglString& key, [const nglString& key ...]); ///< Subtract multiple sets and store the resulting set in a key
  void SELECT(int64 index); ///< Change the selected database for the current connection
  void SET(const nglString& key, const nglString& value); ///< Set the string value of a key
  //void SETBIT(const nglString& key, int64 offset, const nglString& value); ///< Sets or clears the bit at offset in the string value stored at key
  //void SETEX(const nglString& key, int64 seconds, const nglString& value); ///< Set the value and expiration of a key
  //void SETNX(const nglString& key, const nglString& value); ///< Set the value of a key, only if the key does not exist
  //void SETRANGE(const nglString& key, int64 offset, const nglString& value); ///< Overwrite part of a string at key starting at the specified offset
  //void SHUTDOWN([NOSAVE] [SAVE]); ///< Synchronously save the dataset to disk and then shut down the server
  //void SINTER(const nglString& key, [const nglString& key ...]); ///< Intersect multiple sets
  //void SINTERSTORE(const nglString& destination, const nglString& key, [const nglString& key ...]); ///< Intersect multiple sets and store the resulting set in a key
  //void SISMEMBER(const nglString& key, const nglString& member); ///< Determine if a given value is a member of a set
  //void SLAVEOF(const nglString& host, const nglString& port); ///< Make the server a slave of another instance, or promote it as master
  //void SLOWLOG(const nglString& subcommand, [const nglString& argument]); ///< Manages the Redis slow queries log
  void SMEMBERS(const nglString& key); ///< Get all the members in a set
  //void SMOVE(const nglString& source, const nglString& destination, const nglString& member); ///< Move a member from one set to another
  //void SORT(const nglString& key, [BY const nglString& pattern] [LIMIT int64 offset int64 count] [GET const nglString& pattern [GET const nglString& pattern ...]] [ASC|DESC] [ALPHA] [STORE const nglString& destination]); ///< Sort the elements in a list, set or sorted set
  //void SPOP(const nglString& key); ///< Remove and return a random member from a set
  //void SRANDMEMBER(const nglString& key); ///< Get a random member from a set
  void SREM(const nglString& key, const nglString& member); ///< Remove one or more members from a set
  void SREM(const std::map<nglString, nglString>& pairs); ///< Remove one or more members from a set
  //void STRLEN(const nglString& key); ///< Get the length of the value stored in a key
  void SUBSCRIBE(const nglString& channel); ///< Listen for messages published to the given channels
  void SUBSCRIBE(const std::vector<nglString>& rChannels); ///< Listen for messages published to the given channels
  //void SUNION(const nglString& key, [const nglString& key ...]); ///< Add multiple sets
  //void SUNIONSTORE(const nglString& destination, const nglString& key, [const nglString& key ...]); ///< Add multiple sets and store the resulting set in a key
  //void SYNC(); ///< Internal command used for replication
  //void TIME(); ///< Return the current server time
  //void TTL(const nglString& key); ///< Get the time to live for a key
  //void TYPE(const nglString& key); ///< Determine the type stored at key
  void UNSUBSCRIBE(const nglString& channel); ///< Stop listening for messages posted to the given channels
  void UNSUBSCRIBE(const std::vector<nglString>& rChannels); ///< Stop listening for messages posted to the given channels
  //void UNWATCH(); ///< Forget about all watched keys
  //void WATCH(const nglString& key, [const nglString& key ...]); ///< Watch the given keys to determine execution of the MULTI/EXEC block
  //void ZADD(const nglString& key, int64 score, const nglString& member, [int64 score], [const nglString& member]); ///< Add one or more members to a sorted set, or update its score if it already exists
  //void ZCARD(const nglString& key); ///< Get the number of members in a sorted set
  //void ZCOUNT(const nglString& key, int64 min, int64 max); ///< Count the members in a sorted set with scores within the given values
  //void ZINCRBY key increment member); ///< Increment the score of a member in a sorted set
  //void ZINTERSTORE(const nglString& destination, int64 numkeys, const nglString& key, [const nglString& key ...] [WEIGHTS, int64 weight [int64 weight ...]] [AGGREGATE SUM|MIN|MAX]); ///< Intersect multiple sorted sets and store the resulting sorted set in a new key
  //void ZRANGE(const nglString& key, int64 start, int64 stop, [WITHSCORES]); ///< Return a range of members in a sorted set, by index
  //void ZRANGEBYSCORE(const nglString& key, int64 min, int64 max ,[WITHSCORES] [LIMIT int64 offset, int64 count]); ///< Return a range of members in a sorted set, by score
  //void ZRANK(const nglString& key, const nglString& member); ///< Determine the index of a member in a sorted set
  //void ZREM(const nglString& key, const nglString& member, [const nglString& member ...]); ///< Remove one or more members from a sorted set
  //void ZREMRANGEBYRANK(const nglString& key, int64 start, int64 stop); ///< Remove all members in a sorted set within the given indexes
  //void ZREMRANGEBYSCORE(const nglString& key, int64 min, int64 max); ///< Remove all members in a sorted set within the given scores
  //void ZREVRANGE(const nglString& key, int64 start, int64 stop [WITHSCORES]); ///< Return a range of members in a sorted set, by index, with scores ordered from high to low
  //void ZREVRANGEBYSCORE(const nglString& key, int64 max, int64 min, [WITHSCORES] [LIMIT, int64 offset, int64 count]); ///< Return a range of members in a sorted set, by score, with scores ordered from high to low
  //void ZREVRANK(const nglString& key, const nglString& member); ///< Determine the index of a member in a sorted set, with scores ordered from high to low
  //void ZSCORE(const nglString& key, const nglString& member); ///< Get the score associated with the given member in a sorted set
  //void ZUNIONSTORE(const nglString& destination, int64 numkeys, const nglString& key, [const nglString& key ...] [WEIGHTS int64 weight [int64 weight ...]] [AGGREGATE SUM|MIN|MAX]); ///< Add multiple sorted sets and store the resulting sorted set in a new key


private:
  std::vector<nglString> mRequest;
  std::vector<nglString> mReply;
  RedisReplyType mReplyType;
  nglString mError;
  nglString mStatus;
  int64 mInteger;

  friend class RedisClient;
};

class RedisClient
{
public:

  RedisClient();
  virtual ~RedisClient();

  // Server connection:
  bool Connect(const nuiNetworkHost& rHost);
  void Disconnect();
  bool IsConnected();

  // Send commands:
  RedisReplyType SendCommand(RedisRequest& rRequest);
  RedisReplyType PrintSendCommand(RedisRequest& rRequest);

private:
  nuiTCPClient* mpClient;
  nglCriticalSection mLock;
};

