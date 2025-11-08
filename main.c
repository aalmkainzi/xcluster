
typedef struct S
{
    void*p;
    int k;
} S;

#include <stdio.h>

// undefs just for lsp
#undef XLIST_T
#undef XLIST_NAME
#undef XLIST_MAKE_SENTINEL
#undef XLIST_IS_SENTINEL
#undef XLIST_SENTINEL_SET_PTR
#undef XLIST_SENTINEL_GET_PTR
#undef XLIST_IMPL

#define XLIST_T S
#define XLIST_NAME ss
#define XLIST_MAKE_SENTINEL(a) ((a)->k = -1)
#define XLIST_IS_SENTINEL(a) ((a)->k == -1)
#define XLIST_SENTINEL_SET_PTR(a,pp) ((a)->p=(void*)pp)
#define XLIST_SENTINEL_GET_PTR(a) (a->p)
#define XLIST_IMPL
#include "xlist.h"

int main()
{
    ss a;
    ss_init(&a);
    
    for(int i = 0 ; i < 500 ; i++)
    {
        S* s = ss_put(&a, (S){.k = i + 1});
        if(i == 64)
        {
            ss_del(&a, s);
        }
    }
    
    for(ss_iter_t it = ss_begin(&a); it.ptr != ss_end(&a).ptr ; it = ss_iter_next(it) )
    {
        printf("%d\n", it.ptr->k);
    }
    
    ss_deinit(&a);
    return 0;
}
