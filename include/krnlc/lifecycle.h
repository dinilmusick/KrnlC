#ifndef KRNLC_LIFECYCLE_H
#define KRNLC_LIFECYCLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum krnlc_service_state {
  KRNLC_UNINITIALIZED = 0,
  KRNLC_INITIALIZING,
  KRNLC_READY,
  KRNLC_STARTING,
  KRNLC_RUNNING,
  KRNLC_STOPPING,
  KRNLC_STOPPED,
  KRNLC_STATE_COUNT
} krnlc_service_state;

typedef enum krnlc_status {
  KRNLC_OK = 0,
  KRNLC_INVALID_ARGUMENT,
  KRNLC_INVALID_TRANSITION,
  KRNLC_NOT_RUNNING
} krnlc_status;

typedef struct krnlc_lifecycle {
  krnlc_service_state state;
} krnlc_lifecycle;

/* The transition graph matches KrnlTS LifecycleManager, including rollback. */
krnlc_status krnlc_lifecycle_init(krnlc_lifecycle *lifecycle);
bool krnlc_lifecycle_can_transition(const krnlc_lifecycle *lifecycle,
                                     krnlc_service_state next);
krnlc_status krnlc_lifecycle_transition(krnlc_lifecycle *lifecycle,
                                         krnlc_service_state next);
krnlc_status krnlc_lifecycle_assert_running(const krnlc_lifecycle *lifecycle);
const char *krnlc_lifecycle_state_name(krnlc_service_state state);

#ifdef __cplusplus
}
#endif
#endif
