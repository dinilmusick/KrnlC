#include "krnlc/lifecycle.h"

static bool valid(krnlc_service_state state) {
  return state >= KRNLC_UNINITIALIZED && state < KRNLC_STATE_COUNT;
}

krnlc_status krnlc_lifecycle_init(krnlc_lifecycle *lifecycle) {
  if (!lifecycle) return KRNLC_INVALID_ARGUMENT;
  lifecycle->state = KRNLC_UNINITIALIZED;
  return KRNLC_OK;
}

bool krnlc_lifecycle_can_transition(const krnlc_lifecycle *lifecycle,
                                     krnlc_service_state next) {
  if (!lifecycle || !valid(lifecycle->state) || !valid(next)) return false;
  switch (lifecycle->state) {
    case KRNLC_UNINITIALIZED: return next == KRNLC_INITIALIZING;
    case KRNLC_INITIALIZING: return next == KRNLC_READY || next == KRNLC_UNINITIALIZED;
    case KRNLC_READY: return next == KRNLC_STARTING;
    case KRNLC_STARTING: return next == KRNLC_RUNNING || next == KRNLC_READY;
    case KRNLC_RUNNING: return next == KRNLC_STOPPING;
    case KRNLC_STOPPING: return next == KRNLC_STOPPED || next == KRNLC_RUNNING;
    case KRNLC_STOPPED: return next == KRNLC_STARTING;
    default: return false;
  }
}

krnlc_status krnlc_lifecycle_transition(krnlc_lifecycle *lifecycle,
                                         krnlc_service_state next) {
  if (!lifecycle || !valid(next) || !valid(lifecycle->state)) return KRNLC_INVALID_ARGUMENT;
  if (!krnlc_lifecycle_can_transition(lifecycle, next)) return KRNLC_INVALID_TRANSITION;
  lifecycle->state = next;
  return KRNLC_OK;
}

krnlc_status krnlc_lifecycle_assert_running(const krnlc_lifecycle *lifecycle) {
  if (!lifecycle || !valid(lifecycle->state)) return KRNLC_INVALID_ARGUMENT;
  return lifecycle->state == KRNLC_RUNNING ? KRNLC_OK : KRNLC_NOT_RUNNING;
}

const char *krnlc_lifecycle_state_name(krnlc_service_state state) {
  switch (state) {
    case KRNLC_UNINITIALIZED: return "uninitialized";
    case KRNLC_INITIALIZING: return "initializing";
    case KRNLC_READY: return "ready";
    case KRNLC_STARTING: return "starting";
    case KRNLC_RUNNING: return "running";
    case KRNLC_STOPPING: return "stopping";
    case KRNLC_STOPPED: return "stopped";
    default: return "unknown";
  }
}
