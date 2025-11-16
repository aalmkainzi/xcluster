
typedef struct S
{
    void*p;
    int k;
} S;

#include <stdio.h>

// undefs just for lsp
#undef XCLUSTER_T
#undef XCLUSTER_NAME
#undef XCLUSTER_MAKE_SENTINEL
#undef XCLUSTER_IS_SENTINEL
#undef XCLUSTER_SENTINEL_SET_PTR
#undef XCLUSTER_SENTINEL_GET_PTR
#undef XCLUSTER_IMPL

#define XCLUSTER_T S
#define XCLUSTER_NAME ss
#define XCLUSTER_MAKE_SENTINEL(a) ((a)->k = -1)
#define XCLUSTER_IS_SENTINEL(a) ((a)->k == -1)
#define XCLUSTER_SENTINEL_SET_PTR(a,pp) ((a)->p=(void*)pp)
#define XCLUSTER_SENTINEL_GET_PTR(a) ((a)->p)
#define XCLUSTER_IMPL
#include "xcluster.h"

int main()
{
    ss a;
    ss_init(&a);
    
    for(int i = 0 ; i < 500 ; i++)
    {
        S *s = ss_put(&a, (S){.k = i + 1});
    }
    
    for(S *it = ss_begin(&a) ; it != ss_end(&a) ; it = ss_next(it))
    {
        printf("%d\n", it->k);
    }
    
    ss_deinit(&a);
    return 0;
}
