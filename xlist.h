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

#if !defined(XLIST_T) || !defined(XLIST_NAME) || !defined(XLIST_MAKE_SENTINEL) || !defined(XLIST_IS_SENTINEL) || !defined(XLIST_SENTINEL_GET_PTR) || !defined(XLIST_SENTINEL_SET_PTR)
    #error "Must define XLIST_T, XLIST_NAME, XLIST_MAKE_SENTINEL, XLIST_IS_SENTINEL, XLIST_SENTINEL_GET_PTR, and XLIST_SENTINEL_SET_PTR"
#endif

#define XLIST_CAT_(a, b) a##b
#define XLIST_CAT(a, b) XLIST_CAT_(a,b)

#define xlist_bucket_t   XLIST_CAT(XLIST_NAME, _bucket_t)
#define xlist_iter_t     XLIST_CAT(XLIST_NAME, _iter_t)

#define xlist_init       XLIST_CAT(XLIST_NAME, _init)
#define xlist_put_ptr XLIST_CAT(XLIST_NAME, _put_ptr)
#define xlist_put        XLIST_CAT(XLIST_NAME, _put)
#define xlist_del        XLIST_CAT(XLIST_NAME, _del)
#define xlist_deinit     XLIST_CAT(XLIST_NAME, _deinit)
#define xlist_begin      XLIST_CAT(XLIST_NAME, _begin)
#define xlist_end        XLIST_CAT(XLIST_NAME, _end)
#define xlist_iter_next  XLIST_CAT(XLIST_NAME, _iter_next)

typedef struct xlist_bucket_t
{
    // maybe we do add prev ptr here. not for linked list traversal, but to say they are actually connected in memory, so if this bucket is about to be deleted, the prev one can just extend its cap. the prev field can be NULL
    // issue is, if prev is already deleted....
    // this idea is a wash, just have a bucket reserve so we can reuse its buffers
    
    // TODO we also need bridge_prev and bridge_next for buckets that link by buffer (may be NULL)
    
    struct xlist_bucket_t *bridge_prev;
    struct xlist_bucket_t *bridge_next;
    
    struct xlist_bucket_t *next;
    struct xlist_bucket_t *prev;
    size_t not_full_index;
    XLIST_T *elms;
    size_t count;
    size_t cap;
} xlist_bucket_t;

typedef struct XLIST_NAME
{
    struct
    {
        xlist_bucket_t **array;
        size_t count;
        size_t cap;
    } not_full_buckets;
    
    struct
    {
        xlist_bucket_t **array;
        size_t cap;
        size_t count;
    } buckets_with_prev_bridges; // only store prevs, that way we don't get the same hole repeated as a next bridge and a prev bridge
    
    struct
    {
        void **array;
        size_t count;
        size_t cap;
    } allocations;
    
    xlist_bucket_t *tail;
    xlist_bucket_t *head;
    xlist_bucket_t *end_sentinel;
    size_t bucket_count;
    
    size_t prev_cap;
    size_t count;
} XLIST_NAME;

typedef struct xlist_iter_t
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
} xlist_iter_t;

#ifdef XLIST_IMPL

#define xlist_erase_not_full_bucket XLIST_CAT(XLIST_NAME, _erase_not_full_bucket)
#define xlist_push_not_full_bucket  XLIST_CAT(XLIST_NAME, _push_not_full_bucket)
#define xlist_assign_sentinel       XLIST_CAT(XLIST_NAME, _assign_sentinel)
#define xlist_unlink_bucket         XLIST_CAT(XLIST_NAME, _unlink_bucket)

#define XLIST_MAYBE_GROW(s, ...)                                        \
do                                                                      \
{                                                                       \
    const size_t _n = (__VA_ARGS__ +0) ? (__VA_ARGS__ +0) : 1;          \
    if(((s).count + _n - 1) >= (s).cap)                                 \
    {                                                                   \
        (s).cap = ((s).cap + (_n - 1)) * 2;                             \
        (s).array = realloc((s).array, (s).cap * sizeof(*((s).array))); \
    }                                                                   \
} while(0)

#define XLIST_POP(s) \
((s).array[--(s).count])

#define XLIST_PUSH(s, x) \
((s).array[(s).count++] = (x))

void xlist_assign_sentinel(XLIST_T *elm, xlist_bucket_t *bucket)
{
    XLIST_MAKE_SENTINEL((elm));
    XLIST_SENTINEL_SET_PTR(elm, bucket);
}

void xlist_init(XLIST_NAME *ls)
{
    memset(ls, 0, sizeof(XLIST_NAME));
    
    
    ls->end_sentinel = malloc(sizeof(xlist_bucket_t));
    memset(ls->end_sentinel, 0, sizeof(xlist_bucket_t));
    
    ls->head = ls->end_sentinel;
    ls->tail = ls->end_sentinel;
    
    ls->not_full_buckets.cap = 16;
    ls->not_full_buckets.array = malloc(sizeof(xlist_bucket_t*) * ls->not_full_buckets.cap);
    
    ls->allocations.cap = 16;
    ls->allocations.array = malloc(sizeof(void*) * ls->allocations.cap);
    
    ls->buckets_with_prev_bridges.cap = 16;
    ls->buckets_with_prev_bridges.array = malloc(sizeof(xlist_bucket_t*) * ls->buckets_with_prev_bridges.cap);
    
    ls->prev_cap = 64;
}

void xlist_erase_not_full_bucket(XLIST_NAME *ls, xlist_bucket_t *b)
{
    assert(b->not_full_index != (size_t)-1);
    
    size_t index = b->not_full_index;
    size_t last = ls->not_full_buckets.count - 1;
    
    ls->not_full_buckets.array[index] = ls->not_full_buckets.array[last];
    ls->not_full_buckets.array[index]->not_full_index = index;
    ls->not_full_buckets.count -= 1;
    b->not_full_index = (size_t)-1;
}

void xlist_push_not_full_bucket(XLIST_NAME *ls, xlist_bucket_t *b)
{
    assert(b->not_full_index == (size_t)-1);
    
    XLIST_MAYBE_GROW(ls->not_full_buckets);
    XLIST_PUSH(ls->not_full_buckets, b);
    
    b->not_full_index = ls->not_full_buckets.count - 1;
}

void xlist_unlink_bucket(XLIST_NAME *ls, xlist_bucket_t *b)
{
    if(b == ls->head)
    {
        ls->head = ls->head->next;
        ls->head->prev = NULL;
        if(b == ls->tail)
        {
            ls->tail = ls->end_sentinel;
        }
    }
    else if(b == ls->tail)
    {
        ls->tail = ls->tail->prev;
        ls->tail->next = ls->end_sentinel;
        ls->end_sentinel->prev = ls->tail;
    }
    else if(b->next != NULL)
    {
        b->prev->next = b->next;
        b->next->prev = b->prev;
    }
    
    b->next = b->prev = NULL;
}

// this function shouldn't exist for this data structure
// because we're dealing with sentinels, user must initailize on put
XLIST_T *xlist_put_ptr(XLIST_NAME *ls, const XLIST_T *new_elm)
{
    if(ls->not_full_buckets.count != 0)
    {
        ls->count++;
        
        xlist_bucket_t *bucket = ls->not_full_buckets.array[ls->not_full_buckets.count - 1];
        XLIST_T *ret = &bucket->elms[bucket->count];
        bucket->count++;
        
        xlist_assign_sentinel(&bucket->elms[bucket->count], bucket);
        
        if(bucket->count == bucket->cap)
        {
            bucket->not_full_index = (size_t)-1;
            ls->not_full_buckets.count -= 1;
        }
        
        memcpy(ret, new_elm, sizeof(XLIST_T));
        return ret;
    }
    if(ls->buckets_with_prev_bridges.count != 0)
    {
        xlist_bucket_t *bucket = XLIST_POP(ls->buckets_with_prev_bridges);
        
        xlist_bucket_t *prev = bucket->bridge_prev;
        
        // they may be empty and unlinked, or full
        assert(prev != NULL);
        assert(prev->count == prev->cap || prev->count == 0);
        assert(bucket->count == bucket->cap || bucket->count == 0);
        assert((prev->elms + prev->cap + 1) == bucket->elms);
        
        prev->cap = prev->cap + bucket->cap + 1;
        XLIST_T *ret = &prev->elms[prev->count];
        
        prev->count += 1 + bucket->count;
        
        // prev->elms = bucket->elms;
        XLIST_SENTINEL_SET_PTR(&prev->elms[prev->count], prev);
        
        prev->bridge_next = bucket->bridge_next;
        if(prev->bridge_next != NULL)
        {
            prev->bridge_next->bridge_prev = prev;
        }
        
        if(prev->next == NULL)
        {
            if(bucket->next != NULL) // reuse bucket's links
            {
                prev->next = bucket->next;
                prev->prev = bucket->prev;
            }
            else
            {
                if(ls->tail == ls->end_sentinel)
                {
                    ls->head = ls->tail = prev;
                    ls->tail->next = ls->end_sentinel;
                    ls->end_sentinel->prev = ls->tail;
                }
                else
                {
                    ls->tail->next = prev;
                    prev->prev = ls->tail;
                    ls->tail = prev;
                    ls->tail->next = ls->end_sentinel;
                    ls->end_sentinel->prev = ls->tail;
                }
            }
        }
        
        if(prev->count < prev->cap)
        {
            xlist_push_not_full_bucket(ls, prev);
        }
        
        xlist_unlink_bucket(ls, bucket);
        
        memcpy(ret, new_elm, sizeof(XLIST_T));
        return ret;
    }
    
    XLIST_MAYBE_GROW(ls->allocations, 2);
    
    xlist_bucket_t *new_bucket = malloc(sizeof(xlist_bucket_t));
    XLIST_PUSH(ls->allocations, new_bucket);
    
    memset(new_bucket, 0, sizeof(xlist_bucket_t));
    new_bucket->not_full_index = (size_t)-1;
    new_bucket->cap = ls->prev_cap * 2;
    ls->prev_cap *= 2;
    
    new_bucket->elms = malloc(sizeof(XLIST_T) * (new_bucket->cap + 1));
    XLIST_PUSH(ls->allocations, new_bucket->elms);
    
    XLIST_T *ret = &new_bucket->elms[0];
    
    xlist_assign_sentinel(&new_bucket->elms[1], new_bucket);
    
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
    
    memcpy(ret, new_elm, sizeof(XLIST_T));
    return ret;
}

XLIST_T *xlist_put(XLIST_NAME *ls, XLIST_T elm)
{
    XLIST_T *ptr = xlist_put_ptr(ls, &elm);
    *ptr = elm;
    return ptr;
}

XLIST_T *xlist_del(XLIST_NAME *ls, XLIST_T *elm)
{
    XLIST_T *end = elm;
    while(!XLIST_IS_SENTINEL(end))
    {
        end++;
    }
    
    xlist_bucket_t *bp = (xlist_bucket_t*) XLIST_SENTINEL_GET_PTR(end);
    size_t deleted_index = elm - bp->elms;
    
    xlist_bucket_t *new_bucket = malloc(sizeof(xlist_bucket_t));
    memset(new_bucket, 0, sizeof(xlist_bucket_t));
    new_bucket->not_full_index = (size_t)-1;
    
    XLIST_MAYBE_GROW(ls->allocations);
    XLIST_PUSH(ls->allocations, new_bucket);
    
    if(deleted_index == 0)
    {
        // current bucket has 0 elms, unlink it from the chain, but keep its as a bridge_prev for the new node
        
        new_bucket->elms = bp->elms + 1;
        new_bucket->count = bp->count - 1;
        new_bucket->cap = bp->cap - 1;
        
        bp->count = 0;
        bp->cap = 0;
        
        if(bp->not_full_index != (size_t)-1)
        {
            new_bucket->not_full_index = bp->not_full_index;
            ls->not_full_buckets.array[new_bucket->not_full_index] = new_bucket;
            bp->not_full_index = (size_t)-1;
        }
        
        new_bucket->bridge_prev = bp;
        new_bucket->bridge_next = bp->bridge_next;
        bp->bridge_next = new_bucket;
        
        if(new_bucket->bridge_next != NULL)
        {
            new_bucket->bridge_next->bridge_prev = new_bucket;
        }
        
        XLIST_T *ret = NULL;
        if(new_bucket->count != 0)
        {
            new_bucket->next = bp->next;
            new_bucket->prev = bp->prev;
            
            new_bucket->next->prev = new_bucket; // always exists
            
            if(new_bucket->prev != NULL)
            {
                new_bucket->prev->next = new_bucket;
            }
            
            ret = new_bucket->elms;
        }
        else
        {
            ret = bp->next->elms;
        }
        
        xlist_unlink_bucket(ls, bp);
        
        XLIST_MAYBE_GROW(ls->buckets_with_prev_bridges);
        XLIST_PUSH(ls->buckets_with_prev_bridges, new_bucket);
        
        assert(
            XLIST_SENTINEL_GET_PTR((&new_bucket->elms[new_bucket->count])) == bp
        );
        
        xlist_assign_sentinel(elm, bp);
        xlist_assign_sentinel(&new_bucket->elms[new_bucket->count], new_bucket);
        
        return ret;
    }
    else if(deleted_index == bp->count - 1)
    {
        new_bucket->elms = bp->elms + bp->count;
        
        new_bucket->count = 0;
        new_bucket->cap = bp->cap - deleted_index - 1;
        
        bp->count -= 1;
        bp->cap = bp->count;
        
        new_bucket->bridge_prev = bp;
        new_bucket->bridge_next = bp->bridge_next;
        
        if(new_bucket->bridge_next != NULL)
        {
            new_bucket->bridge_next->bridge_prev = new_bucket;
        }
        
        // if(new_bucket->cap > 0)
        // {
        //     if(bp->not_full_index != (size_t)-1)
        //     {
        //         new_bucket->not_full_index = bp->not_full_index;
        //         ls->not_full_buckets.array[new_bucket->not_full_index] = new_bucket;
        //     }
        //     else
        //     {
        //         XLIST_MAYBE_GROW(ls->not_full_buckets);
        //         xlist_push_not_full_bucket(ls, new_bucket);
        //     }
        // }
        
        if(bp->not_full_index != (size_t)-1)
            xlist_erase_not_full_bucket(ls, bp);
        
        bp->bridge_next = new_bucket;
        
        XLIST_MAYBE_GROW(ls->buckets_with_prev_bridges);
        XLIST_PUSH(ls->buckets_with_prev_bridges, new_bucket);
        
        xlist_assign_sentinel(elm, bp);
        xlist_assign_sentinel(&new_bucket->elms[0], new_bucket);
        
        return bp->next->elms;
    }
    else
    {
        // let's say old_cap is 8, deleted index is 2
        // that means new_bucket will be starting at 3 through 8, so 0->5 so cap=5
        // let's say old_count is 6
        // new count is 2
        // new cap is 2
        // new_bucket count is 3
        new_bucket->count = bp->count - deleted_index - 1;
        new_bucket->cap   = bp->cap   - deleted_index - 1;
        
        bp->count = deleted_index;
        bp->cap = bp->count;
        
        new_bucket->next = bp->next;
        new_bucket->prev = bp;
        new_bucket->next->prev = new_bucket;
        new_bucket->prev->next = new_bucket;
        
        new_bucket->bridge_next = bp->bridge_next;
        bp->bridge_next = new_bucket;
        new_bucket->bridge_prev = bp;
        
        if(new_bucket->bridge_next != NULL)
        {
            new_bucket->bridge_next->bridge_prev = new_bucket;
        }
        
        new_bucket->elms = bp->elms + bp->count + 1;
        
        // TODO this piece of code is repeated 2 times so far, refactor
        // lots of common code between these 3 branches
        if(bp->not_full_index != (size_t)-1)
        {
            new_bucket->not_full_index = bp->not_full_index;
            ls->not_full_buckets.array[new_bucket->not_full_index] = new_bucket;
            bp->not_full_index = (size_t)-1;
        }
        
        XLIST_MAYBE_GROW(ls->buckets_with_prev_bridges);
        XLIST_PUSH(ls->buckets_with_prev_bridges, new_bucket);
        
        xlist_assign_sentinel(elm, bp);
        xlist_assign_sentinel(&new_bucket->elms[new_bucket->count], new_bucket);
        
        return new_bucket->elms;
    }
}

void xlist_deinit(XLIST_NAME *ls)
{
    for(size_t i = 0 ; i < ls->allocations.count ; i++)
    {
        free(ls->allocations.array[i]);
    }
    free(ls->allocations.array);
    free(ls->end_sentinel);
    free(ls->not_full_buckets.array);
    free(ls->buckets_with_prev_bridges.array);
}

xlist_iter_t xlist_begin(XLIST_NAME *ls)
{
    return (xlist_iter_t){ls->head->elms};
}

xlist_iter_t xlist_end(XLIST_NAME *ls)
{
    return (xlist_iter_t){ls->end_sentinel->elms};
}

xlist_iter_t xlist_iter_next(xlist_iter_t it)
{
    it.ptr += 1;
    if(XLIST_IS_SENTINEL(it.ptr))
    {
        xlist_bucket_t *bucket = (xlist_bucket_t*) XLIST_SENTINEL_GET_PTR(it.ptr);
        return (xlist_iter_t){.ptr = bucket->next->elms};
    }
    return it;
}

#endif

#undef XLIST_CAT_
#undef XLIST_CAT

#undef xlist_bucket_t
#undef xlist_iter_t

#undef xlist_init
#undef xlist_put_ptr
#undef xlist_put
#undef xlist_del
#undef xlist_deinit
#undef xlist_begin
#undef xlist_end
#undef xlist_iter_next

#undef xlist_erase_not_full_bucket
#undef xlist_push_not_full_bucket 
#undef xlist_assign_sentinel
