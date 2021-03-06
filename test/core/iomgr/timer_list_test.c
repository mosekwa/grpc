/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "src/core/lib/iomgr/port.h"

// This test only works with the generic timer implementation
#ifdef GRPC_TIMER_USE_GENERIC

#include "src/core/lib/iomgr/timer.h"

#include <string.h>

#include <grpc/support/log.h>
#include "src/core/lib/debug/trace.h"
#include "test/core/util/test_config.h"

#define MAX_CB 30

extern grpc_tracer_flag grpc_timer_trace;
extern grpc_tracer_flag grpc_timer_check_trace;

static int cb_called[MAX_CB][2];

static void cb(grpc_exec_ctx *exec_ctx, void *arg, grpc_error *error) {
  cb_called[(intptr_t)arg][error == GRPC_ERROR_NONE]++;
}

static void add_test(void) {
  gpr_timespec start = gpr_now(GPR_CLOCK_REALTIME);
  int i;
  grpc_timer timers[20];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;

  gpr_log(GPR_INFO, "add_test");

  grpc_timer_list_init(start);
  grpc_timer_trace.value = 1;
  grpc_timer_check_trace.value = 1;
  memset(cb_called, 0, sizeof(cb_called));

  /* 10 ms timers.  will expire in the current epoch */
  for (i = 0; i < 10; i++) {
    grpc_timer_init(
        &exec_ctx, &timers[i],
        gpr_time_add(start, gpr_time_from_millis(10, GPR_TIMESPAN)),
        GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)i, grpc_schedule_on_exec_ctx),
        start);
  }

  /* 1010 ms timers.  will expire in the next epoch */
  for (i = 10; i < 20; i++) {
    grpc_timer_init(
        &exec_ctx, &timers[i],
        gpr_time_add(start, gpr_time_from_millis(1010, GPR_TIMESPAN)),
        GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)i, grpc_schedule_on_exec_ctx),
        start);
  }

  /* collect timers.  Only the first batch should be ready. */
  GPR_ASSERT(grpc_timer_check(
                 &exec_ctx,
                 gpr_time_add(start, gpr_time_from_millis(500, GPR_TIMESPAN)),
                 NULL) == GRPC_TIMERS_FIRED);
  grpc_exec_ctx_finish(&exec_ctx);
  for (i = 0; i < 20; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 10));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  GPR_ASSERT(grpc_timer_check(
                 &exec_ctx,
                 gpr_time_add(start, gpr_time_from_millis(600, GPR_TIMESPAN)),
                 NULL) == GRPC_TIMERS_CHECKED_AND_EMPTY);
  grpc_exec_ctx_finish(&exec_ctx);
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 10));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  /* collect the rest of the timers */
  GPR_ASSERT(grpc_timer_check(
                 &exec_ctx,
                 gpr_time_add(start, gpr_time_from_millis(1500, GPR_TIMESPAN)),
                 NULL) == GRPC_TIMERS_FIRED);
  grpc_exec_ctx_finish(&exec_ctx);
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 20));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  GPR_ASSERT(grpc_timer_check(
                 &exec_ctx,
                 gpr_time_add(start, gpr_time_from_millis(1600, GPR_TIMESPAN)),
                 NULL) == GRPC_TIMERS_CHECKED_AND_EMPTY);
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 20));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  grpc_timer_list_shutdown(&exec_ctx);
  grpc_exec_ctx_finish(&exec_ctx);
}

static gpr_timespec tfm(int m) {
  return gpr_time_from_millis(m, GPR_CLOCK_REALTIME);
}

/* Cleaning up a list with pending timers. */
void destruction_test(void) {
  grpc_timer timers[5];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;

  gpr_log(GPR_INFO, "destruction_test");

  grpc_timer_list_init(gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_timer_trace.value = 1;
  grpc_timer_check_trace.value = 1;
  memset(cb_called, 0, sizeof(cb_called));

  grpc_timer_init(
      &exec_ctx, &timers[0], tfm(100),
      GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)0, grpc_schedule_on_exec_ctx),
      gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_timer_init(
      &exec_ctx, &timers[1], tfm(3),
      GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)1, grpc_schedule_on_exec_ctx),
      gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_timer_init(
      &exec_ctx, &timers[2], tfm(100),
      GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)2, grpc_schedule_on_exec_ctx),
      gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_timer_init(
      &exec_ctx, &timers[3], tfm(3),
      GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)3, grpc_schedule_on_exec_ctx),
      gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_timer_init(
      &exec_ctx, &timers[4], tfm(1),
      GRPC_CLOSURE_CREATE(cb, (void *)(intptr_t)4, grpc_schedule_on_exec_ctx),
      gpr_time_0(GPR_CLOCK_REALTIME));
  GPR_ASSERT(grpc_timer_check(&exec_ctx, tfm(2), NULL) == GRPC_TIMERS_FIRED);
  grpc_exec_ctx_finish(&exec_ctx);
  GPR_ASSERT(1 == cb_called[4][1]);
  grpc_timer_cancel(&exec_ctx, &timers[0]);
  grpc_timer_cancel(&exec_ctx, &timers[3]);
  grpc_exec_ctx_finish(&exec_ctx);
  GPR_ASSERT(1 == cb_called[0][0]);
  GPR_ASSERT(1 == cb_called[3][0]);

  grpc_timer_list_shutdown(&exec_ctx);
  grpc_exec_ctx_finish(&exec_ctx);
  GPR_ASSERT(1 == cb_called[1][0]);
  GPR_ASSERT(1 == cb_called[2][0]);
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);
  gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
  add_test();
  destruction_test();
  return 0;
}

#else /* GRPC_TIMER_USE_GENERIC */

int main(int argc, char **argv) { return 1; }

#endif /* GRPC_TIMER_USE_GENERIC */
