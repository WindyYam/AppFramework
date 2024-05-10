# App Framework
A wrapper for ActionScheduler to manage low power mode with ActionScheduler's timeline

You need ActionScheduler module as well, see https://github.com/WindyYam/ActionScheduler

To use it, you need to implement these 4 weak functions in your own code:

```
// The function for things you want to do before suspend
void AppFramework_PreSuspendHook(void);
// The function to do the low power suspend with specific time in milliseconds
void AppFramework_SuspendHook(uint32_t suspendTimeMs);
// The function for things to do after wakeup
void AppFramework_PostSuspendHook(void);
// The function for getting the timestamp in milliseconds(should not be affected by low power mode)
uint32_t AppFramework_GetTimestampHook(void);
```

Then, call AppFramework_Init() for initialization,

put AppFramework_Loop() in your main loop

and call AppFramework_Schedule and other functions for scheduling like ActionScheduler, but this time it is with low power mode support, so it will sleep and wakeup before the function task

To put onhold the low power mode in your code, include wake_lock.h, call ACQUIRE_WAKELOCK() to onhold, then RELEASE_WAKELOCK() to put it back.
