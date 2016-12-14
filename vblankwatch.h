/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VBLANK_WATCH_H
#define VBLANK_WATCH_H

#include <semaphore.h>
#include <cutils/log.h>

#include "autolock.h"

namespace android {

class vblank_watch
{
public:
   static vblank_watch *instance();

   ~vblank_watch()
   {
      sem_destroy(&sem);
   }

   inline bool post()
   {
#ifdef SYNC_POST
      AutoLock lock(&lock_, "post");
      int ret = lock.Lock();
      if (ret || counter)
         return false;
#endif

      counter++;

      ALOGD("posting %u\n", counter);
      sem_post(&sem);
      return true;
   }

   inline void wait()
   {
#ifdef SYNC_POST
      AutoLock lock(&lock_, "wait");
      int ret = lock.Lock();
      if (ret || !counter)
         return;
#endif

      ALOGD("waiting... (vblank count %u)\n", counter);
      sem_wait(&sem);

      counter = 0;
   }

protected:
   vblank_watch();

private:
   sem_t sem;
   mutable pthread_mutex_t lock_;

   unsigned counter;
};

}

#endif
