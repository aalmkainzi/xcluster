/*
XList stands for "Exploding Array",
when deleting, it splits the the hole into 2 arrays,
in order to maintain pointer/iterator stability of other elements.

when inserting, it looks into stack of non_full_buckets, if empty, alloc new bucket.

when deleting, split into 2 arrays, left one is full from the start, right one will keep the old cap + left_arr_size

but how will iterators be stable? and what about it_next()?

*/

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(XLIST_T) || !defined(XLIST_NAME) || !defined(XLIST_SENTINEL) || !defined(XLIST_IS_SENTINEL) || !defined(XLIST_PTR_FIELD)
    #error "Must define XLIST_T, XLIST_NAME, XLIST_SENTINEL, and XLIST_IS_SENTINEL"
#endif

typedef struct XListBucket
{
    // maybe we do add prev ptr here. not for linked list traversal, but to say they are actually connected in memory, so if this bucket is about to be deleted, the prev one can just extend its cap. the prev field can be NULL
    // issue is, if prev is already deleted....
    // this idea is a wash, just have a bucket reserve so we can reuse its buffers
    
    struct XListBucket *next;
    struct XListBucket *prev;
    size_t not_full_index;
    XLIST_T *elms;
    size_t count;
    size_t cap;
} XListBucket;

typedef struct XList
{
    XListBucket **not_full_buckets;
    size_t nfb_count;
    size_t nfb_cap;
    
    XListBucket *tail;
    XListBucket *head;
    XListBucket *end_sentinel;
    size_t b_count;
    
    size_t prev_cap;
    size_t count;
    
    void **allocations;
    size_t al_count;
    size_t al_cap;
} XList;

typedef struct XListIterator
{
    XLIST_T *ptr;
    //size_t id; // something to do with index+bucket?
    // maybe we can store a second buffer at the bucket?
    // or the one-past-last elm stores some data about owning bucket? (might be the way)
    // issue is, without a ptr to the bucket, we dont know where the end is (maybe sentinel value?)
    
    // new idea:
    //void *allocation; //key1
    //size_t idx_within_allocation; //key2
    // these two fields will be used as key into some map
    
    // or maybe use sentinel value to mark end of array (-1 for ints for example)
    // the address of the sentinel value will be used as a key in some map to get the owning bucket. (THIS MIGHT BE THE WAY!)
    
    // new idea: the main list structure holds array of all buckets, just check if ptr >= begin and < end of which bucket
    
    // maybe require the sentinel have a ptr field, which we can reuse to make it a ptr to the bucket
    // so this means: requires sentinel, but with one field we can customize
} XListIterator;


void xlist_init(XList *ls)
{
    memset(ls, 0, sizeof(XList));
    
    ls->end_sentinel = malloc(sizeof(XListBucket));
    memset(ls->end_sentinel, 0, sizeof(XListBucket));
    
    ls->head = ls->end_sentinel;
    ls->tail = ls->end_sentinel;
    
    ls->nfb_cap = 16;
    ls->not_full_buckets = malloc(sizeof(XListBucket*) * ls->nfb_cap);
    
    ls->prev_cap = 64;
    
    ls->al_cap = 16;
    ls->allocations = malloc(sizeof(*ls->allocations) * ls->al_cap);
}

#define XLIST_MAYBE_GROW(ptr, cap_ptr, count, ...)                       \
do                                                                       \
{                                                                        \
    size_t _n = 0 __VA_OPT__(+1) ? 0 __VA_OPT__(+(__VA_ARGS__)) : 1;     \
    size_t *_cap = (cap_ptr);                                            \
    const size_t _count = (count);                                       \
    if((_count + _n - 1) >= *_cap)                                       \
    {                                                                    \
        *_cap = (*_cap + _n) * 2;                                        \
        ptr = realloc(ptr, *_cap * sizeof(*(ptr)));                      \
    }                                                                    \
} while(0)

XLIST_T *xlist_put_uninit(XList *ls)
{
    if(ls->nfb_count != 0)
    {
        ls->count++;
        
        XListBucket *bucket = ls->not_full_buckets[ls->nfb_count - 1];
        XLIST_T *elm = &bucket->elms[bucket->count];
        bucket->count++;
        
        bucket->elms[bucket->count] = XLIST_SENTINEL;
        
        if(bucket->count == bucket->cap)
        {
            bucket->not_full_index = -1;
            ls->nfb_count -= 1;
        }
        
        return elm;
    }
    
    XLIST_MAYBE_GROW(ls->not_full_buckets, &ls->nfb_cap, ls->nfb_count);
    XLIST_MAYBE_GROW(ls->allocations, &ls->al_cap, ls->al_count, 2);
    
    XListBucket *new_bucket = malloc(sizeof(XListBucket));
    
    ls->allocations[ls->al_count++] = new_bucket;
    
    memset(new_bucket, 0, sizeof(XListBucket));
    new_bucket->not_full_index = ls->nfb_count++;
    new_bucket->cap = ls->prev_cap * 2;
    ls->prev_cap *= 2;
    
    new_bucket->elms = malloc(sizeof(XLIST_T) * (new_bucket->cap + 1));
    XLIST_T *elm = &new_bucket->elms[0];
    
    new_bucket->elms[1] = XLIST_SENTINEL;
    
    new_bucket->count = 1;
    
    if(ls->count == 0)
    {
        ls->head = new_bucket;
    }
    else
    {
        ls->tail->next = new_bucket;
        new_bucket->prev = ls->tail;
    }
    ls->tail = new_bucket;
    ls->end_sentinel->prev = new_bucket;
    new_bucket->next = ls->end_sentinel;
    
    
    ls->count += 1;
    return elm;
    // TODO close bridges
}

XLIST_T *xlist_del(XList *ls, XLIST_T *elm)
{
    *elm = XLIST_SENTINEL;
    
    XLIST_T *end = elm;
    while(!XLIST_IS_SENTINEL(end))
    {
        end++;
    }
    
    XListBucket *bp = (XListBucket*) end->XLIST_PTR_FIELD;
    bp->count = elm - bp->elms;
    if(bp->count == 0)
    {
        // TODO delete the bucket
        // TODO and put it in some reserve
        
        return &bp->next->elms[0];
    }
    else
    {
        // this will be bp->next
        XListBucket *new_bucket = malloc(sizeof(XListBucket));
        
        XListBucket *old_next = bp->next;
        bp->next = new_bucket;
        new_bucket->next = old_next;
        new_bucket->prev = bp;
        old_next->prev = new_bucket;
        
        new_bucket->elms = elm + 1;
        new_bucket->cap =
        // TODO create new bucket and assign its beginning to elm+1, make elm sentinel with ptr value pointing to the new bucket
    }
    
}

void xlist_deinit(XList *ls)
{
    for(size_t i = 0 ; i < ls->al_count ; i++)
    {
        free(ls->allocations[i]);
    }
    free(ls->allocations);
    free(ls->not_full_buckets);
}








