#ifndef WAKE_LOCK_H
#define WAKE_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_framework.h"

// For every C/C++ source file which include this header, module_wake_lock static variable will be created, to describe the lock used on that module
// In general, ACQUIRE_WAKELOCK() and RELEASE_WAKELOCK() should be appeared in pairs in the same file, not from another file function, as the module_wake_lock variable will be different
static uint8_t module_wake_lock = 0;

#define ACQUIRE_WAKELOCK() {\
                                module_wake_lock ++;\
                                DEBUG_PRINTF("%s():", __FUNCTION__);\
                                AppFramework_WakeLockRecursive(true);\
                            }
#define RELEASE_WAKELOCK() {   if(module_wake_lock > 0){\
                                    DEBUG_PRINTF("%s():", __FUNCTION__);\
                                    AppFramework_WakeLockRecursive(false);\
                                    module_wake_lock --;\
                                }\
                                else\
                                {\
                                    DEBUG_PRINTF("Release WakeLock Error! %s\n", __FUNCTION__);\
                                }\
                            }

#define IS_WAKELOCK() module_wake_lock

#ifdef __cplusplus
}
#endif

#endif /* WAKE_LOCK_H */
