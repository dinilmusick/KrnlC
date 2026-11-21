#include <krnlc/kernel.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

typedef struct fixture {int invocations;int cleanup;int traces;int wakes;uint64_t clock;} fixture;
static uint64_t now(void *p) {return ((fixture *)p)->clock;}
static void trace(void *p,const krnlc_error *e) {fixture *f=p;f->traces++;assert(e->code!=KRNLC_SUCCESS);}
static void wake(void *p) {((fixture *)p)->wakes++;}
static void cleaned(void *p) {((fixture *)p)->cleanup++;}
static krnlc_result echo(krnlc_context *ctx,const krnlc_record *input,krnlc_record *output) {
 fixture *f=(fixture *)ctx->state;f->invocations++;
 if(output->capacity<1)return KRNLC_E_CAPACITY;
 output->values[0]=input->values[0];output->values[0].name="result";output->count=1;
 return KRNLC_SUCCESS;
}
static krnlc_result invalid_output(krnlc_context *ctx,const krnlc_record *input,krnlc_record *out) {
 (void)ctx;(void)input;if(!out->capacity)return KRNLC_E_CAPACITY;
 out->count=1;out->values[0].name="result";out->values[0].type=KRNLC_BOOL;out->values[0].as.boolean=true;
 return KRNLC_SUCCESS;
}
static krnlc_result fails(krnlc_context *ctx,const krnlc_record *in,krnlc_record *out) {
 (void)ctx;(void)in;(void)out;return KRNLC_E_HANDLER;
}
static krnlc_result recursive(krnlc_context *ctx,const krnlc_record *in,krnlc_record *out) {
 return krnlc_invoke(ctx->runtime,"calc","echo",in,out,0,NULL);
}
static krnlc_result marks_cancelled(krnlc_context *ctx,const krnlc_record *in,krnlc_record *out) {
 (void)in;(void)out;if(ctx->cancelled)*ctx->cancelled=true;
 return KRNLC_SUCCESS;
}
static const krnlc_field in_fields[]={{"number",KRNLC_I64,true,0}};
static const krnlc_field out_fields[]={{"result",KRNLC_I64,true,0}};
static const krnlc_schema input_schema={in_fields,1,true};
static const krnlc_schema output_schema={out_fields,1,true};
static const krnlc_operation ops[]={
 {"echo",&input_schema,&output_schema,echo},
 {"invalid",&input_schema,&output_schema,invalid_output},
 {"fail",&input_schema,&output_schema,fails},
 {"recursive",&input_schema,&output_schema,recursive},
 {"cancel",&input_schema,&output_schema,marks_cancelled},
 {"echo_chain",&output_schema,&output_schema,echo}
};
static const char *const caps[]={"math","local"};
static const char *const deps[]={"calc"};
typedef struct event_fixture {krnlc_bus *bus;uint64_t self;int visits;int nested;int added;} event_fixture;
static void event_late(void *p,const char *event,const krnlc_record *payload) {
 event_fixture *f=p;(void)event;(void)payload;f->added++;
}
static void event_first(void *p,const char *event,const krnlc_record *payload) {
 event_fixture *f=p;f->visits++;
 assert(krnlc_unsubscribe(f->bus,f->self)==KRNLC_SUCCESS);
 assert(krnlc_subscribe(f->bus,event,event_late,p,NULL)==KRNLC_SUCCESS);
 assert(krnlc_emit(f->bus,event,payload)==KRNLC_SUCCESS);
 f->nested++;
}
static void event_depth(void *p,const char *event,const krnlc_record *payload) {
 event_fixture *f=p;f->visits++;
 if(f->bus->depth==2)assert(krnlc_emit(f->bus,event,payload)==KRNLC_E_REENTRANT);
}
static void test_events(void) {
 krnlc_bus bus;krnlc_subscription slots[3];event_fixture f={0};f.bus=&bus;
 assert(krnlc_bus_init(&bus,slots,3,2)==KRNLC_SUCCESS);
 assert(krnlc_subscribe(&bus,"change",event_first,&f,&f.self)==KRNLC_SUCCESS);
 assert(krnlc_subscribe(&bus,"change",event_depth,&f,NULL)==KRNLC_SUCCESS);
 assert(krnlc_emit(&bus,"change",NULL)==KRNLC_SUCCESS);
 assert(f.visits==3 && f.added==1 && f.nested==1);
 assert(krnlc_emit(&bus,"change",NULL)==KRNLC_SUCCESS);
 assert(f.added==2 && f.visits==4);
 assert(krnlc_bus_dispose(&bus)==KRNLC_SUCCESS);
 assert(krnlc_bus_dispose(&bus)==KRNLC_SUCCESS);
 assert(krnlc_emit(&bus,"change",NULL)==KRNLC_E_DISPOSED);
}
static void test_schema(void) {
 krnlc_value v[2]={{.name="number",.type=KRNLC_I64,.as.integer=42}};
 krnlc_record rec={v,1,2};
 assert(krnlc_validate(&input_schema,&rec)==KRNLC_SUCCESS);
 v[0].type=KRNLC_BOOL;assert(krnlc_validate(&input_schema,&rec)==KRNLC_E_SCHEMA);
 v[0].type=KRNLC_I64;rec.count=0;assert(krnlc_validate(&input_schema,&rec)==KRNLC_E_SCHEMA);
 rec.count=2;v[1]=v[0];assert(krnlc_validate(&input_schema,&rec)==KRNLC_E_SCHEMA);
 v[1].name="extra";assert(krnlc_validate(&input_schema,&rec)==KRNLC_E_SCHEMA);
 rec.count=1;rec.capacity=0;assert(krnlc_validate(&input_schema,&rec)==KRNLC_E_ARGUMENT);
 krnlc_field bfield={"bytes",KRNLC_BYTES,true,3};krnlc_schema bytes={&bfield,1,true};
 v[0].name="bytes";v[0].type=KRNLC_BYTES;v[0].as.blob.data=(const uint8_t *)"abcd";v[0].as.blob.length=4;
 rec.capacity=2;assert(krnlc_validate(&bytes,&rec)==KRNLC_E_SCHEMA);
 v[0].as.blob.length=3;assert(krnlc_validate(&bytes,&rec)==KRNLC_SUCCESS);
 v[0].as.blob.data=NULL;assert(krnlc_validate(&bytes,&rec)==KRNLC_E_SCHEMA);
}
static krnlc_result transport(void *p,const char *s,const char *o,const krnlc_record *in,krnlc_record *out) {
 (void)p;assert(strcmp(s,"calc")==0);assert(strcmp(o,"echo")==0);
 if(out->capacity<1)return KRNLC_E_CAPACITY;
 out->values[0]=in->values[0];out->values[0].name="result";out->count=1;return KRNLC_SUCCESS;
}

static void test_null_schema_and_bounds(void) {
 assert(KRNLC_ABI_VERSION==2u);
 krnlc_value values[2]={{.name="a",.type=KRNLC_I64,.as.integer=4},
                        {.name="a",.type=KRNLC_I64,.as.integer=5}};
 krnlc_record record={values,1,2};
 assert(krnlc_validate(NULL,&record)==KRNLC_SUCCESS);
 record.count=2;assert(krnlc_validate(NULL,&record)==KRNLC_E_SCHEMA);
 record.count=1;values[0].type=(krnlc_type)999;
 assert(krnlc_validate(NULL,&record)==KRNLC_E_SCHEMA);
 values[0].type=KRNLC_BYTES;values[0].as.blob.length=1;values[0].as.blob.data=NULL;
 assert(krnlc_validate(NULL,&record)==KRNLC_E_SCHEMA);
 const krnlc_field bad[2]={{"a",KRNLC_I64,true,0},{NULL,KRNLC_I64,false,0}};
 krnlc_schema schema={bad,2,true};
 values[0].type=KRNLC_I64;assert(krnlc_validate(&schema,&record)==KRNLC_E_ARGUMENT);
 krnlc_runtime runtime;krnlc_service service;
 assert(krnlc_runtime_init(&runtime,&service,SIZE_MAX/sizeof(service)+1,NULL)==KRNLC_E_ARGUMENT);
 krnlc_bus bus;krnlc_subscription sub;
 assert(krnlc_bus_init(&bus,&sub,SIZE_MAX/sizeof(sub)+1,1)==KRNLC_E_ARGUMENT);
 krnlc_queue queue;krnlc_job job;
 assert(krnlc_queue_init(&queue,&runtime,&job,SIZE_MAX/sizeof(job)+1)==KRNLC_E_ARGUMENT);
}
static void test_dependency_lifetime(void) {
 krnlc_runtime r;krnlc_service slots[2];fixture f={0};
 krnlc_descriptor base={.name="base",.state=&f,.cleanup=cleaned};
 const char *const required[]={"base"};
 krnlc_descriptor client={.name="client",.dependencies=required,.dependency_count=1};
 assert(krnlc_runtime_init(&r,slots,2,NULL)==KRNLC_SUCCESS);
 krnlc_service *a,*b;
 assert(krnlc_register(&r,&base,&a)==KRNLC_SUCCESS);
 assert(krnlc_register(&r,&client,&b)==KRNLC_SUCCESS);
 assert(krnlc_start(b)==KRNLC_E_STATE);
 assert(krnlc_start(a)==KRNLC_SUCCESS);
 assert(krnlc_start(b)==KRNLC_SUCCESS);
 assert(krnlc_stop(a)==KRNLC_E_STATE);
 assert(krnlc_dispose(a)==KRNLC_E_STATE);
 assert(krnlc_stop(b)==KRNLC_SUCCESS);
 assert(krnlc_stop(a)==KRNLC_SUCCESS);
 assert(krnlc_dispose(a)==KRNLC_E_STATE);
 assert(krnlc_start(b)==KRNLC_E_STATE);
 assert(krnlc_start(a)==KRNLC_SUCCESS);
 assert(krnlc_start(b)==KRNLC_SUCCESS);
 assert(krnlc_runtime_dispose(&r)==KRNLC_SUCCESS);
 assert(f.cleanup==1 && a->disposed && b->disposed);
 assert(krnlc_runtime_dispose(&r)==KRNLC_SUCCESS);
}
int main(void) {
 test_schema();test_events();test_null_schema_and_bounds();test_dependency_lifetime();
 fixture f={0};krnlc_host host={&f,now,trace,wake};krnlc_runtime runtime;krnlc_service storage[2];
 assert(krnlc_runtime_init(&runtime,storage,2,&host)==KRNLC_SUCCESS);
 krnlc_descriptor calc={"calc",ops,6,caps,2,NULL,0,&f,cleaned};
 krnlc_descriptor dependent={"dep",ops,1,NULL,0,deps,1,&f,NULL};
 krnlc_service *s=NULL;
 assert(krnlc_register(&runtime,&dependent,NULL)==KRNLC_E_NOT_FOUND);
 assert(krnlc_register(&runtime,&calc,&s)==KRNLC_SUCCESS);
 assert(s==krnlc_find(&runtime,"calc"));
 assert(krnlc_has_capability(s,"math")&&!krnlc_has_capability(s,"unknown"));
 assert(krnlc_register(&runtime,&calc,NULL)==KRNLC_E_EXISTS);
 assert(krnlc_register(&runtime,&dependent,NULL)==KRNLC_SUCCESS);
 krnlc_bus service_bus;krnlc_subscription service_events[1];
 assert(krnlc_bus_init(&service_bus,service_events,1,1)==KRNLC_SUCCESS);
 assert(krnlc_service_attach_bus(s,&service_bus)==KRNLC_SUCCESS);
 assert(krnlc_service_attach_bus(&storage[1],&service_bus)==KRNLC_E_EXISTS);
 assert(krnlc_register(&runtime,&(krnlc_descriptor){.name="third"},NULL)==KRNLC_E_CAPACITY);
 krnlc_value iv[]={{.name="number",.type=KRNLC_I64,.as.integer=42}};
 krnlc_value ov[1];krnlc_record in={iv,1,1};krnlc_record out={ov,0,1};
 assert(krnlc_invoke(&runtime,"calc","echo",&in,&out,0,NULL)==KRNLC_E_STATE);
 assert(krnlc_start(s)==KRNLC_SUCCESS);
 assert(krnlc_service_attach_bus(s,&service_bus)==KRNLC_E_STATE);
 assert(krnlc_service_emit(s,"x",NULL)==KRNLC_SUCCESS);
 assert(krnlc_start(s)==KRNLC_SUCCESS);
 assert(krnlc_invoke(&runtime,"calc","echo",&in,&out,0,NULL)==KRNLC_SUCCESS);
 assert(out.count==1 && out.values[0].as.integer==42);
 assert(krnlc_invoke(&runtime,"calc","invalid",&in,&out,0,NULL)==KRNLC_E_SCHEMA);
 assert(krnlc_invoke(&runtime,"calc","fail",&in,&out,0,NULL)==KRNLC_E_HANDLER);
 assert(krnlc_last_error(&runtime)->code==KRNLC_E_HANDLER && f.traces>0);
 assert(krnlc_invoke(&runtime,"calc","recursive",&in,&out,0,NULL)==KRNLC_E_REENTRANT);
 assert(krnlc_invoke(&runtime,"calc","unknown",&in,&out,0,NULL)==KRNLC_E_NOT_FOUND);
 f.clock=20;assert(krnlc_invoke(&runtime,"calc","echo",&in,&out,19,NULL)==KRNLC_E_EXPIRED);
 bool canceled=true;assert(krnlc_invoke(&runtime,"calc","echo",&in,&out,0,&canceled)==KRNLC_E_CANCELLED);
 canceled=false;assert(krnlc_invoke(&runtime,"calc","cancel",&in,&out,0,&canceled)==KRNLC_E_CANCELLED);

 krnlc_job memory[2];krnlc_queue q;
 assert(krnlc_queue_init(&q,&runtime,memory,2)==KRNLC_SUCCESS);
 krnlc_job j={0};j.service="calc";j.operation="echo";j.input=&in;j.output=&out;
 uint64_t a=0,b=0,executed=0;krnlc_result done=KRNLC_E_ARGUMENT;
 assert(krnlc_enqueue(&q,&j,&a)==KRNLC_SUCCESS);
 assert(f.wakes==1);
 assert(krnlc_enqueue(&q,&j,&b)==KRNLC_SUCCESS);
 assert(krnlc_enqueue(&q,&j,NULL)==KRNLC_E_CAPACITY);
 assert(krnlc_last_error(&runtime)->code==KRNLC_E_CAPACITY);
 assert(krnlc_cancel(&q,a)==KRNLC_SUCCESS);
 assert(krnlc_pump(&q,&executed,&done)==KRNLC_SUCCESS && executed==a && done==KRNLC_E_CANCELLED);
 assert(krnlc_last_error(&runtime)->code==KRNLC_E_CANCELLED);
 assert(krnlc_pump(&q,&executed,&done)==KRNLC_SUCCESS && executed==b && done==KRNLC_SUCCESS);
 assert(krnlc_pump(&q,NULL,NULL)==KRNLC_E_NOT_FOUND);
 j.deadline_ms=19;assert(krnlc_enqueue(&q,&j,&a)==KRNLC_SUCCESS);
 assert(krnlc_pump(&q,NULL,&done)==KRNLC_SUCCESS && done==KRNLC_E_EXPIRED);
 krnlc_value ov1[1],ov2[1];krnlc_record step1={ov1,0,1},step2={ov2,0,1};
 krnlc_flow_node nodes[2]={
  {"echo",0,-1,&in,&step1},
  {"echo_chain",UINT64_C(1),0,NULL,&step2}
 };
 krnlc_flow flow={"calc",nodes,2};
 assert(krnlc_flow_run(&runtime,&flow,0)==KRNLC_SUCCESS);
 assert(step2.count==1 && step2.values[0].as.integer==42);
 nodes[0].dependencies=UINT64_C(2);
 assert(krnlc_flow_run(&runtime,&flow,0)==KRNLC_E_CYCLE);
 nodes[0].dependencies=0;nodes[1].input_from=0;nodes[1].dependencies=0;
 assert(krnlc_flow_run(&runtime,&flow,0)==KRNLC_E_ARGUMENT);
 krnlc_proxy proxy={NULL,transport,&calc};out.count=0;
 assert(krnlc_proxy_call(&proxy,"echo",&in,&out)==KRNLC_SUCCESS);
 assert(krnlc_proxy_call(&proxy,"missing",&in,&out)==KRNLC_E_NOT_FOUND);
 assert(krnlc_stop(s)==KRNLC_SUCCESS);
 assert(krnlc_stop(s)==KRNLC_SUCCESS);
 assert(krnlc_start(s)==KRNLC_SUCCESS);
 assert(krnlc_dispose(s)==KRNLC_E_STATE); /* dependent must be disposed first */
 assert(krnlc_dispose(&storage[1])==KRNLC_SUCCESS);
 assert(krnlc_dispose(s)==KRNLC_SUCCESS);
 assert(krnlc_dispose(s)==KRNLC_SUCCESS && f.cleanup==1);
 assert(krnlc_start(s)==KRNLC_E_DISPOSED);
 assert(service_bus.disposed);
 assert(krnlc_service_emit(s,"x",NULL)==KRNLC_E_DISPOSED);
 assert(krnlc_invoke(&runtime,"calc","echo",&in,&out,0,NULL)==KRNLC_E_DISPOSED);
 assert(krnlc_runtime_metrics(&runtime)->calls>=10);
 assert(krnlc_runtime_metrics(&runtime)->errors>0);
 assert(krnlc_runtime_metrics(&runtime)->queue_full==1);
 assert(krnlc_runtime_dispose(&runtime)==KRNLC_SUCCESS);
 assert(krnlc_runtime_dispose(&runtime)==KRNLC_SUCCESS);

 krnlc_runtime no_clock;krnlc_service one[1];assert(krnlc_runtime_init(&no_clock,one,1,NULL)==KRNLC_SUCCESS);
 assert(krnlc_register(&no_clock,&calc,NULL)==KRNLC_SUCCESS);
 assert(krnlc_start(&one[0])==KRNLC_SUCCESS);
 assert(krnlc_invoke(&no_clock,"calc","echo",&in,&out,10,NULL)==KRNLC_E_UNSUPPORTED);
 return 0;
}
