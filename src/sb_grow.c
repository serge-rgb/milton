// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "common.h"

#include "memory.h"
#include "utils.h"

void* sb__sbgrowf(void *arr, int increment, int itemsize)
{
   int dbl_cur = arr ? 2*sb__sbm(arr) : 0;
   int min_needed = sb_count(arr) + increment;
   int m = dbl_cur > min_needed ? dbl_cur : min_needed;
   int *p = (int *) mlt_realloc(arr ? sb__sbraw(arr) : 0, itemsize * m + sizeof(int)*2);
   if (p) {
      if (!arr)
         p[1] = 0;
      p[0] = m;
      return p+2;
   } else {
      #ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
      STRETCHY_BUFFER_OUT_OF_MEMORY ;
      #endif
      return (void *) (2*sizeof(int)); // try to force a NULL pointer exception later
   }
}
