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
private:
  CacheItem mItem;
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
    typename ItemMap::iterator it = mItems.find(rKey);
    if (it == mItems.end())
    {
      const ItemType& item(mCreateItem(rKey));
      const typename KeyList::iterator i = mKeys.push_front(rKey);
      CacheItem<KeyType, ItemType> cacheItem(i, item);
      mItems[rKey] = cacheItem;

      Purge();
      return item;
    }

    mKeys.splice(mKeys.begin(), mKeys, it->second.GetIterator());
    return it->second.GetItem();
  }

  typedef nuiFastDelegate1<const KeyType&, const ItemType&> CreateItemDelegate;
  typedef nuiFastDelegate2<const KeyType&, const ItemType&> DisposeItemDelegate;

  const ItemType& SetDelegates(const CreateItemDelegate& rCreateDelegate, const DisposeItemDelegate& rDisposeDelegate)
  {
    mCreateItem = rCreateDelegate;
    mDisposeItem = rDisposeDelegate;
  }
  

private:
  KeyList mKeys;
  ItemMap mItems;

  CreateItemDelegate mCreateItem;
  DisposeItemDelegate mDisposeItem;

  void Purge()
  {

  }
};

