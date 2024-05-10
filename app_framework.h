#ifndef APP_FRAMEWORK_H
#define APP_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "action_scheduler.h"

#ifdef DEBUG_PRINT
void DEBUG_PRINTF(const char* fmt, ...);
// No buffer version is for print in ISR, which does block printing rather than put into print buffer
void DEBUG_PRINTF_NOBUFFER(const char* fmt, ...);
#else
#define DEBUG_PRINTF(fmt, ...)
#define DEBUG_PRINTF_NOBUFFER(fmt, ...)
#endif

void AppFramework_Init(void);
void AppFramework_Loop(void);
void AppFramework_WakeLockRecursive(bool hold);
ActionSchedulerId_t AppFramework_Schedule(uint32_t delayedTimeInMs, ActionCallback_t cb, void* arg);
ActionSchedulerId_t AppFramework_ScheduleReload(uint32_t delayedTimeInMs, uint32_t reloadTimeInMs, ActionCallback_t cb, void *arg);
bool AppFramework_Unschedule(ActionSchedulerId_t* actionId);
bool AppFramework_UnscheduleAll(ActionCallback_t cb);
void AppFramework_SetSuspendEnable(bool en);

// Hook functions for injecting the dependency(I don't like callback it waste function pointer and we don't need dynamic binding)
void AppFramework_PreSuspendHook(void);
void AppFramework_SuspendHook(uint32_t suspendTimeMs);
void AppFramework_PostSuspendHook(void);
uint32_t AppFramework_GetTimestampHook(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FRAMEWORK_H */
