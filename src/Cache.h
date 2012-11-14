//
//  Cache.h
//  yastream
//
//  Created by Sébastien Métrot on 10/28/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#pragma once

#include <list>
#include <map>
#include "nui.h"

nglString nglBytes(int64 b);

template <class KeyType, class ItemType>
class CacheItem
{
public:
  typedef std::list<KeyType> KeyList;

  CacheItem(const typename KeyList::iterator& rIterator, const ItemType& rItem)
  : mIterator(rIterator), mItem(rItem), mRefCount(1)
  {
  }

  CacheItem()
  : mRefCount(-1)
  {
  }

  CacheItem(const CacheItem<KeyType, ItemType>& rCacheItem)
  : mIterator(rCacheItem.mIterator), mItem(rCacheItem.mItem), mRefCount(rCacheItem.mRefCount)
  {
  }

  ~CacheItem()
  {
  }

  const ItemType& GetItem() const
  {
    return mItem;
  }

  const typename KeyList::iterator& GetIterator() const
  {
    return mIterator;
  }

  void SetWeight(int64 Weight)
  {
    mWeight = Weight;
  }


  int64 GetWeight() const
  {
    return mWeight;
  }

  int64 Acquire()
  {
    mRefCount++;
    return mRefCount;
  }

  int64 Release()
  {
    mRefCount--;
    return mRefCount;
  }

  int64 GetRefCount() const
  {
    return mRefCount;
  }

private:
  ItemType mItem;
  int64 mWeight;
  typename KeyList::iterator mIterator;
  int64 mRefCount;
};

template <class KeyType, class ItemType>
class Cache : public nuiNonCopyable
{
public:
  Cache()
  : mWeight(0), mMaxWeight(0)
  {
  }

  virtual ~Cache()
  {
  }

  typedef CacheItem<KeyType, ItemType> Item;
  typedef std::list<KeyType> KeyList;
  typedef std::map<KeyType, Item> ItemMap;

  bool GetItem(const KeyType& rKey, ItemType& rItem)
  {
    NGL_ASSERT(mCreateItem);
    nglCriticalSectionGuard g(mCS);
    typename ItemMap::iterator it = mItems.find(rKey);
    if (it == mItems.end())
    {
      //mCS.Unlock(); // Beware!!! We temporarly unlock the CS because calling mCreateItem may be time consuming!
      int64 Weight = 0;
      bool res = mCreateItem(rKey, rItem, Weight);
      //mCS.Lock(); // Beware!!! We temporarly relock the CS because calling mCreateItem may have been time consuming!

      AddItem(rKey, rItem);
      NGL_LOG("radio", NGL_LOG_INFO, "Cache::GetItem '%s'", rKey.GetChars());

      Purge();
      return true;
    }

    mKeys.splice(mKeys.begin(), mKeys, it->second.GetIterator());
    rItem = it->second.GetItem();
    it->second.Acquire();
    return true;
  }

  bool ReleaseItem(const KeyType& rKey, const ItemType& rItem)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Cache::ReleaseItem '%s'", rKey.GetChars());
    nglCriticalSectionGuard g(mCS);
    typename ItemMap::iterator it = mItems.find(rKey);
    NGL_ASSERT(it != mItems.end());
    it->second.Release();
  }

  typedef nuiFastDelegate3<const KeyType&, ItemType&, int64&, bool> CreateItemDelegate;
  typedef nuiFastDelegate2<const KeyType&, const ItemType&, bool> DisposeItemDelegate;

  const ItemType& SetDelegates(const CreateItemDelegate& rCreateDelegate, const DisposeItemDelegate& rDisposeDelegate)
  {
    nglCriticalSectionGuard g(mCS);
    mCreateItem = rCreateDelegate;
    mDisposeItem = rDisposeDelegate;
  }
  
  void SetMaxWeight(int64 MaxWeight)
  {
    nglCriticalSectionGuard g(mCS);
    mMaxWeight = MaxWeight;
  }

  int64 GetMaxWeight() const
  {
    nglCriticalSectionGuard g(mCS);
    return mMaxWeight;
  }

  int64 GetWeight() const
  {
    nglCriticalSectionGuard g(mCS);
    return mWeight;
  }

protected:
  int64 mMaxWeight;
  int64 mWeight;
  KeyList mKeys;
  ItemMap mItems;

  CreateItemDelegate mCreateItem;
  DisposeItemDelegate mDisposeItem;

  void Purge()
  {
    nglCriticalSectionGuard g(mCS);
    NGL_ASSERT(mDisposeItem);

    typename KeyList::reverse_iterator rit;
    typename KeyList::reverse_iterator rend;

    rit = mKeys.rbegin();
    rend = mKeys.rend();

    while (mWeight > mMaxWeight && rit != rend)
    {
      nglString key;
      {
        key = *rit;
        typename ItemMap::iterator it = mItems.find(key);
        NGL_ASSERT(it != mItems.end());

        CacheItem<KeyType, ItemType>& item(mItems[key]);
        if (item.GetRefCount() == 0)
        {
          NGL_LOG("radio", NGL_LOG_INFO, "Cache::Purge '%s'", key.GetChars());
          mWeight -= item.GetWeight();

          ItemType i(item.GetItem());
          mItems.erase(it);

          typename KeyList::iterator itr(rit.base());
          rit++;
          itr--;
          mKeys.erase(itr);
          mDisposeItem(key, i);
        }
        else
        {
          ++rit;
        }
      }

    }
  }

  void AddItem(const KeyType& rKey, const ItemType& rItem)
  {
    mKeys.push_front(rKey);
    typename KeyList::iterator i = mKeys.begin();
    mItems[rKey] = CacheItem<KeyType, ItemType>(i, rItem);

  }
  mutable nglCriticalSection mCS;
};

class FileCache : public Cache<nglPath, nglPath>
{
public:

  class File : public nglIStream
  {
  public:
    File(FileCache& rCache, const nglPath& rKey, const nglPath& rPath)
    : mrCache(rCache), mKey(rKey), mPath(rPath)
    {
    }

    virtual ~File()
    {
      delete mpStream;
      mpStream = NULL;

      mrCache.ReleaseItem(mKey, mPath);
    }

    bool Open()
    {
      mpStream = mPath.OpenRead();
      return mpStream != NULL;
    }

    nglStreamState GetState() const
    {
      return mpStream->GetState();
    }

    nglFileOffset GetPos() const
    {
      return mpStream->GetPos();
    }

    nglFileOffset SetPos (nglFileOffset Where, nglStreamWhence Whence = eStreamFromStart)
    {
      return mpStream->SetPos(Where, Whence);
    }

    nglFileSize Available (uint WordSize = 1)
    {
      return mpStream->Available(WordSize);
    }

    virtual int64 Read (void* pData, int64 WordCount, uint WordSize = 1)
    {
      return mpStream->Read(pData, WordCount, WordSize);
    }

    virtual void SetEndian(nglEndian Endian)
    {
      return mpStream->SetEndian(Endian);
    }
    
  private:
    nglIStream* mpStream;
    FileCache& mrCache;
    nglPath mKey;
    nglPath mPath;
    
  };




  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  FileCache(int64 MaxBytes, const nglPath& rSource, const nglPath& rDestination)
  : mSource(rSource), mDestination(rDestination), mByPass(rSource == rDestination)
  {
    SetMaxWeight(MaxBytes);
    SetDelegates(nuiMakeDelegate(this, &FileCache::Create), nuiMakeDelegate(this, &FileCache::Dispose));
  }

  virtual ~FileCache()
  {

  }

  bool Create(const nglPath& rSource, nglPath& rDestination, int64& rFileSize)
  {
    // Compute original path
    nglString p = rSource.GetPathName();
    nglString a = p.Extract(0, 1);
    nglString b = p.Extract(1, 1);
    nglString c = p.Extract(2, 1);
    p.Insert('/', 6);
    p.Insert('/', 3);

    nglString file = p;

    //nglPath path = "/space/new/medias/song";
    nglPath path = mSource;//"/data/glusterfs-storage/replica2all/song/";
    path += file;

    NGL_LOG("radio", NGL_LOG_INFO, "SetTrack %s\n", path.GetChars());


    // Compute destination path

    //nglPath path = "/space/new/medias/song";
    nglPath cache;

    if (mByPass)
    {
      cache = path;
    }
    else
    {
      cache = mDestination;//"/data/glusterfs-storage/replica2all/song/";
      cache += a;
      cache += b;
      cache += c;

      cache.Create(true);
      cache += rSource;

      NGL_LOG("radio", NGL_LOG_INFO, "SetTrack %s\n", path.GetChars());

      // Copy file
      path.Copy(cache);
    }

    // Compute file size
    rFileSize = cache.GetSize();

    rDestination = cache;

    return true;
  }

  nglIStream* GetStream(const nglPath& rSource)
  {
    nglPath path;
    if (GetItem(rSource, path))
    {
      File* pFile = new File(*this, rSource, path);
      if (pFile->Open())
        return pFile;
      delete pFile;
    }
    return NULL;
  }

  bool Dispose(const nglPath& rSource, const nglPath& rDestination)
  {
    if (!mByPass)
      rDestination.Delete();
    return true;
  }

#define CACHE_VERSION 0

  bool Save(nglOStream* pStream) const
  {
    nglCriticalSectionGuard g(mCS);
    pStream->WriteText("YaCache\0");

    int32 count = 0;

    int32 version = CACHE_VERSION;
    pStream->WriteInt32(&version);

    // Source path:
    count = mSource.GetPathName().GetLength();
    pStream->WriteInt32(&count);
    pStream->WriteText(mSource.GetChars());

    // Destination path:
    count = mDestination.GetPathName().GetLength();
    pStream->WriteInt32(&count);
    pStream->WriteText(mDestination.GetChars());

    // Write caches:
    count = mItems.size();
    pStream->WriteInt32(&count);

    KeyList::const_iterator it = mKeys.begin();
    KeyList::const_iterator end = mKeys.end();

    while (it != end)
    {
      const nglPath& rKey(*it);
      ItemMap::const_iterator it2 = mItems.find(rKey);
      NGL_ASSERT(it2 != mItems.end());
      const CacheItem<nglPath, nglPath>& rItem(it2->second);

      // Write source path (key)
      count = rKey.GetPathName().GetLength();
      pStream->WriteInt32(&count);
      pStream->WriteText(rKey.GetChars());

      // Write source path (key)
      count = rItem.GetItem().GetPathName().GetLength();
      pStream->WriteInt32(&count);
      pStream->WriteText(rItem.GetItem().GetPathName().GetChars());

      ++it;
    }
    return true;
  }

  bool Load(nglIStream* pStream)
  {
    nglCriticalSectionGuard g(mCS);

    const int32 BUF_SIZE = 4*1024;
    int64 r = 0;
    nglChar pChars[BUF_SIZE];
    memset(pChars, 0, BUF_SIZE);
    r = pStream->Read(pChars, 1, 8);
    nglString tag(pChars);
    if (tag != "YaCache!")
      return false;

    int32 count = 0;

    int32 version = CACHE_VERSION;
    pStream->ReadInt32(&version);

    // Source path:
    pStream->ReadInt32(&count);
    pStream->Read(pChars, 1, count);
    nglString src;
    src.Import(pChars, count, eEncodingNative);;
    mSource = src;

    // Destination path:
    count = mDestination.GetPathName().GetLength();
    pStream->ReadInt32(&count);
    pStream->Read(pChars, 1, count);
    nglString dest;
    dest.Import(pChars, count, eEncodingNative);
    mDestination = dest;

    // Read caches:
    pStream->ReadInt32(&count);

    int32 items = count;
    for (int32 i = 0; i < items; i++)
    {
      nglString key;
      nglString item;

      // Write source path (key)
      pStream->ReadInt32(&count);
      pStream->Read(pChars, 1, count);
      key.Import(pChars, count, eEncodingNative);

      // Write source path (key)
      pStream->ReadInt32(&count);
      pStream->Read(pChars, 1, count);
      item.Import(pChars, count, eEncodingNative);

      AddItem(key, item);
    }
    return true;
  }

  bool Save(const nglPath& rPath) const
  {
    if (mByPass)
      return true;
    nglOStream* pStream = rPath.OpenWrite();
    bool res = Save(pStream);
    delete pStream;
    return res;
  }

  bool Load(const nglPath& rPath)
  {
    if (mByPass)
      return true;
    nglIStream* pStream = rPath.OpenRead();
    bool res = Load(pStream);
    delete pStream;
    return res;
  }

  void Dump(nglString& rString) const
  {
    nglCriticalSectionGuard g(mCS);

    KeyList::const_iterator it = mKeys.begin();
    KeyList::const_iterator end = mKeys.end();

    int32 i = 0;
    int64 s = 0;
    while (it != end)
    {
      const nglPath& rKey(*it);
      ItemMap::const_iterator it2 = mItems.find(rKey);
      NGL_ASSERT(it2 != mItems.end());
      const CacheItem<nglPath, nglPath>& rItem(it2->second);

      // Write source path (key)
      rString.Add("[").Add(i).Add("] ");
      rString.Add(rItem.GetRefCount()).Add(" - ").Add(nglBytes(rItem.GetWeight())).Add(" | ");
      rString.Add(rKey.GetPathName());
      rString.Add(" -.> ");
      rString.Add(rItem.GetItem().GetPathName());

      rString.AddNewLine();
      i++;
      s += rItem.GetWeight();
      ++it;
    }

    rString.AddNewLine();
    rString.Add("Total files: ").Add(i).AddNewLine();
    rString.Add("Total bytes: ").Add(nglBytes(s)).AddNewLine();
  }
private:
  nglPath mSource;
  nglPath mDestination;
  bool mByPass;
};


