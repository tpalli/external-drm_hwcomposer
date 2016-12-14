
#include "vblankwatch.h"

static android::vblank_watch *_instance = 0;

android::vblank_watch *android::vblank_watch::instance()
{
   if (!_instance)
      _instance = new vblank_watch;

   return _instance;
}

android::vblank_watch::vblank_watch()
{
   sem_init(&sem, 0, 0);
   counter = 0;
}

