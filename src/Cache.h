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

  CacheItem(const typename KeyList::iterator& rIterator, const ItemType& rItem, int64 Weight, bool AutoAcquire, int64 maxref, int64 hits)
  : mIterator(rIterator), mItem(rItem), mRefCount(AutoAcquire?1:0), mWeight(Weight), mHits(mRefCount), mMaxRefCount(MAX(maxref, mRefCount))
  {
  }

  CacheItem()
  : mRefCount(-1), mWeight(0), mMaxRefCount(0), mHits(0)
  {
  }

  CacheItem(const CacheItem<KeyType, ItemType>& rCacheItem)
  : mIterator(rCacheItem.mIterator), mItem(rCacheItem.mItem), mRefCount(rCacheItem.mRefCount), mWeight(rCacheItem.mWeight), mMaxRefCount(rCacheItem.mMaxRefCount), mHits(rCacheItem.mHits)
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
    mMaxRefCount = MAX(mRefCount, mMaxRefCount);
    mHits++;
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

  int64 GetMaxRefCount() const
  {
    return mMaxRefCount;
  }

  int64 GetHits() const
  {
    return mHits;
  }

private:
  ItemType mItem;
  int64 mWeight;
  typename KeyList::iterator mIterator;
  int64 mRefCount;
  int64 mMaxRefCount;
  int64 mHits;
};

template <class KeyType, class ItemType>
class Cache : public nuiNonCopyable
{
public:
  Cache()
  : mWeight(0), mMaxWeight(0), mByPass(false), mHits(0), mMisses(0)
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

      if (!res)
        return false;

      AddItem(rKey, rItem, Weight, true, 0, 1);
      NGL_LOG("radio", NGL_LOG_INFO, "Cache::GetItem '%s'", rKey.GetChars());

      Purge();
      OnCacheModified();
      mMisses++;
      return true;
    }

    mKeys.splice(mKeys.end(), mKeys, it->second.GetIterator());
    rItem = it->second.GetItem();
    it->second.Acquire();
    mHits++;
    return true;
  }

  virtual void OnCacheModified()
  {
  }

  bool ReleaseItem(const KeyType& rKey, const ItemType& rItem)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Cache::ReleaseItem '%s'", rKey.GetChars());
    nglCriticalSectionGuard g(mCS);
    typename ItemMap::iterator it = mItems.find(rKey);
    NGL_ASSERT(it != mItems.end());
    it->second.Release();
    return true;
  }

  typedef nuiFastDelegate3<const KeyType&, ItemType&, int64&, bool> CreateItemDelegate;
  typedef nuiFastDelegate2<const KeyType&, const ItemType&, bool> DisposeItemDelegate;

  void SetDelegates(const CreateItemDelegate& rCreateDelegate, const DisposeItemDelegate& rDisposeDelegate)
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

  int64 GetHits() const
  {
    nglCriticalSectionGuard g(mCS);
    return mHits;
  }

  int64 GetMisses() const
  {
    nglCriticalSectionGuard g(mCS);
    return mMisses;
  }

protected:
  int64 mMaxWeight;
  int64 mWeight;
  KeyList mKeys;
  ItemMap mItems;
  bool mByPass;
  int64 mHits;
  int64 mMisses;


  CreateItemDelegate mCreateItem;
  DisposeItemDelegate mDisposeItem;

  void Purge()
  {
    //nglThread::Sleep(20);
    nglCriticalSectionGuard g(mCS);

    if (mByPass)
      return;

    NGL_LOG("radio", NGL_LOG_INFO, "Cache::Purge current = %s  max = %s", nglBytes(mWeight).GetChars(), nglBytes(mMaxWeight).GetChars());
    NGL_ASSERT(mDisposeItem);

    typename KeyList::iterator rit(mKeys.begin());
    typename KeyList::iterator rend(mKeys.end());

    while ((mWeight > mMaxWeight) && (rit != rend))
    {
      const nglPath& k = *rit;
      nglString key = k.GetPathName();
      NGL_LOG("radio", NGL_LOG_INFO, "Cache::Purge try key %s", key.GetChars());


      typename ItemMap::iterator it = mItems.find(key);
      NGL_ASSERT(it != mItems.end());

      CacheItem<KeyType, ItemType>& item(mItems[key]);

      //NGL_LOG("radio", NGL_LOG_INFO, "Cache::Purge %s > %s - %s => %s (%d)", nglBytes(mWeight).GetChars(), nglBytes(mMaxWeight).GetChars(), key.GetChars(), nglBytes(item.GetWeight()).GetChars(), (int32)item.GetRefCount());

      if (item.GetRefCount() == 0)
      {
        NGL_LOG("radio", NGL_LOG_INFO, "Cache::Purge '%s'", key.GetChars());
        mWeight -= item.GetWeight();

        ItemType i(item.GetItem());
        mKeys.erase(rit++);
        mItems.erase(it);

        mDisposeItem(key, i);
      }
      else
      {
        ++rit;
      }
    }
  }

  void AddItem(const KeyType& rKey, const ItemType& rItem, int64 Weight, bool AutoAcquired, int64 maxrefs, int64 hits)
  {
    typename KeyList::iterator i = mKeys.insert(mKeys.end(), rKey);
    mItems[rKey] = CacheItem<KeyType, ItemType>(i, rItem, Weight, AutoAcquired, maxrefs, hits);
    mWeight += Weight;
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
  : mSource(rSource), mDestination(rDestination)
  {
    mByPass = rSource == rDestination;
    SetMaxWeight(MaxBytes);
    SetDelegates(nuiMakeDelegate(this, &FileCache::Create), nuiMakeDelegate(this, &FileCache::Dispose));
    Load(mDestination + nglPath("yastream.cache"));
    Purge();
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

    //NGL_LOG("radio", NGL_LOG_INFO, "SetTrack %s\n", path.GetChars());


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

      //NGL_LOG("radio", NGL_LOG_INFO, "SetTrack %s\n", path.GetChars());

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

  void OnCacheModified()
  {
    Save(mDestination + nglPath("yastream.cache"));
  }

#define CACHE_VERSION 1

  bool Save(nglOStream* pStream) const
  {
    nglCriticalSectionGuard g(mCS);
    double start = nglTime();
    pStream->WriteText("YaCache!");

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


    if (CACHE_VERSION > 0)
    {
      pStream->WriteInt64(&mHits);
      pStream->WriteInt64(&mMisses);
    }

    // Write caches:
    count = mItems.size();
    pStream->WriteInt32(&count);

    KeyList::const_iterator it = mKeys.begin();
    KeyList::const_iterator end = mKeys.end();

    int c = 0;
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


      if (CACHE_VERSION > 0)
      {
        int64 maxrefs = rItem.GetMaxRefCount();
        int64 hits = rItem.GetHits();
        pStream->WriteInt64(&maxrefs);
        pStream->WriteInt64(&hits);
      }

      c++;
      ++it;
    }

    double t = nglTime() - start;
    NGL_LOG("radio", NGL_LOG_INFO, "Saved %d cache items (%f seconds)", c, t);
    return true;
  }

  bool Load(nglIStream* pStream)
  {
    nglCriticalSectionGuard g(mCS);
    double start = nglTime();

    const int32 BUF_SIZE = 4*1024;
    int64 r = 0;
    nglChar pChars[BUF_SIZE];
    memset(pChars, 0, BUF_SIZE);
    r = pStream->Read(pChars, 1, 8);
    nglString tag(pChars);
    if (tag != "YaCache!")
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Tag not found: %s", tag.GetChars());
      return false;
    }

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

    if (CACHE_VERSION > 0)
    {
      int64 t = 0;
      pStream->ReadInt64(&t);
      mHits = t;
      pStream->ReadInt64(&t);
      mMisses = t;
    }

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

      int64 maxrefs = 0;
      int64 hits = 1;
      if (CACHE_VERSION > 0)
      {
        pStream->ReadInt64(&maxrefs);
        pStream->ReadInt64(&hits);
      }

      //NGL_LOG("radio", NGL_LOG_INFO, "load cache %s -> %s", key.GetChars(), item.GetChars());
      AddItem(key, item, nglPath(item).GetSize(), false, maxrefs, hits);
    }

    double t = nglTime() - start;
    NGL_LOG("radio", NGL_LOG_INFO, "loaded cache in %f seconds", t);

    return true;
  }

  bool Save(const nglPath& rPath) const
  {
    if (mByPass)
      return true;
    nglOStream* pStream = rPath.OpenWrite();
    if (!pStream)
      return false;
    bool res = Save(pStream);
    delete pStream;
    return res;
  }

  bool Load(const nglPath& rPath)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "FileCache::Load %s", rPath.GetChars());
    if (mByPass)
      return true;
    nglIStream* pStream = rPath.OpenRead();
    if (!pStream)
    {
      NGL_LOG("radio", NGL_LOG_INFO, "FileCache::Load unable to open file");
      return false;
    }
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

    DumpStats(rString);
  }

  void DumpStats(nglString& rString) const
  {
    nglCriticalSectionGuard g(mCS);

    rString.Add("Total files: ").Add((int64)mItems.size()).AddNewLine();
    rString.Add("Total bytes: ").Add(nglBytes(GetWeight())).Add(" (max = ").Add(nglBytes(GetMaxWeight())).Add(")").AddNewLine();

    rString.Add("Total accesses: ").Add(mHits + mMisses).Add(" (hits = ").Add(mHits).Add(" misses = ").Add(mMisses);

    if (mMisses + mHits != 0)
      rString.Add(" ratio = ").Add((double)mHits / (double)(mHits+mMisses));

    rString.Add(mMisses).Add(")").AddNewLine();
  }

private:
  nglPath mSource;
  nglPath mDestination;
};


