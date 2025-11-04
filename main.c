
typedef struct S
{
    void*p;
    int k;
} S;

#define XLIST_T S
#define XLIST_NAME ss
#define XLIST_IMPL
#define XLIST_SENTINEL (struct S){.k=INT_MIN}
#define XLIST_IS_SENTINEL(a) ((a)->k == INT_MIN)
#define XLIST_PTR_FIELD p

#include <stdio.h>
#include <limits.h>
#include "xlist.h"

int main()
{
    ss a;
    xlist_init(&a);
    
    xlist_put_uninit(&a);
    
    xlist_deinit(&a);
    return 0;
}
