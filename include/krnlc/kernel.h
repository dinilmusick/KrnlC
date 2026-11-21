#ifndef KRNLC_KERNEL_H
#define KRNLC_KERNEL_H
#include "krnlc/lifecycle.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KRNLC_ABI_VERSION 2u
#define KRNLC_MAX_FLOW_NODES 64u

typedef enum krnlc_result {
 KRNLC_SUCCESS=0, KRNLC_E_ARGUMENT, KRNLC_E_STATE, KRNLC_E_EXISTS,
 KRNLC_E_NOT_FOUND, KRNLC_E_CAPACITY, KRNLC_E_SCHEMA, KRNLC_E_HANDLER,
 KRNLC_E_EXPIRED, KRNLC_E_CANCELLED, KRNLC_E_CYCLE, KRNLC_E_DISPOSED,
 KRNLC_E_REENTRANT, KRNLC_E_UNSUPPORTED
} krnlc_result;
typedef struct krnlc_error {krnlc_result code;const char *service;const char *operation;const char *detail;} krnlc_error;
typedef enum krnlc_type {KRNLC_BOOL, KRNLC_I64, KRNLC_BYTES, KRNLC_TEXT} krnlc_type;
typedef struct krnlc_blob {const uint8_t *data;size_t length;} krnlc_blob;
typedef struct krnlc_value {const char *name;krnlc_type type;union {bool boolean;int64_t integer;krnlc_blob blob;} as;} krnlc_value;
typedef struct krnlc_record {krnlc_value *values;size_t count;size_t capacity;} krnlc_record;
typedef struct krnlc_field {const char *name;krnlc_type type;bool required;size_t max_bytes;} krnlc_field;
typedef struct krnlc_schema {const krnlc_field *fields;size_t count;bool reject_unknown;} krnlc_schema;
struct krnlc_context;
struct krnlc_runtime;
struct krnlc_service;
typedef krnlc_result (*krnlc_handler)(struct krnlc_context *,const krnlc_record *,krnlc_record *);
typedef struct krnlc_operation {const char *name;const krnlc_schema *input;const krnlc_schema *output;krnlc_handler run;} krnlc_operation;
typedef void (*krnlc_cleanup)(void *state);
typedef struct krnlc_descriptor {
 const char *name;const krnlc_operation *operations;size_t operation_count;
 const char *const *capabilities;size_t capability_count;
 const char *const *dependencies;size_t dependency_count;
 void *state;krnlc_cleanup cleanup;
} krnlc_descriptor;
typedef uint64_t (*krnlc_clock)(void *user);
typedef void (*krnlc_trace)(void *user,const krnlc_error *error);
typedef void (*krnlc_schedule)(void *user);
typedef struct krnlc_host {void *user;krnlc_clock now_ms;krnlc_trace trace;krnlc_schedule wake;} krnlc_host;
struct krnlc_bus;
typedef struct krnlc_service {struct krnlc_runtime *owner;const krnlc_descriptor *descriptor;krnlc_lifecycle lifecycle;struct krnlc_bus *events;bool disposed;bool busy;} krnlc_service;
typedef struct krnlc_metrics {uint64_t calls;uint64_t errors;uint64_t queue_full;} krnlc_metrics;
typedef struct krnlc_runtime {krnlc_service *services;size_t capacity;size_t count;krnlc_host host;krnlc_error last_error;krnlc_metrics metrics;} krnlc_runtime;
typedef struct krnlc_context {krnlc_runtime *runtime;krnlc_service *service;void *state;uint64_t deadline_ms;bool *cancelled;} krnlc_context;
krnlc_result krnlc_validate(const krnlc_schema *schema,const krnlc_record *record);
krnlc_result krnlc_runtime_init(krnlc_runtime *runtime,krnlc_service *memory,size_t capacity,const krnlc_host *host);
krnlc_result krnlc_register(krnlc_runtime *runtime,const krnlc_descriptor *descriptor,krnlc_service **service);
krnlc_service *krnlc_find(const krnlc_runtime *runtime,const char *name);
bool krnlc_has_capability(const krnlc_service *service,const char *capability);
krnlc_result krnlc_start(krnlc_service *service);
krnlc_result krnlc_stop(krnlc_service *service);
krnlc_result krnlc_dispose(krnlc_service *service);
krnlc_result krnlc_runtime_dispose(krnlc_runtime *runtime);
const krnlc_metrics *krnlc_runtime_metrics(const krnlc_runtime *runtime);
krnlc_result krnlc_invoke(krnlc_runtime *runtime,const char *service,const char *operation,const krnlc_record *input,krnlc_record *output,uint64_t deadline_ms,bool *cancelled);
const krnlc_error *krnlc_last_error(const krnlc_runtime *runtime);
const char *krnlc_result_string(krnlc_result code);
/* Queue is single-threaded; caller must hold all referenced records alive until dequeue. */
typedef struct krnlc_job {uint64_t id;const char *service;const char *operation;const krnlc_record *input;krnlc_record *output;uint64_t deadline_ms;bool cancelled;krnlc_result result;} krnlc_job;
typedef struct krnlc_queue {krnlc_job *items;size_t capacity;size_t count;uint64_t next_id;krnlc_runtime *runtime;} krnlc_queue;
krnlc_result krnlc_queue_init(krnlc_queue *queue,krnlc_runtime *runtime,krnlc_job *memory,size_t capacity);
krnlc_result krnlc_enqueue(krnlc_queue *queue,const krnlc_job *job,uint64_t *id);
krnlc_result krnlc_cancel(krnlc_queue *queue,uint64_t id);
krnlc_result krnlc_pump(krnlc_queue *queue,uint64_t *id,krnlc_result *result);
/* Mutable subscriptions may be added/removed inside callbacks; nesting is explicitly bounded. */
typedef void (*krnlc_event_fn)(void *user,const char *event,const krnlc_record *payload);
typedef struct krnlc_subscription {uint64_t id;const char *event;krnlc_event_fn callback;void *user;bool active;} krnlc_subscription;
typedef struct krnlc_bus {struct krnlc_service *owner;krnlc_subscription *items;size_t capacity;uint64_t next_id;unsigned depth;unsigned max_depth;bool disposed;} krnlc_bus;
krnlc_result krnlc_bus_init(krnlc_bus *bus,krnlc_subscription *memory,size_t capacity,unsigned max_depth);
krnlc_result krnlc_subscribe(krnlc_bus *bus,const char *event,krnlc_event_fn callback,void *user,uint64_t *id);
krnlc_result krnlc_unsubscribe(krnlc_bus *bus,uint64_t id);
krnlc_result krnlc_emit(krnlc_bus *bus,const char *event,const krnlc_record *payload);
krnlc_result krnlc_bus_dispose(krnlc_bus *bus);
krnlc_result krnlc_service_attach_bus(krnlc_service *service,krnlc_bus *bus);
krnlc_result krnlc_service_emit(krnlc_service *service,const char *event,const krnlc_record *payload);
/* Flow index dependencies are 64-bit masks; input_from<0 chooses node.input. */
typedef struct krnlc_flow_node {const char *operation;uint64_t dependencies;int input_from;const krnlc_record *input;krnlc_record *output;} krnlc_flow_node;
typedef struct krnlc_flow {const char *service;const krnlc_flow_node *nodes;size_t count;} krnlc_flow;
krnlc_result krnlc_flow_run(krnlc_runtime *runtime,const krnlc_flow *flow,uint64_t deadline_ms);
/* Transport-neutral remote call. Callback implements transport; no implicit network. */
typedef krnlc_result (*krnlc_transport)(void *user,const char *service,const char *operation,const krnlc_record *request,krnlc_record *response);
typedef struct krnlc_proxy {void *user;krnlc_transport send;const krnlc_descriptor *contract;} krnlc_proxy;
krnlc_result krnlc_proxy_call(const krnlc_proxy *proxy,const char *operation,const krnlc_record *request,krnlc_record *response);
#ifdef __cplusplus
}
#endif
#endif
