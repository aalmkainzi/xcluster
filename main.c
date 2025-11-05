
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
    
    S elm = {.k = 50};
    
    for(int i = 0 ; i < 100 ; i++)
    {
        xlist_put(&a, elm);
    }
    
    xlist_deinit(&a);
    return 0;
}
