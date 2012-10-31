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
  : mIterator(rIterator), mItem(rItem)
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
private:
  CacheItem mItem;
  int64 mWeight;
  typename KeyList::iterator mIterator;
};

template <class KeyType, class ItemType>
class Cache
{
public:
  Cache()
  {
  }

  ~Cache()
  {
  }

  typedef CacheItem<KeyType, ItemType> Item;
  typedef std::list<KeyType> KeyList;
  typedef std::map<KeyType, Item> ItemMap;

  const ItemType& GetItem(const KeyType& rKey)
  {
    nglCriticalSectionGuard g(mCS);
    typename ItemMap::iterator it = mItems.find(rKey);
    if (it == mItems.end())
    {
      mCS.Unlock(); // Beware!!! We temporarly unlock the CS because calling mCreateItem may be time consuming!
      int64 Weight = 0;
      const ItemType& item(mCreateItem(rKey, Weight));
      mCS.Lock(); // Beware!!! We temporarly relock the CS because calling mCreateItem may have been time consuming!

      const typename KeyList::iterator i = mKeys.push_front(rKey);
      CacheItem<KeyType, ItemType> cacheItem(i, item);
      mItems[rKey] = cacheItem;

      Purge();
      return item;
    }

    mKeys.splice(mKeys.begin(), mKeys, it->second.GetIterator());
    return it->second.GetItem();
  }

  typedef nuiFastDelegate2<const KeyType&, int64&, const ItemType&> CreateItemDelegate;
  typedef nuiFastDelegate2<const KeyType&, const ItemType&> DisposeItemDelegate;

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

private:
  int64 mMaxWeight;
  KeyList mKeys;
  ItemMap mItems;

  CreateItemDelegate mCreateItem;
  DisposeItemDelegate mDisposeItem;

  void Purge()
  {

  }

  nglCriticalSection mCS;
};

