#include "krnlc/kernel.h"
#include <string.h>

static bool word(const char *s) {return s && *s;}
static bool typename_valid(krnlc_type t) {return t>=KRNLC_BOOL && t<=KRNLC_TEXT;}
static krnlc_result report(krnlc_runtime *r,krnlc_result c,const char *s,const char *op,const char *detail) {
 if (r && c!=KRNLC_SUCCESS) {
  r->metrics.errors++;
  r->last_error.code=c;r->last_error.service=s;r->last_error.operation=op;r->last_error.detail=detail;
  if (r->host.trace) r->host.trace(r->host.user,&r->last_error);
 }
 return c;
}
const char *krnlc_result_string(krnlc_result c) {
 switch(c) {case KRNLC_SUCCESS:return "success";case KRNLC_E_ARGUMENT:return "invalid argument";
 case KRNLC_E_STATE:return "invalid state";case KRNLC_E_EXISTS:return "already exists";
 case KRNLC_E_NOT_FOUND:return "not found";case KRNLC_E_CAPACITY:return "capacity exhausted";
 case KRNLC_E_SCHEMA:return "schema mismatch";case KRNLC_E_HANDLER:return "handler failure";
 case KRNLC_E_EXPIRED:return "deadline exceeded";case KRNLC_E_CANCELLED:return "cancelled";
 case KRNLC_E_CYCLE:return "cycle detected";case KRNLC_E_DISPOSED:return "disposed";
 case KRNLC_E_REENTRANT:return "reentrant call";case KRNLC_E_UNSUPPORTED:return "adapter missing";
 default:return "unknown";}
}
krnlc_result krnlc_validate(const krnlc_schema *schema,const krnlc_record *record) {
 if (!record || (record->count && !record->values) || record->count>record->capacity) return KRNLC_E_ARGUMENT;
 if (!schema) return KRNLC_SUCCESS;
 if (schema->count && !schema->fields) return KRNLC_E_ARGUMENT;
 for(size_t i=0;i<schema->count;i++) {
  const krnlc_field *field=&schema->fields[i]; bool found=false;
  if (!word(field->name)||!typename_valid(field->type)) return KRNLC_E_ARGUMENT;
  for(size_t j=0;j<i;j++) if (strcmp(field->name,schema->fields[j].name)==0) return KRNLC_E_ARGUMENT;
  for(size_t j=0;j<record->count;j++) if (word(record->values[j].name)&&strcmp(field->name,record->values[j].name)==0) {
   const krnlc_value *v=&record->values[j];found=true;
   if (v->type!=field->type) return KRNLC_E_SCHEMA;
   if (v->type==KRNLC_TEXT || v->type==KRNLC_BYTES) {
    if (v->as.blob.length && !v->as.blob.data) return KRNLC_E_SCHEMA;
    if (field->max_bytes && v->as.blob.length>field->max_bytes) return KRNLC_E_SCHEMA;
   }
  }
  if (field->required && !found) return KRNLC_E_SCHEMA;
 }
 for(size_t i=0;i<record->count;i++) {
  const krnlc_value *v=&record->values[i];bool recognized=false;
  if (!word(v->name)||!typename_valid(v->type)) return KRNLC_E_SCHEMA;
  if ((v->type==KRNLC_TEXT||v->type==KRNLC_BYTES)&&v->as.blob.length&&!v->as.blob.data) return KRNLC_E_SCHEMA;
  for(size_t j=0;j<i;j++) if (strcmp(v->name,record->values[j].name)==0) return KRNLC_E_SCHEMA;
  for(size_t j=0;j<schema->count;j++) if (strcmp(v->name,schema->fields[j].name)==0) {recognized=true;break;}
  if (schema->reject_unknown && !recognized) return KRNLC_E_SCHEMA;
 }
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_runtime_init(krnlc_runtime *r,krnlc_service *memory,size_t capacity,const krnlc_host *host) {
 if (!r || (capacity && !memory)) return KRNLC_E_ARGUMENT;
 memset(r,0,sizeof(*r));r->services=memory;r->capacity=capacity;if(host) r->host=*host;
 if (capacity) memset(memory,0,capacity*sizeof(*memory));
 return KRNLC_SUCCESS;
}
krnlc_service *krnlc_find(const krnlc_runtime *r,const char *name) {
 if (!r || !word(name)) return NULL;
 for(size_t i=0;i<r->count;i++) if (r->services[i].descriptor && strcmp(name,r->services[i].descriptor->name)==0) return &r->services[i];
 return NULL;
}
bool krnlc_has_capability(const krnlc_service *s,const char *cap) {
 if (!s||!s->descriptor||!word(cap)) return false;
 for(size_t i=0;i<s->descriptor->capability_count;i++) if (s->descriptor->capabilities[i] && strcmp(s->descriptor->capabilities[i],cap)==0) return true;
 return false;
}
static const krnlc_operation *operation_find(const krnlc_descriptor *d,const char *name) {
 if(!d||!word(name)) return NULL;
 for(size_t i=0;i<d->operation_count;i++) if (strcmp(d->operations[i].name,name)==0) return &d->operations[i];
 return NULL;
}
krnlc_result krnlc_register(krnlc_runtime *r,const krnlc_descriptor *d,krnlc_service **out) {
 if (!r||!d||!word(d->name)||(d->operation_count&&!d->operations)||
     (d->capability_count&&!d->capabilities)||(d->dependency_count&&!d->dependencies)) return KRNLC_E_ARGUMENT;
 if (krnlc_find(r,d->name)) return KRNLC_E_EXISTS;
 if(r->count==r->capacity) return KRNLC_E_CAPACITY;
 for(size_t i=0;i<d->operation_count;i++) {
  if(!word(d->operations[i].name)||!d->operations[i].run) return KRNLC_E_ARGUMENT;
  for(size_t j=0;j<i;j++) if(strcmp(d->operations[j].name,d->operations[i].name)==0) return KRNLC_E_EXISTS;
 }
 for(size_t i=0;i<d->capability_count;i++) if(!word(d->capabilities[i])) return KRNLC_E_ARGUMENT;
 for(size_t i=0;i<d->dependency_count;i++) if(!word(d->dependencies[i])||!krnlc_find(r,d->dependencies[i])) return KRNLC_E_NOT_FOUND;
 krnlc_service *s=&r->services[r->count];memset(s,0,sizeof(*s));s->descriptor=d;
 if (krnlc_lifecycle_init(&s->lifecycle)!=KRNLC_OK) return KRNLC_E_STATE;
 r->count++;if(out)*out=s;return KRNLC_SUCCESS;
}
krnlc_result krnlc_start(krnlc_service *s) {
 if(!s||!s->descriptor) return KRNLC_E_ARGUMENT;
 if(s->disposed) return KRNLC_E_DISPOSED;
 if(s->lifecycle.state==KRNLC_RUNNING) return KRNLC_SUCCESS;
 if(s->lifecycle.state==KRNLC_UNINITIALIZED) {
  if(krnlc_lifecycle_transition(&s->lifecycle,KRNLC_INITIALIZING)!=KRNLC_OK ||
     krnlc_lifecycle_transition(&s->lifecycle,KRNLC_READY)!=KRNLC_OK) return KRNLC_E_STATE;
 }
 if(s->lifecycle.state==KRNLC_READY||s->lifecycle.state==KRNLC_STOPPED) {
  if(krnlc_lifecycle_transition(&s->lifecycle,KRNLC_STARTING)!=KRNLC_OK ||
     krnlc_lifecycle_transition(&s->lifecycle,KRNLC_RUNNING)!=KRNLC_OK) return KRNLC_E_STATE;
  return KRNLC_SUCCESS;
 }
 return KRNLC_E_STATE;
}
krnlc_result krnlc_stop(krnlc_service *s) {
 if(!s||!s->descriptor) return KRNLC_E_ARGUMENT;
 if(s->disposed)return KRNLC_E_DISPOSED;
 if(s->lifecycle.state==KRNLC_STOPPED)return KRNLC_SUCCESS;
 if(s->busy||s->lifecycle.state!=KRNLC_RUNNING)return KRNLC_E_STATE;
 if(krnlc_lifecycle_transition(&s->lifecycle,KRNLC_STOPPING)!=KRNLC_OK ||
    krnlc_lifecycle_transition(&s->lifecycle,KRNLC_STOPPED)!=KRNLC_OK)return KRNLC_E_STATE;
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_dispose(krnlc_service *s) {
 if(!s||!s->descriptor)return KRNLC_E_ARGUMENT;
 if(s->disposed)return KRNLC_SUCCESS;
 if(s->busy)return KRNLC_E_STATE;
 s->disposed=true;
 if(s->events)krnlc_bus_dispose(s->events);
 if(s->descriptor->cleanup)s->descriptor->cleanup(s->descriptor->state);
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_runtime_dispose(krnlc_runtime *r) {
 if(!r)return KRNLC_E_ARGUMENT;
 for(size_t i=0;i<r->count;i++)if(r->services[i].busy)return KRNLC_E_STATE;
 for(size_t i=0;i<r->count;i++) {
  krnlc_result rc=krnlc_dispose(&r->services[i]);if(rc!=KRNLC_SUCCESS)return rc;
 }
 r->count=0;return KRNLC_SUCCESS;
}
const krnlc_metrics *krnlc_runtime_metrics(const krnlc_runtime *r) {return r?&r->metrics:NULL;}
const krnlc_error *krnlc_last_error(const krnlc_runtime *r) {return r?&r->last_error:NULL;}
krnlc_result krnlc_invoke(krnlc_runtime *r,const char *name,const char *opname,const krnlc_record *in,krnlc_record *out,uint64_t deadline,bool *cancelled) {
 if(!r||!word(name)||!word(opname)||!in||!out) return KRNLC_E_ARGUMENT;
 r->metrics.calls++;
 krnlc_service *s=krnlc_find(r,name);
 if(!s)return report(r,KRNLC_E_NOT_FOUND,name,opname,"service not registered");
 if(s->disposed)return report(r,KRNLC_E_DISPOSED,name,opname,"service disposed");
 if(s->lifecycle.state!=KRNLC_RUNNING)return report(r,KRNLC_E_STATE,name,opname,"service not running");
 if(s->busy)return report(r,KRNLC_E_REENTRANT,name,opname,"same service recursion rejected");
 const krnlc_operation *op=operation_find(s->descriptor,opname);
 if(!op)return report(r,KRNLC_E_NOT_FOUND,name,opname,"operation not found");
 if(cancelled&&*cancelled)return report(r,KRNLC_E_CANCELLED,name,opname,"cancelled before dispatch");
 if(deadline&&!r->host.now_ms)return report(r,KRNLC_E_UNSUPPORTED,name,opname,"clock adapter required");
 if(deadline&&r->host.now_ms(r->host.user)>=deadline)return report(r,KRNLC_E_EXPIRED,name,opname,"deadline exceeded");
 krnlc_result rc=krnlc_validate(op->input,in);
 if(rc!=KRNLC_SUCCESS)return report(r,rc,name,opname,"invalid input");
 if(out->count>out->capacity || (out->capacity&&!out->values))return report(r,KRNLC_E_ARGUMENT,name,opname,"invalid output buffer");
 krnlc_context ctx={r,s,s->descriptor->state,deadline,cancelled};
 s->busy=true;rc=op->run(&ctx,in,out);s->busy=false;
 if(rc!=KRNLC_SUCCESS)return report(r,rc,name,opname,"handler returned failure");
 if(cancelled&&*cancelled)return report(r,KRNLC_E_CANCELLED,name,opname,"cancelled during dispatch");
 if(deadline&&r->host.now_ms(r->host.user)>=deadline)return report(r,KRNLC_E_EXPIRED,name,opname,"deadline exceeded after dispatch");
 rc=krnlc_validate(op->output,out);
 if(rc!=KRNLC_SUCCESS)return report(r,rc,name,opname,"invalid output");
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_queue_init(krnlc_queue *q,krnlc_runtime *r,krnlc_job *memory,size_t capacity) {
 if(!q||!r||(capacity&&!memory))return KRNLC_E_ARGUMENT;
 memset(q,0,sizeof(*q));q->runtime=r;q->items=memory;q->capacity=capacity;return KRNLC_SUCCESS;
}
krnlc_result krnlc_enqueue(krnlc_queue *q,const krnlc_job *job,uint64_t *id) {
 if(!q||!q->runtime||!job||!word(job->service)||!word(job->operation)||!job->input||!job->output)return KRNLC_E_ARGUMENT;
 if(q->count==q->capacity){q->runtime->metrics.queue_full++;return KRNLC_E_CAPACITY;}
 if(job->deadline_ms&&!q->runtime->host.now_ms)return KRNLC_E_UNSUPPORTED;
 krnlc_job next=*job;next.id=++q->next_id;if(!next.id)next.id=++q->next_id;
 next.result=KRNLC_SUCCESS;q->items[q->count++]=next;
 if(id)*id=next.id;
 if(q->runtime->host.wake)q->runtime->host.wake(q->runtime->host.user);
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_cancel(krnlc_queue *q,uint64_t id) {
 if(!q||!id)return KRNLC_E_ARGUMENT;
 for(size_t i=0;i<q->count;i++) if(q->items[i].id==id) {q->items[i].cancelled=true;return KRNLC_SUCCESS;}
 return KRNLC_E_NOT_FOUND;
}
krnlc_result krnlc_pump(krnlc_queue *q,uint64_t *id,krnlc_result *result) {
 if(!q||!q->runtime)return KRNLC_E_ARGUMENT;
 if(!q->count)return KRNLC_E_NOT_FOUND;
 krnlc_job job=q->items[0];q->count--;
 if(q->count)memmove(q->items,q->items+1,q->count*sizeof(*q->items));
 if(id)*id=job.id;
 krnlc_result rc=job.cancelled ? KRNLC_E_CANCELLED : krnlc_invoke(q->runtime,job.service,job.operation,job.input,job.output,job.deadline_ms,&job.cancelled);
 if(result)*result=rc;
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_bus_init(krnlc_bus *bus,krnlc_subscription *memory,size_t capacity,unsigned max_depth) {
 if(!bus||(capacity&&!memory)||!max_depth)return KRNLC_E_ARGUMENT;
 memset(bus,0,sizeof(*bus));bus->items=memory;bus->capacity=capacity;bus->max_depth=max_depth;
 if(capacity)memset(memory,0,sizeof(*memory)*capacity);
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_subscribe(krnlc_bus *bus,const char *event,krnlc_event_fn fn,void *user,uint64_t *id) {
 if(!bus||!word(event)||!fn)return KRNLC_E_ARGUMENT;
 if(bus->disposed)return KRNLC_E_DISPOSED;
 for(size_t i=0;i<bus->capacity;i++) if(!bus->items[i].active) {
  krnlc_subscription *s=&bus->items[i];s->active=true;s->event=event;s->callback=fn;s->user=user;
  s->id=++bus->next_id;if(!s->id)s->id=++bus->next_id;
  if(id)*id=s->id;
  return KRNLC_SUCCESS;
 }
 return KRNLC_E_CAPACITY;
}
krnlc_result krnlc_unsubscribe(krnlc_bus *bus,uint64_t id) {
 if(!bus||!id)return KRNLC_E_ARGUMENT;
 if(bus->disposed)return KRNLC_E_DISPOSED;
 for(size_t i=0;i<bus->capacity;i++) if(bus->items[i].active&&bus->items[i].id==id) {bus->items[i].active=false;return KRNLC_SUCCESS;}
 return KRNLC_E_NOT_FOUND;
}
krnlc_result krnlc_emit(krnlc_bus *bus,const char *event,const krnlc_record *payload) {
 if(!bus||!word(event))return KRNLC_E_ARGUMENT;
 if(bus->disposed)return KRNLC_E_DISPOSED;
 if(bus->depth>=bus->max_depth)return KRNLC_E_REENTRANT;
 const uint64_t limit=bus->next_id;bus->depth++;
 for(size_t i=0;i<bus->capacity && !bus->disposed;i++) {
  krnlc_subscription *s=&bus->items[i];
  if(s->active && s->id<=limit && s->event && strcmp(s->event,event)==0) {
   krnlc_event_fn cb=s->callback;void *user=s->user;cb(user,event,payload);
  }
 }
 bus->depth--;return KRNLC_SUCCESS;
}
krnlc_result krnlc_bus_dispose(krnlc_bus *bus) {
 if(!bus)return KRNLC_E_ARGUMENT;
 if(bus->disposed)return KRNLC_SUCCESS;
 bus->disposed=true;for(size_t i=0;i<bus->capacity;i++)bus->items[i].active=false;
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_service_attach_bus(krnlc_service *s,krnlc_bus *bus) {
 if(!s||!s->descriptor||!bus)return KRNLC_E_ARGUMENT;
 if(s->disposed||bus->disposed)return KRNLC_E_DISPOSED;
 if(s->lifecycle.state!=KRNLC_UNINITIALIZED)return KRNLC_E_STATE;
 s->events=bus;return KRNLC_SUCCESS;
}
krnlc_result krnlc_service_emit(krnlc_service *s,const char *event,const krnlc_record *payload) {
 if(!s||!s->descriptor)return KRNLC_E_ARGUMENT;
 if(s->disposed)return KRNLC_E_DISPOSED;
 if(s->lifecycle.state!=KRNLC_RUNNING)return KRNLC_E_STATE;
 if(!s->events)return KRNLC_E_UNSUPPORTED;
 return krnlc_emit(s->events,event,payload);
}
krnlc_result krnlc_flow_run(krnlc_runtime *r,const krnlc_flow *f,uint64_t deadline) {
 if(!r||!f||!word(f->service)||(f->count&&!f->nodes)||f->count>KRNLC_MAX_FLOW_NODES)return KRNLC_E_ARGUMENT;
 uint64_t done=0;size_t completed=0;
 for(size_t i=0;i<f->count;i++) {
  const krnlc_flow_node *n=&f->nodes[i];
  if(!word(n->operation)||!n->output||n->input_from < -1 || n->input_from >= (int)f->count)return KRNLC_E_ARGUMENT;
  if(n->input_from>=0 && !(n->dependencies & (UINT64_C(1)<<(unsigned)n->input_from)))return KRNLC_E_ARGUMENT;
  if(n->input_from<0 && !n->input)return KRNLC_E_ARGUMENT;
  if(n->dependencies & ~((f->count==64)?UINT64_MAX:((UINT64_C(1)<<f->count)-1)))return KRNLC_E_ARGUMENT;
  if(n->dependencies & (UINT64_C(1)<<i))return KRNLC_E_CYCLE;
 }
 while(completed<f->count) {
  bool progress=false;
  for(size_t i=0;i<f->count;i++) {
   uint64_t bit=UINT64_C(1)<<i;
   if((done&bit)||((f->nodes[i].dependencies & done)!=f->nodes[i].dependencies))continue;
   const krnlc_flow_node *n=&f->nodes[i];const krnlc_record *in=n->input_from>=0?f->nodes[n->input_from].output:n->input;
   krnlc_result rc=krnlc_invoke(r,f->service,n->operation,in,n->output,deadline,NULL);
   if(rc!=KRNLC_SUCCESS)return rc;
   done |= bit;completed++;progress=true;
  }
  if(!progress)return KRNLC_E_CYCLE;
 }
 return KRNLC_SUCCESS;
}
krnlc_result krnlc_proxy_call(const krnlc_proxy *p,const char *name,const krnlc_record *in,krnlc_record *out) {
 if(!p||!p->send||!p->contract||!word(p->contract->name)||!word(name)||!in||!out)return KRNLC_E_ARGUMENT;
 const krnlc_operation *op=operation_find(p->contract,name);
 if(!op)return KRNLC_E_NOT_FOUND;
 krnlc_result rc=krnlc_validate(op->input,in);if(rc!=KRNLC_SUCCESS)return rc;
 rc=p->send(p->user,p->contract->name,name,in,out);if(rc!=KRNLC_SUCCESS)return rc;
 return krnlc_validate(op->output,out);
}
