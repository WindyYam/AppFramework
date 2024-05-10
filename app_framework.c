#include <stdio.h>
#include <stdarg.h>
#include "app_framework.h"
#include "action_scheduler.h"
#include "critical_section.h"

#define MIN_WAKEUP_SAFEZONE_MS 0U
#define MIN_SUSPEND_TIME_DELAY 1U

// This app_framework is mainly for a wrapper environment to run action scheduler as app/task/event framework, and manage low power mode sleep time based on event scheduling
static uint32_t lastTick = 0;
static bool suspendEnabled = true;
static uint8_t powerLockRecursive = 0;  // Enable low power when lock is 0
static bool suspendedLastRound = false;
// This module assumes a 32768hz RTC

// used to abort the suspend if the timeline has updated due to interrupt and nearest event delay is now less than suspendTime
static bool ShouldAbortSuspend(uint32_t suspendTime)
{
    return ActionScheduler_GetNextEventDelay() < suspendTime;
}

__attribute__((weak)) void AppFramework_PreSuspendHook()
{
    // Override in user code for things you want to do before suspend
    // e.g. flush Uart TX
}

__attribute__((weak)) void AppFramework_SuspendHook(uint32_t suspendTimeMs)
{
    // Override in user code for implementing the low power suspend action
}

__attribute__((weak)) void AppFramework_PostSuspendHook()
{
    // Override in user code for things you want to do after wakeup
    // e.g. restart PLL if in stop1 mode
}

__attribute__((weak)) uint32_t AppFramework_GetTimestampHook(void)
{
    // Override in user code for returning timestamp
    return 0;
}

static void Suspend(uint32_t timeInMs)
{
  timeInMs = timeInMs > MIN_WAKEUP_SAFEZONE_MS ? timeInMs - MIN_WAKEUP_SAFEZONE_MS : 0U;   // A safezone to wake up a little bit earlier

  if(timeInMs > 0)
  {
    DEBUG_PRINTF("Sleep for %dms...\n", timeInMs);
    AppFramework_PreSuspendHook();
    uint32_t lock = Enter_Critical();
    // Interrupt scheduling in rare case can happen above the Enter_Critical line, result in most recent action delay updated
    if(ShouldAbortSuspend(timeInMs))
    {
      Exit_Critical(lock);
      DEBUG_PRINTF("Abort sleep\n");
      suspendedLastRound = false;
      return;
    }
    suspendedLastRound = true;
    AppFramework_SuspendHook(timeInMs);
    Exit_Critical(lock);
    AppFramework_PostSuspendHook();
  }
  else
  {
    suspendedLastRound = false;
  }
}

// This call will return the duration to the beginning of timeline, used on AppFramework_Schedule for an absolute time scheduling
static inline uint32_t GetDurationToTimelineBeginning()
{
    return (uint32_t)(AppFramework_GetTimestampHook() - lastTick) - ActionScheduler_GetProceedingTime();
}

void AppFramework_WakeLockRecursive(bool hold)
{
    uint32_t lock = Enter_Critical();
    if(hold)
    {
        powerLockRecursive += 1U;
    }
    else
    {
        powerLockRecursive -= 1U;
    }
    Exit_Critical(lock);
    if(hold)
    {
        DEBUG_PRINTF("Hold WakeLock %d\n", powerLockRecursive);
    }
    else
    {
        DEBUG_PRINTF("Release WakeLock %d\n", powerLockRecursive);
    }
}

// This schedule delay is relative to the timestamp when it executes, compare to to ActionScheduler_Schedule which is based on the head of the timeline at that moment
// The ActionScheduler_Proceed calls might not be continuous process when sleep is involved. In this case if you use ActionScheduler_Schedule for a delayed action (e.g. from wakeup ISR) before the timeline proceed, it will be executed in less than expected delay
// For example, if you've slept 10 secs, and get wake up by an interrupt which runs ISR to ActionScheduler_Schedule a 5 secs action, this action will be executed right away, as timeline head is going to proceed 10 sec, and your 5s action is relative to the beginning of timeline.
// Compare to ActionScheduler_Schedule for which delay is relative to the last round ActionScheduler_Proceed get called, this function make sure delay is based on the absolute time when it is called
// So, this function can be used on wakeup ISR, as it is delay to the current absolute time when it executes, the delay will be precise
// On the other hand, if you do want the delay to be based on the current timeline beginning(where the last proceed ends or in the middle of a proceed), use ActionScheduler_Schedule directly
// Compare these 2 calls: imagine 5s elapsed to last round, timeline is going to proceed 5s, and you have an event in 1s which also schedule another 1s delay event in the callback,
// 1. If the new event is scheduled by ActionScheduler_Schedule directly, the 5s proceed will fire both events, as the new event delay is based on where the first event is on
// 2. If the new event is scheduled by AppFramework_Schedule, the 5s proceed will fire the first event only, and another 1s passed the new event will fire, as AppFramework_Schedule is based on the current absolute time it executes
ActionSchedulerId_t AppFramework_Schedule(uint32_t delayedTimeInMs, ActionCallback_t cb, void *arg)
{
    return ActionScheduler_ScheduleReload(GetDurationToTimelineBeginning() + delayedTimeInMs, delayedTimeInMs, cb, arg);
}
// If you want the reload to be different to the delay
ActionSchedulerId_t AppFramework_ScheduleReload(uint32_t delayedTimeInMs, uint32_t reloadTimeInMs, ActionCallback_t cb, void *arg)
{
    return ActionScheduler_ScheduleReload(GetDurationToTimelineBeginning() + delayedTimeInMs, reloadTimeInMs, cb, arg);
}

bool AppFramework_Unschedule(ActionSchedulerId_t *actionId)
{
    return ActionScheduler_Unschedule(actionId);
}

bool AppFramework_UnscheduleAll(ActionCallback_t cb)
{
    return ActionScheduler_UnscheduleAll(cb);
}

void AppFramework_SetSuspendEnable(bool en)
{
    suspendEnabled = en;
    DEBUG_PRINTF("Suspend enable: %d\n", en);
}

__attribute__((weak)) void DEBUG_PRINTF(const char* fmt, ...)
{
    // Default implementation using printf
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stderr, fmt, argptr);
    va_end(argptr);
}

__attribute__((weak)) void DEBUG_PRINTF_NOBUFFER(const char* fmt, ...)
{
    // Default implementation using printf
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stderr, fmt, argptr);
    va_end(argptr);
}

void AppFramework_Init()
{
    ActionScheduler_Clear();
    lastTick = AppFramework_GetTimestampHook();
}

void AppFramework_Loop()
{
    // In this framework, system sleep for most of the time until
    // interrupt wake it up to process ISR code, or
    // the next scheduled action is about to occur, and wake up by RTC.
    // After that, system go back to sleep again.
    // So, the ActionScheduler module takes a very fundamental part in this framework.
    // For all user works, you should use AppFramework_Schedule to post action event all the time
    // For ISR, post the work load to AppFramework_Schedule to run in normal context
    // Waked up, either by RTC or by interrupt
    uint32_t nowtick = AppFramework_GetTimestampHook();

    uint32_t elapsed = nowtick - lastTick;
    if(suspendedLastRound)
    {
        DEBUG_PRINTF("Wake up from %dms\n", elapsed);
        suspendedLastRound = false;
    }

    (void)ActionScheduler_Proceed(elapsed);
    
    // Synchronize the timestamp and proceeding time, lock needed here as no ISR schedule is allowed in between
    uint32_t lock = Enter_Critical();
    lastTick = nowtick;
    ActionScheduler_ClearProceedingTime();
    Exit_Critical(lock);
    if(suspendEnabled)
    {
        if (powerLockRecursive == 0U)
        {
            uint32_t nextEventDelay = ActionScheduler_GetNextEventDelay();
            Suspend(nextEventDelay);
        }
    }
}