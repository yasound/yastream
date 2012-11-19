//
//  Cache.cpp
//  yastream
//
//  Created by Sébastien Métrot on 10/28/12.
//  Copyright (c) 2012 Yasound. All rights reserved.
//

#include "Cache.h"

nglString nglBytes(int64 b)
{
  nglString s;

  int64 k = b / 1024;
  b = b % 1024;
  int64 m = k / 1024;
  k = k % 1024;
  int64 g = m / 1024;
  m = m % 1024;
  int64 t = g / 1024;
  g = g % 1024;

  if (t)
    s.Add((int32)t).Add(" Tb");
  else if (g)
    s.Add((int32)g).Add(" Gb");
  else if (m)
    s.Add((int32)m).Add(" Mb");
  else if (k)
    s.Add((int32)k).Add(" Kb");
  else //if (b)
    s.Add((int32)b).Add(" bytes");

  return s;
}


