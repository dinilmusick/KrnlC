# KrnlC

Portable C11 service kernel for the ULTRST ecosystem. Framework scope follows
`KrnlTS@9fdd0eb792361dd72b57cb678b933ba0c8fcae3e/IMPLEMENTATION-SPEC.md`:
service-local execution, schemas, lifecycle, bounded queues, events, functionality
orchestration and protocol-neutral proxies. **Cross-service orchestration belongs
to Skipo and is deliberately excluded.**

## Build and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix ./stage
```

Public headers: `<krnlc/lifecycle.h>` (PR #1 compatible) and `<krnlc/kernel.h>`.
Installed package: `find_package(KrnlC CONFIG REQUIRED)`, link
`KrnlC::krnlc`. `examples/consumer/` is built as an **independent CMake
project** by the `installed_consumer` test.

## Embedding contract

- No heap, OS calls, filesystem, networking, threads, clock or scheduler
  dependency in the kernel. All memory (`krnlc_service[]`, queue entries,
  subscription slots, record fields and string/byte payloads) comes from the
  caller. A slot remains occupied until `krnlc_runtime_dispose`; register-only
  storage cannot be expanded or removed individually. Oversized capacities
  that overflow pointer-size arithmetic are rejected before initialization.
- Descriptors, operation names, schema field arrays and capability/dependency
  name arrays are **borrowed** for the service lifetime. Record backing arrays,
  strings and byte spans are borrowed for each synchronous call or, for queued
  jobs, until `krnlc_pump` returns. No returned `krnlc_value` payload is owned
  by the framework; handlers populate caller-owned `krnlc_record` arrays.
- A service's `descriptor.state` belongs to its creator. Its `cleanup` callback
  is invoked **once** by `krnlc_dispose` or `krnlc_runtime_dispose`, including
  when the service was never started. It must release any owned subordinate
  state; never free caller-owned kernel slots. Once disposed, it cannot restart. A live dependency cannot be stopped while
  a dependent runs, or disposed while any dependent is still registered.
  The runtime disposes services in reverse registration order.
  `krnlc_runtime_dispose` invalidates every previously obtained service handle.
- `krnlc_stop` is restartable; `krnlc_dispose` is terminal and idempotent.
  Lifecycle transition matrix is intentionally unchanged from merged PR #1.
- The core is **single-threaded and non-reentrant for same-service invocation**.
  Calling `krnlc_invoke` recursively for the same service fails with
  `KRNLC_E_REENTRANT`. For multi-threaded hosts, all kernel calls, including
  event subscription mutations, require an application-provided external lock.
  No lock is silently assumed. Event subscriptions may be changed *inside*
  callbacks; nested emits are allowed up to explicit `max_depth`. Each event bus
  can be attached to only one service and must not be reinitialized while attached.
- Queue scheduling is explicit: `wake(user)` is notification only; it **does not
  execute a worker**. The host calls `krnlc_pump`. A queued job can be cancelled
  until dispatched. Running synchronous handlers cannot be forcibly preempted;
  they may check `ctx.cancelled` cooperatively. The optional millisecond clock
  callback is mandatory when nonzero deadlines are requested.
- Capacity exhaustion returns `KRNLC_E_CAPACITY`, not a successful fallback.
  No implicit dynamic allocation exists, so heap allocation failure inside the
  library is structurally impossible. A caller that allocates its own backing
  storage dynamically must handle its own allocation failures before passing it
  to KrnlC. No embedded hardware behavior is simulated.
- `krnlc_proxy_call` never opens sockets: its callback performs transport.
  `krnlc_flow_run` executes a dependency DAG inside **one** service (maximum
  64 nodes). A dependent service must be started only after its dependencies;
  it does not auto-start, run, or schedule other services. `input_from` selects the previous node's output record; all
  operations remain schema validated.
- Telemetry is caller-observable via `runtime.metrics` and the optional `trace`
  callback; error detail is a static borrowed string. `krnlc_last_error` is
  overwritten by later failures and has the same lifetime as the runtime.

## Platform independence

Build targets normal desktop C11 toolchains and microcontroller C runtimes that
provide `stdint.h`, `stddef.h`, `stdbool.h`, `string.h`. No POSIX dependence.
Actual bare-metal scheduler, network adapter, timebase and synchronization
must be supplied by the embedding platform and **are not claimed as tested**.

The pinned `GutC@0e26ac721c7a020078cfaf52e6fce259fe03301e` is recorded
in CMake and `docs/GUTC.md`. This kernel consumes **zero GutC functions**;
there is no fabricated native adapter. Optional integrations must prove the
revision and capabilities at build time before advertising support.

See `docs/PARITY.md` for behavior/exception matrix.

## ABI and lifetime guidance

`KRNLC_ABI_VERSION` is now `2`. This release adds runtime ownership
pointers to the public static-storage structures. Do not link objects compiled against an older header with a newer
KrnlC library: rebuild all consumers from the installed headers. Public
struct layouts are exposed to permit deterministic caller-owned static memory;
clients must treat service pointers as non-owning handles and not mutate runtime
internal fields directly. No binary ABI stabilization across versions is claimed.

A NULL operation schema permits arbitrary **valid** records, not malformed or
duplicate field names. An unspecified output schema still requires a coherent
output record. The optional trace callback sees queue capacity exhaustion and
pre-dispatch cancellation, in addition to invocation errors.
