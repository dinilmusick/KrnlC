# KrnlTS ↔ KrnlC behavior matrix

Authority: `dinilmusick/KrnlTS@9fdd0eb792361dd72b57cb678b933ba0c8fcae3e`, `IMPLEMENTATION-SPEC.md`.
This is a conceptual API mapping, **not** a claim of TypeScript surface-level identity.

| KrnlTS behavior | KrnlC C11 counterpart | Scope/exception |
|---|---|---|
| LifecycleManager | `krnlc_lifecycle_*`, `krnlc_start/stop/dispose` | Seven-state matrix retained; dispose is separate terminal flag |
| Service definition / ServiceFacade | `krnlc_descriptor`, `krnlc_operation`, `krnlc_runtime` | Static descriptors, caller-owned storage, no JS reflection |
| Input/output runtime contracts | `krnlc_schema`, `krnlc_validate` | Explicit typed records; no JSON Schema evaluator |
| Functionality invocation | `krnlc_invoke`, `krnlc_context` | Synchronous; no JS promise or worker isolation |
| KrnlQ (`p-queue`) | `krnlc_queue_*` | Bounded, host-pumped; no automatic dispatch/thread |
| Service events/state | `krnlc_bus_*`, `krnlc_service_attach_bus` | Caller-owned event store, bounded nested dispatch |
| FunctionalityOrchestration | `krnlc_flow_run` | DAG up to 64 nodes, single service; no cross-service support |
| Service proxy | `krnlc_proxy_call` | Transport callback only; no built-in HTTP, GraphQL or gRPC |
| Error and runtime traces | `krnlc_last_error`, metrics, trace callback | No platform logger or persistent telemetry exporter |
| CLI, TypeScript AST/purity validation | Not provided | Node/TS-specific tooling is not portable into a bare C runtime |
| Cross-service ServiceOrchestration | Not provided | Explicitly owned by Skipo |

## Verification boundaries

CTest covers lifecycle 7×7 matrix, contracts/invalid data, dependencies,
capacity exhaustion, invocation failures, cancellation/deadlines, nested event
mutation, flow cycle detection, restart/terminal disposal and external installed
consumer compilation. GCC and Clang strict builds plus ASan/UBSan are expected
on supported hosts. Embedded targets and actual transports remain integration
responsibilities of embedding applications.
