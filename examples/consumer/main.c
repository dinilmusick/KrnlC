#include <krnlc/kernel.h>
int main(void) {
 krnlc_runtime r;
 krnlc_service memory[1];
 if(krnlc_runtime_init(&r,memory,1,NULL)!=KRNLC_SUCCESS)return 1;
 if(KRNLC_ABI_VERSION!=1u)return 2;
 return r.capacity==1 ? 0 : 3;
}
