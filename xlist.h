/*
XLIST_NAME stands for "Exploding Array",
when deleting, it splits the the hole into 2 arrays,
in order to maintain pointer/iterator stability of other elements.

when inserting, it looks into stack of non_full_buckets, if empty, alloc new bucket.

when deleting, split into 2 arrays, left one is full from the start, right one will keep the old cap + left_arr_size

but how will iterators be stable? and what about it_next()?

*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if !defined(XLIST_T) || !defined(XLIST_NAME) || !defined(XLIST_SENTINEL) || !defined(XLIST_IS_SENTINEL) || !defined(XLIST_PTR_FIELD)
    #error "Must define XLIST_T, XLIST_NAME, XLIST_SENTINEL, and XLIST_IS_SENTINEL"
#endif

#define XLIST_CAT_(a, b) a##b
#define XLIST_CAT(a, b) XLIST_CAT_(a,b)

#define XListBucket      XLIST_CAT(XLIST_NAME, Bucket_T)
#define XListIterator    XLIST_CAT(XLIST_NAME, Iterator_T)

#define xlist_init       XLIST_CAT(XLIST_NAME, _init)
#define xlist_put_uninit XLIST_CAT(XLIST_NAME, _put_uninit)
#define xlist_put        XLIST_CAT(XLIST_NAME, _put)
#define xlist_del        XLIST_CAT(XLIST_NAME, _del)
#define xlist_deinit     XLIST_CAT(XLIST_NAME, _deinit)
#define xlist_begin      XLIST_CAT(XLIST_NAME, _begin)
#define xlist_end        XLIST_CAT(XLIST_NAME, _end)

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

typedef struct XLIST_NAME
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
} XLIST_NAME;

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

#ifdef XLIST_IMPL

#define xlist_erase_not_full_bucket XLIST_CAT(XLIST_NAME, _erase_not_full_bucket)
#define xlist_push_not_full_bucket  XLIST_CAT(XLIST_NAME, _push_not_full_bucket)

#define XLIST_MAYBE_GROW(ptr, cap_ptr, count, ...)                   \
do                                                                   \
{                                                                    \
    size_t _n = 0 __VA_OPT__(+1) ? 0 __VA_OPT__(+(__VA_ARGS__)) : 1; \
    size_t *_cap = (cap_ptr);                                        \
    const size_t _count = (count);                                   \
    if((_count + _n - 1) >= *_cap)                                   \
    {                                                                \
        *_cap = (*_cap + _n) * 2;                                    \
        ptr = realloc(ptr, *_cap * sizeof(*(ptr)));                  \
    }                                                                \
} while(0)

void xlist_init(XLIST_NAME *ls)
{
    memset(ls, 0, sizeof(XLIST_NAME));
    
    
    ls->end_sentinel = malloc(sizeof(XListBucket));
    memset(ls->end_sentinel, 0, sizeof(XListBucket));
    
    ls->head = ls->end_sentinel;
    ls->tail = ls->end_sentinel;
    
    ls->nfb_cap = 16;
    ls->not_full_buckets = malloc(sizeof(XListBucket*) * ls->nfb_cap);
    
    ls->al_cap = 16;
    ls->allocations = malloc(sizeof(*ls->allocations) * ls->al_cap);
    
    ls->prev_cap = 64;
}

void xlist_erase_not_full_bucket(XLIST_NAME *ls, XListBucket *b)
{
    assert(b->not_full_index != (size_t)-1);
    
    size_t index = b->not_full_index;
    size_t last = ls->nfb_count - 1;
    
    ls->not_full_buckets[index] = ls->not_full_buckets[last];
    ls->not_full_buckets[index]->not_full_index = index;
    ls->nfb_count -= 1;
    b->not_full_index = (size_t)-1;
}

void xlist_push_not_full_bucket(XLIST_NAME *ls, XListBucket *b)
{
    assert(b->not_full_index == (size_t)-1);
    
    XLIST_MAYBE_GROW(ls->not_full_buckets, &ls->nfb_cap, ls->nfb_count);
    ls->not_full_buckets[ls->nfb_count] = b;
    b->not_full_index = ls->nfb_count;
    ls->nfb_count += 1;
}

XLIST_T *xlist_put_uninit(XLIST_NAME *ls)
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
            bucket->not_full_index = (size_t)-1;
            ls->nfb_count -= 1;
        }
        
        return elm;
    }
    
    XLIST_MAYBE_GROW(ls->allocations, &ls->al_cap, ls->al_count, 2);
    
    XListBucket *new_bucket = malloc(sizeof(XListBucket));
    ls->allocations[ls->al_count++] = new_bucket;
    
    memset(new_bucket, 0, sizeof(XListBucket));
    new_bucket->not_full_index = (size_t)-1;
    new_bucket->cap = ls->prev_cap * 2;
    ls->prev_cap *= 2;
    
    new_bucket->elms = malloc(sizeof(XLIST_T) * (new_bucket->cap + 1));
    ls->allocations[ls->al_count++] = new_bucket->elms;
    
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
    
    xlist_push_not_full_bucket(ls, new_bucket);
    ls->count += 1;
    return elm;
    // TODO close bridges
}

XLIST_T *xlist_del(XLIST_NAME *ls, XLIST_T *elm)
{
    *elm = XLIST_SENTINEL;
    
    XLIST_T *end = elm;
    while(!XLIST_IS_SENTINEL(end))
    {
        end++;
    }
    
    XListBucket *bp = (XListBucket*) end->XLIST_PTR_FIELD;
    size_t deleted_index = elm - bp->elms;
    if(deleted_index == 0)
    {
        // just shift the elms by 1
        XLIST_T *ret = &bp->elms[1];
        bp->elms += 1;
        bp->count -= 1;
        bp->cap -= 1;
        return ret;
        
        // TODO we need some mechanism to reuse the deleted slot...
        // maybe the planned bridge mechanism can also handle this
        // case by having NULL ptr for prev bucket.
        // Another problem is bridges next to each other...
        
        // hmmm maybe we shouldn't shift the elms like that, instead
        // create a new bucket with size and cap 0, don't link it with the others,
        // just store it for the bridge mechanism to work properly
        
        // TODO also handle if count is now 0,
        // you don't want it linked to the other buckets if its empty
    }
    else if(deleted_index == bp->count - 1)
    {
        // TODO same thing
        // we shouldnt do it
        // instead should split into new bucket to the right, with cap and count 0, but unlink it from this bucket
        // will only be used as a bridge in case a new element will be inserted, so the slot can be reused
        // remember, merging two nodes makes the total cap = cap1+cap2+1 because we need one less sentinel
        bp->count -= 1;
        return bp->next->elms;
    }
    else
    {
        // split into new bucket
        
        size_t old_count = bp->count;
        size_t old_cap = bp->cap;
        
        bp->count = deleted_index;
        bp->cap = bp->count;
        
        if(bp->not_full_index != (size_t)-1)
        {
            xlist_erase_not_full_bucket(ls, bp);
        }
        
        // this will be bp->next
        XListBucket *new_bucket = malloc(sizeof(XListBucket));
        
        XListBucket *old_next = bp->next;
        bp->next = new_bucket;
        new_bucket->next = old_next;
        new_bucket->prev = bp;
        old_next->prev = new_bucket;
        
        new_bucket->elms = elm + 1;
        
        // let's say old_cap is 8, deleted index is 2
        // that means new_bucket will be starting at 3 through 8, so 0->5 so cap=5
        // let's say old_count is 6
        // new count is 2
        // new cap is 2
        // new_bucket count is 3
        new_bucket->cap   = old_cap   - deleted_index - 1;
        new_bucket->count = old_count - deleted_index - 1;
        if(new_bucket->cap > new_bucket->count)
        {
            xlist_push_not_full_bucket(ls, new_bucket);
        }
        
        return new_bucket->elms;
    }
}

void xlist_deinit(XLIST_NAME *ls)
{
    for(size_t i = 0 ; i < ls->al_count ; i++)
    {
        free(ls->allocations[i]);
    }
    free(ls->end_sentinel);
    free(ls->allocations);
    free(ls->not_full_buckets);
}

XListIterator xlist_begin(XLIST_NAME *ls)
{
    return (XListIterator){ls->head->elms};
}

XListIterator xlist_end(XLIST_NAME *ls)
{
    return (XListIterator){ls->end_sentinel->elms};
}

#endif



