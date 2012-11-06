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
      mCS.Unlock(); // Beware!!! We temporarly unlock the CS because calling mCreateItem may be time consuming!
      int64 Weight = 0;
      ItemType item;
      bool res = mCreateItem(rKey, item, Weight);
      mCS.Lock(); // Beware!!! We temporarly relock the CS because calling mCreateItem may have been time consuming!

      mKeys.push_front(rKey);
      typename KeyList::iterator i = mKeys.begin();
      CacheItem<KeyType, ItemType> cacheItem(i, item);
      mItems[rKey] = cacheItem;
      NGL_LOG("radio", NGL_LOG_INFO, "Cache::GetItem '%s'", rKey.GetChars());

      Purge();
      rItem = item;
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
    NGL_ASSERT(mDisposeItem);

    typename KeyList::reverse_iterator rit(mKeys.rbegin());
    typename KeyList::reverse_iterator rend(mKeys.rend());

    while (mWeight > mMaxWeight && rit != rend)
    {
      nglString key;
      {
        nglCriticalSectionGuard g(mCS);

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

  nglCriticalSection mCS;
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
    nglPath cache = mDestination;//"/data/glusterfs-storage/replica2all/song/";
    cache += a;
    cache += b;
    cache += c;

    cache.Create(true);
    cache += rSource;

    NGL_LOG("radio", NGL_LOG_INFO, "SetTrack %s\n", path.GetChars());

  // Copy file
    path.Copy(cache);

  // Compute file size
    rFileSize = cache.GetSize();

    rDestination = cache;

    return true;
  }

  nglIStream* GetStream(const nglPath& rSource)
  {
    nglPath path;
    if (GetItem(rSource, path))
      return new File(*this, rSource, path);
    return NULL;
  }

  bool Dispose(const nglPath& rSource, const nglPath& rDestination)
  {
    rDestination.Delete();
    return true;
  }


private:
  nglPath mSource;
  nglPath mDestination;
};


