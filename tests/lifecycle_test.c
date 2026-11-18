#include "krnlc/lifecycle.h"
#include <assert.h>
#include <string.h>

static const bool accepted[KRNLC_STATE_COUNT][KRNLC_STATE_COUNT] = {
  {false, true,  false, false, false, false, false},
  {true,  false, true,  false, false, false, false},
  {false, false, false, true,  false, false, false},
  {false, false, true,  false, true,  false, false},
  {false, false, false, false, false, true,  false},
  {false, false, false, false, true,  false, true },
  {false, false, false, true,  false, false, false}
};

int main(void) {
  krnlc_lifecycle machine;
  assert(krnlc_lifecycle_init(&machine) == KRNLC_OK);
  assert(machine.state == KRNLC_UNINITIALIZED);
  assert(krnlc_lifecycle_assert_running(&machine) == KRNLC_NOT_RUNNING);
  for (int from = 0; from < KRNLC_STATE_COUNT; ++from) {
    for (int to = 0; to < KRNLC_STATE_COUNT; ++to) {
      machine.state = (krnlc_service_state)from;
      assert(krnlc_lifecycle_can_transition(&machine, (krnlc_service_state)to) == accepted[from][to]);
      krnlc_status status = krnlc_lifecycle_transition(&machine, (krnlc_service_state)to);
      assert(status == (accepted[from][to] ? KRNLC_OK : KRNLC_INVALID_TRANSITION));
      assert((int)machine.state == (accepted[from][to] ? to : from));
    }
  }
  assert(krnlc_lifecycle_init(&machine) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_INITIALIZING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_UNINITIALIZED) == KRNLC_OK); /* failed init */
  assert(krnlc_lifecycle_transition(&machine, KRNLC_INITIALIZING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_READY) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STARTING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_READY) == KRNLC_OK); /* failed start */
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STARTING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_RUNNING) == KRNLC_OK);
  assert(krnlc_lifecycle_assert_running(&machine) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STOPPING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_RUNNING) == KRNLC_OK); /* failed stop */
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STOPPING) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STOPPED) == KRNLC_OK);
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STARTING) == KRNLC_OK); /* restart */
  assert(krnlc_lifecycle_transition(&machine, KRNLC_RUNNING) == KRNLC_OK);
  assert(krnlc_lifecycle_assert_running(&machine) == KRNLC_OK);
  assert(krnlc_lifecycle_init(NULL) == KRNLC_INVALID_ARGUMENT);
  assert(krnlc_lifecycle_transition(NULL, KRNLC_RUNNING) == KRNLC_INVALID_ARGUMENT);
  assert(!krnlc_lifecycle_can_transition(NULL, KRNLC_RUNNING));
  assert(krnlc_lifecycle_assert_running(NULL) == KRNLC_INVALID_ARGUMENT);
  machine.state = KRNLC_RUNNING;
  assert(krnlc_lifecycle_transition(&machine, KRNLC_STATE_COUNT) == KRNLC_INVALID_ARGUMENT);
  assert(machine.state == KRNLC_RUNNING);
  machine.state = (krnlc_service_state)-1;
  assert(krnlc_lifecycle_transition(&machine, KRNLC_READY) == KRNLC_INVALID_ARGUMENT);
  assert(!krnlc_lifecycle_can_transition(&machine, KRNLC_READY));
  assert(krnlc_lifecycle_assert_running(&machine) == KRNLC_INVALID_ARGUMENT);
  assert(strcmp(krnlc_lifecycle_state_name(KRNLC_STOPPED), "stopped") == 0);
  assert(strcmp(krnlc_lifecycle_state_name((krnlc_service_state)-1), "unknown") == 0);
  return 0;
}
