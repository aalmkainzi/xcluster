#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/*
new idea:
optimization to make the next ptr stored in elms[count] actually just point to the next node's elms directly
but then we can't erase because we need to access the actual node.
maybe just iterate through the container comparing the elm ptr with `>= begin` and `< end` of each node to find its owning node

OR. create secondary array for nodes, in which they store pointers:
ptrs[count] == node->next->elms
ptrs[count-1] == node

i kind of prefer approach 1, it's simpler and doesnt require secondary array.
but then again, if secondary array is gonna be used anyway (user didnt provide XCLUSTER_SENTINEL_GET_PTR and XCLUSTER_SENTINEL_SET_PTR),
then might as well do approach 2

ALSO. think of an unstable_handle/iterator type that can enable fast erasure

*/

#if !defined(XCLUSTER_T) || !defined(XCLUSTER_NAME) || !defined(XCLUSTER_MAKE_SENTINEL) || !defined(XCLUSTER_IS_SENTINEL) || !defined(XCLUSTER_SENTINEL_GET_PTR) || !defined(XCLUSTER_SENTINEL_SET_PTR)
    #error "Must define XCLUSTER_T, XCLUSTER_NAME, XCLUSTER_MAKE_SENTINEL, XCLUSTER_IS_SENTINEL, XCLUSTER_SENTINEL_GET_PTR, and XCLUSTER_SENTINEL_SET_PTR"
#endif

#if defined(XCLUSTER_DEBUG)
    #define xcluster_assert assert
#endif

#define XCLUSTER_CAT_(a, _node) a##_node
#define XCLUSTER_CAT(a, _node)  XCLUSTER_CAT_(a,_node)

#define xcluster_node_t     XCLUSTER_CAT(XCLUSTER_NAME, _node_t)

#define xcluster_init       XCLUSTER_CAT(XCLUSTER_NAME, _init)
#define xcluster_put_ptr    XCLUSTER_CAT(XCLUSTER_NAME, _put_ptr)
#define xcluster_put        XCLUSTER_CAT(XCLUSTER_NAME, _put)
#define xcluster_del        XCLUSTER_CAT(XCLUSTER_NAME, _del)
#define xcluster_deinit     XCLUSTER_CAT(XCLUSTER_NAME, _deinit)
#define xcluster_clone      XCLUSTER_CAT(XCLUSTER_NAME, _clone)
#define xcluster_begin      XCLUSTER_CAT(XCLUSTER_NAME, _begin)
#define xcluster_end        XCLUSTER_CAT(XCLUSTER_NAME, _end)
#define xcluster_next       XCLUSTER_CAT(XCLUSTER_NAME, _next)

typedef struct XCLUSTER_NAME
{
    struct
    {
        struct xcluster_node_t **array;
        size_t count;
        size_t cap;
    } not_full_nodes;
    
    struct
    {
        struct xcluster_node_t **array;
        size_t cap;
        size_t count;
    } nodes_with_prev_bridges; // only store prevs, that way we don't get the same hole repeated as a next bridge and a prev bridge. Another approach is, store the hole itself, not the bucket that has it (and always make elms[cap] a sentinel that points to the owning bucket)
    
    struct
    {
        void **array;
        size_t count;
        size_t cap;
    } allocations;
    
    struct xcluster_node_t *tail;
    struct xcluster_node_t *head;
    struct xcluster_node_t *end_sentinel;
    
    struct xcluster_node_t *node_reserve;
    
    size_t prev_cap;
    size_t count;
} XCLUSTER_NAME;

void xcluster_init(XCLUSTER_NAME *_xc);
XCLUSTER_T *xcluster_put_ptr(XCLUSTER_NAME *_xc, const XCLUSTER_T *new_elm);
XCLUSTER_T *xcluster_put(XCLUSTER_NAME *_xc, XCLUSTER_T elm);
XCLUSTER_T *xcluster_del(XCLUSTER_NAME *_xc, XCLUSTER_T *elm);
void xcluster_deinit(XCLUSTER_NAME *_xc);
XCLUSTER_NAME xcluster_clone(XCLUSTER_NAME *_xc);
XCLUSTER_T *xcluster_begin(XCLUSTER_NAME *_xc);
XCLUSTER_T *xcluster_end(XCLUSTER_NAME *_xc);
XCLUSTER_T *xcluster_next(XCLUSTER_T *it);

#ifdef XCLUSTER_IMPL

typedef struct xcluster_node_t
{
    struct xcluster_node_t *bridge_prev;
    struct xcluster_node_t *bridge_next;
    
    struct xcluster_node_t *next;
    struct xcluster_node_t *prev;
    
    size_t not_full_index;
    
    XCLUSTER_T *elms;
    size_t count;
    size_t cap;
} xcluster_node_t;

// TODO const iterator. it will be invalidated by inserts/deletes
// but is faster to iterate, since we wont check for sentinel every iteration,
// instead we know each node's count

// TODO maybe some kind of SOA thing
// where each node stores array for EACH type
// actually, the user type doesn't *need* a pointer field
// if the pointer field macros are not defined,
// we can just store a secondary buffer for pointers only
// it wont slow down iteration speed
// because we'll only use that buffer when we reach a node's end

// TODO An approach to avoid needing pointer from sentinel, and to avoid xcluster_del being slow:
// store a second array of pointers to the owning bucket, kinda crazy, will use lots of memory
// but iteration speed shouldn't be affected I think.

#ifdef XCLUSTER_DEBUG
    #define xcluster_validate XCLUSTER_CAT(XCLUSTER_NAME, _validate)
    bool xcluster_validate(XCLUSTER_NAME *_xc);
#endif

#define xcluster_erase_not_full_node       XCLUSTER_CAT(XCLUSTER_NAME, _erase_not_full_node)
#define xcluster_push_not_full_node        XCLUSTER_CAT(XCLUSTER_NAME, _push_not_full_node)
#define xcluster_assign_sentinel           XCLUSTER_CAT(XCLUSTER_NAME, _assign_sentinel)
#define xcluster_unlink_node               XCLUSTER_CAT(XCLUSTER_NAME, _unlink_node)
#define xcluster_steal_node_links          XCLUSTER_CAT(XCLUSTER_NAME, _steal_node_links)
#define xcluster_alloc_node                XCLUSTER_CAT(XCLUSTER_NAME, _alloc_node)
#define xcluster_steal_node_not_full_index XCLUSTER_CAT(XCLUSTER_NAME, _steal_node_not_full_index)
#define xcluster_bridges_with_prev_index   XCLUSTER_CAT(XCLUSTER_NAME, _bridges_with_prev_index)
#define xcluster_link_node                 XCLUSTER_CAT(XCLUSTER_NAME, _link_node)

xcluster_node_t *xcluster_alloc_node(XCLUSTER_NAME *_xc);

#if defined(__cplusplus) && defined(_MSC_VER)
    #define XCLUSTER_TYPEOF decltype
#else
    #define XCLUSTER_TYPEOF __typeof__
#endif

#define XCLUSTER_MAYBE_GROW(s, ...)                                     \
do                                                                      \
{                                                                       \
    const size_t _n = (__VA_ARGS__ +0) ? (__VA_ARGS__ +0) : 1;          \
    if(((s).count + _n - 1) >= (s).cap)                                 \
    {                                                                   \
        (s).cap = ((s).cap + (_n - 1)) * 2;                             \
        (s).array = (XCLUSTER_TYPEOF((s).array))                        \
                    realloc((s).array, (s).cap * sizeof(*((s).array))); \
    }                                                                   \
} while(0)

#define XCLUSTER_POP(s) \
((s).array[--(s).count])

#define XCLUSTER_PUSH(s, x) \
((s).array[(s).count++] = (x))

void xcluster_assign_sentinel(XCLUSTER_T *elm, xcluster_node_t *node)
{
    XCLUSTER_MAKE_SENTINEL((elm));
    XCLUSTER_SENTINEL_SET_PTR((elm), (node->next->elms));
}

void xcluster_init(XCLUSTER_NAME *_xc)
{
    memset(_xc, 0, sizeof(XCLUSTER_NAME));
    
    
    _xc->end_sentinel = (xcluster_node_t*) malloc(sizeof(xcluster_node_t));
    memset(_xc->end_sentinel, 0, sizeof(xcluster_node_t));
    
    _xc->head = _xc->end_sentinel;
    _xc->tail = _xc->end_sentinel;
    
    _xc->not_full_nodes.cap = 16;
    _xc->not_full_nodes.array = (xcluster_node_t**) malloc(sizeof(xcluster_node_t*) * _xc->not_full_nodes.cap);
    
    _xc->allocations.cap = 16;
    _xc->allocations.array = (void**) malloc(sizeof(void*) * _xc->allocations.cap);
    
    _xc->nodes_with_prev_bridges.cap = 16;
    _xc->nodes_with_prev_bridges.array = (xcluster_node_t**) malloc(sizeof(xcluster_node_t*) * _xc->nodes_with_prev_bridges.cap);
    
    _xc->prev_cap = 2048;
}

void xcluster_assign_next(xcluster_node_t *_node, xcluster_node_t *_new_next);

XCLUSTER_NAME xcluster_clone(XCLUSTER_NAME *_xc)
{
    XCLUSTER_NAME ret;
    xcluster_init(&ret);
    
    ret.head = ret.tail = xcluster_alloc_node(&ret);
    xcluster_assign_next(ret.tail, ret.end_sentinel);
    ret.end_sentinel->prev = ret.tail;
    
    ret.head->elms = (XCLUSTER_T*) malloc((_xc->count + 1) * sizeof(XCLUSTER_T));
    XCLUSTER_MAYBE_GROW(ret.allocations);
    XCLUSTER_PUSH(ret.allocations, ret.head->elms);
    
    xcluster_assign_sentinel(&ret.head->elms[_xc->count], ret.head);
    
    ret.head->count = _xc->count;
    ret.head->cap = _xc->count;
    ret.count = _xc->count;
    
    xcluster_node_t *node_it = _xc->head;
    size_t accum = 0;
    while(node_it != _xc->end_sentinel)
    {
        memcpy(ret.head->elms + accum, node_it->elms, node_it->count * sizeof(XCLUSTER_T));
        accum += node_it->count;
        node_it = node_it->next;
    }
    
    return ret;
}

void xcluster_erase_not_full_node(XCLUSTER_NAME *_xc, xcluster_node_t *_node)
{
    xcluster_assert(_node->not_full_index != (size_t)-1);
    
    size_t index = _node->not_full_index;
    size_t last = _xc->not_full_nodes.count - 1;
    
    _xc->not_full_nodes.array[index] = _xc->not_full_nodes.array[last];
    _xc->not_full_nodes.array[index]->not_full_index = index;
    _xc->not_full_nodes.count -= 1;
    _node->not_full_index = (size_t)-1;
}

void xcluster_push_not_full_node(XCLUSTER_NAME *_xc, xcluster_node_t *_node)
{
    xcluster_assert(_node->not_full_index == (size_t)-1);
    
    XCLUSTER_MAYBE_GROW(_xc->not_full_nodes);
    XCLUSTER_PUSH(_xc->not_full_nodes, _node);
    
    _node->not_full_index = _xc->not_full_nodes.count - 1;
}

void xcluster_assign_next(xcluster_node_t *_node, xcluster_node_t *_new_next)
{
    _node->next = _new_next;
    XCLUSTER_SENTINEL_SET_PTR((&_node->elms[_node->count]), _new_next->elms);
}

void xcluster_unlink_node(XCLUSTER_NAME *_xc, xcluster_node_t *_node)
{
    if(_node == _xc->head)
    {
        _xc->head = _xc->head->next;
        _xc->head->prev = NULL;
        if(_node == _xc->tail)
        {
            _xc->tail = _xc->end_sentinel;
        }
    }
    else if(_node == _xc->tail)
    {
        _xc->tail = _xc->tail->prev;
        xcluster_assign_next(_xc->tail, _xc->end_sentinel);
        _xc->end_sentinel->prev = _xc->tail;
    }
    else
    {
        xcluster_assign_next(_node->prev, _node->next);
        _node->next->prev = _node->prev;
    }
    
    _node->next = _node->prev = NULL;
}

void xcluster_steal_node_links(XCLUSTER_NAME *_xc, xcluster_node_t *_new_node, xcluster_node_t *_old_node)
{
    xcluster_assign_next(_new_node, _old_node->next);
    
    _new_node->prev = _old_node->prev;
    
    _new_node->next->prev = _new_node;
    
    if(_new_node->prev != NULL)
    {
        xcluster_assign_next(_new_node->prev, _new_node);
    }
    
    if(_old_node == _xc->head)
    {
        _xc->head = _new_node;
    }
    if(_old_node == _xc->tail)
    {
        _xc->tail = _new_node;
    }
    
    _old_node->next = _old_node->prev = NULL;
}

void xcluster_link_node(XCLUSTER_NAME *_xc, xcluster_node_t *_node)
{
    if(_xc->tail == _xc->end_sentinel) // empty
    {
        _xc->head = _xc->tail = _node;
        xcluster_assign_next(_node, _xc->end_sentinel);
        _xc->end_sentinel->prev = _node;
    }
    else
    {
        xcluster_assign_next(_xc->tail, _node);
        _node->prev = _xc->tail;
        _xc->tail = _node;
        xcluster_assign_next(_node, _xc->end_sentinel);
        _xc->end_sentinel->prev = _node;
    }
}

XCLUSTER_T *xcluster_put_ptr(XCLUSTER_NAME *_xc, const XCLUSTER_T *_new_elm)
{
    if(_xc->not_full_nodes.count != 0)
    {
        _xc->count++;
        
        xcluster_node_t *node = _xc->not_full_nodes.array[_xc->not_full_nodes.count - 1];
        
        if(node->count == 0)
        {
            xcluster_assert(node->next == NULL);
            xcluster_assert(node->prev == NULL);
            xcluster_link_node(_xc, node);
        }
        
        XCLUSTER_T *ret = &node->elms[node->count];
        node->count++;
        
        xcluster_assign_sentinel(&node->elms[node->count], node);
        
        if(node->count == node->cap)
        {
            node->not_full_index = (size_t)-1;
            _xc->not_full_nodes.count -= 1;
        }
        
        memcpy(ret, _new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
        xcluster_validate(_xc);
#endif
        return ret;
    }
    if(_xc->nodes_with_prev_bridges.count != 0)
    {
        _xc->count++;
        
        xcluster_node_t *node = XCLUSTER_POP(_xc->nodes_with_prev_bridges);
        
        xcluster_node_t *prev = node->bridge_prev;
        
        // they may be empty and unlinked, or full
        xcluster_assert(prev != NULL);
        xcluster_assert(prev->count == prev->cap || prev->count == 0);
        xcluster_assert(node->count == node->cap || node->count == 0);
        xcluster_assert((prev->elms + prev->cap + 1) == node->elms);
        
        size_t prevoldcap = prev->cap;
        size_t prevoldcount = prev->count;
        
        prev->cap = prev->cap + node->cap + 1;
        XCLUSTER_T *ret = &prev->elms[prev->count];
        
        prev->count += 1 + node->count;
        
        prev->bridge_next = node->bridge_next;
        if(prev->bridge_next != NULL)
        {
            prev->bridge_next->bridge_prev = prev;
        }
        
        if(prev->next == NULL)
        {
            if(node->next != NULL) // reuse node's links
            {
                xcluster_steal_node_links(_xc, prev, node);
            }
            else
            {
                xcluster_link_node(_xc, prev);
            }
        }
        else if(node->next != NULL)
        {
            xcluster_unlink_node(_xc, node);
        }
        else
        {
            XCLUSTER_SENTINEL_SET_PTR((&prev->elms[prev->count]), (prev->next->elms));
        }
        
        if(prev->count < prev->cap)
        {
            xcluster_push_not_full_node(_xc, prev);
        }
        
        memcpy(ret, _new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
        xcluster_validate(_xc);
#endif
        return ret;
    }
    
    xcluster_node_t *new_node = xcluster_alloc_node(_xc);
    // XCLUSTER_PUSH(_xc->allocations, new_node);
    
    memset(new_node, 0, sizeof(xcluster_node_t));
    new_node->not_full_index = (size_t)-1;
    new_node->cap = _xc->prev_cap * 2;
    _xc->prev_cap *= 2;
    
    new_node->elms = (XCLUSTER_T*) malloc(sizeof(XCLUSTER_T) * (new_node->cap + 1));
    
    XCLUSTER_MAYBE_GROW(_xc->allocations);
    XCLUSTER_PUSH(_xc->allocations, new_node->elms);
    
    XCLUSTER_T *ret = &new_node->elms[0];
    
    new_node->count = 1;
    
    if(_xc->count == 0)
    {
        _xc->head = new_node;
    }
    else
    {
        xcluster_assign_next(_xc->tail, new_node);
        new_node->prev = _xc->tail;
    }
    _xc->tail = new_node;
    _xc->end_sentinel->prev = new_node;
    
    xcluster_assign_next(new_node, _xc->end_sentinel);
    
    xcluster_push_not_full_node(_xc, new_node);
    _xc->count += 1;
    
    xcluster_assign_sentinel(&new_node->elms[1], new_node);
    
    memcpy(ret, _new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
    xcluster_validate(_xc);
#endif
    return ret;
}

XCLUSTER_T *xcluster_put(XCLUSTER_NAME *_xc, XCLUSTER_T _elm)
{
    XCLUSTER_T *ptr = xcluster_put_ptr(_xc, &_elm);
    *ptr = _elm;
    return ptr;
}

xcluster_node_t *xcluster_alloc_node(XCLUSTER_NAME *_xc)
{
    if(_xc->node_reserve != NULL)
    {
        xcluster_node_t *ret = _xc->node_reserve;
        _xc->node_reserve = _xc->node_reserve->next;
        memset(ret, 0, sizeof(xcluster_node_t));
        ret->not_full_index = (size_t)-1;
        return ret;
    }
    else
    {
        xcluster_node_t *new_node = (xcluster_node_t*) malloc(sizeof(xcluster_node_t));
        memset(new_node, 0, sizeof(xcluster_node_t));
        new_node->not_full_index = (size_t)-1;
        
        XCLUSTER_MAYBE_GROW(_xc->allocations);
        XCLUSTER_PUSH(_xc->allocations, new_node);
        
        return new_node;
    }
}

size_t xcluster_bridges_with_prev_index(XCLUSTER_NAME *_xc, xcluster_node_t *_node)
{
    for(size_t i = 0 ; i < _xc->nodes_with_prev_bridges.count ; i++)
    {
        if(_xc->nodes_with_prev_bridges.array[i] == _node)
        {
            return i;
        }
    }
    return (size_t)-1;
}

void xcluster_steal_node_not_full_index(XCLUSTER_NAME *_xc, xcluster_node_t *_new_node, xcluster_node_t *_old_node)
{
    xcluster_assert(_old_node->not_full_index != (size_t)-1);
    
    _new_node->not_full_index = _old_node->not_full_index;
    _xc->not_full_nodes.array[_new_node->not_full_index] = _new_node;
    
    _old_node->not_full_index = (size_t)-1;
}

void xcluster_push_to_node_reserve(XCLUSTER_NAME *_xc, xcluster_node_t *_unused)
{
    // xcluster_assert(_unused->next == NULL);
    // xcluster_assert(_unused->prev == NULL);
    // xcluster_assert(_unused->count == 0);
    // xcluster_assert(_unused->cap == 0);
    
    xcluster_node_t *old_head = _xc->node_reserve;
    _xc->node_reserve = _unused;
    _unused->next = old_head;
    // no need to set prev, _unused->next might even be NULL, so we'd need a branch
}

XCLUSTER_T *xcluster_del(XCLUSTER_NAME *_xc, XCLUSTER_T *_elm)
{
    _xc->count -= 1;
    
    // XCLUSTER_T *end = _elm;
    // while( !XCLUSTER_IS_SENTINEL(end) )
    // {
    //     end += 1;
    // }
    xcluster_node_t *_cur = _xc->head;
    while(
        !(
            (uintptr_t)_cur->elms <= (uintptr_t)_elm && 
            (uintptr_t)&_cur->elms[_cur->count] > (uintptr_t)_elm)
        )
    {
        _cur = _cur->next;
    }
    
    xcluster_node_t *bp = _cur;
    xcluster_node_t *bp_next_node = bp->next;
    xcluster_node_t *bp_prev_node = bp->prev;
    
    xcluster_assign_sentinel(_elm, bp);
    
    size_t deleted_index = _elm - bp->elms;
    
    if(deleted_index == 0)
    {
        // current node has 0 elms, unlink it from the chain, but keep its as a bridge_prev for the new node
        // BUT. if it has a prev_bridge, merge with it to reduce number of nodes
        
        xcluster_node_t *new_node = xcluster_alloc_node(_xc);
        
        new_node->elms = bp->elms + 1;
        new_node->count = bp->count - 1;
        new_node->cap = bp->cap - 1;
        
        bp->count = 0;
        bp->cap = 0;
        
        // OK so here's actually what should happen:
        // if bp has a bridge_prev, merge with it: if bp->not_full_index!=-1 then reuse its slot, if it is -1 then push the bridge_prev
        // when should new_node steal bp's not_full_index? when it merges with node_prev and node_prev has a not_full_index
        // otherwise new_node should be pushed and get its own index
        
        if(bp->bridge_prev != NULL)
        {
            xcluster_node_t *bp_bridge_prev = bp->bridge_prev;
            xcluster_assert((bp_bridge_prev->elms + bp_bridge_prev->cap + 1) == bp->elms);
            
            bp_bridge_prev->cap = bp_bridge_prev->cap + 1;
            
            if(bp_bridge_prev->not_full_index == (size_t)-1)
            {
                if(bp->not_full_index != (size_t)-1)
                {
                    xcluster_steal_node_not_full_index(_xc, bp_bridge_prev, bp);
                }
                else
                {
                    xcluster_push_not_full_node(_xc, bp_bridge_prev);
                }
            }
            
            bp_bridge_prev->bridge_next = new_node;
            new_node->bridge_prev = bp_bridge_prev;
            
            size_t index = xcluster_bridges_with_prev_index(_xc, bp); // TODO maybe nodes should store with_prev_index
            xcluster_assert(index != (size_t)-1);
            _xc->nodes_with_prev_bridges.array[index] = new_node;
            
            xcluster_assert(bp_bridge_prev->bridge_next == new_node);
            xcluster_assert(new_node->bridge_prev == bp_bridge_prev);
            xcluster_assert((bp_bridge_prev->elms + bp_bridge_prev->cap + 1) == new_node->elms);
            xcluster_assert(new_node->bridge_prev != new_node);
            
            new_node->bridge_next = bp->bridge_next;
            if(new_node->bridge_next != NULL)
            {
                new_node->bridge_next->bridge_prev = new_node;
            }
            
            { // begin
                if(new_node->cap > new_node->count)
                {
                    if(bp->not_full_index != (size_t)-1)
                    {
                        xcluster_steal_node_not_full_index(_xc, new_node, bp);
                    }
                    else
                    {
                        xcluster_push_not_full_node(_xc, new_node);
                    }
                }
                else if(bp->not_full_index != (size_t)-1)
                {
                    xcluster_erase_not_full_node(_xc, bp);
                }
                
                xcluster_assert(
                    XCLUSTER_SENTINEL_GET_PTR((&new_node->elms[new_node->count])) == bp->next->elms
                );
                
                XCLUSTER_T *ret = NULL;
                
                if(new_node->count != 0)
                {
                    xcluster_steal_node_links(_xc, new_node, bp);
                    xcluster_assign_sentinel(&new_node->elms[new_node->count], new_node);
                    ret = new_node->elms;
                }
                else
                {
                    xcluster_unlink_node(_xc, bp);
                    ret = bp_next_node->elms;
                }
                
                xcluster_push_to_node_reserve(_xc, bp);
                
                xcluster_assert(new_node->bridge_prev != new_node);
#ifdef XCLUSTER_DEBUG
                xcluster_validate(_xc); // now the problem is bp has cap 0 but is still in not_full_nodes
#endif
                return ret;
            } // end
        }
        else
        {
            new_node->bridge_prev = bp;
            xcluster_assert(new_node->bridge_prev != new_node);
            
            XCLUSTER_MAYBE_GROW(_xc->nodes_with_prev_bridges);
            XCLUSTER_PUSH(_xc->nodes_with_prev_bridges, new_node);
            
            xcluster_assert((bp->elms + bp->cap + 1) == new_node->elms);
            
            new_node->bridge_next = bp->bridge_next;
            if(new_node->bridge_next != NULL)
            {
                new_node->bridge_next->bridge_prev = new_node;
            }
            bp->bridge_next = new_node;
            
            { // begin
                if(new_node->cap > new_node->count)
                {
                    if(bp->not_full_index != (size_t)-1)
                    {
                        xcluster_steal_node_not_full_index(_xc, new_node, bp);
                    }
                    else
                    {
                        xcluster_push_not_full_node(_xc, new_node);
                    }
                }
                
                xcluster_assert(
                    XCLUSTER_SENTINEL_GET_PTR((&new_node->elms[new_node->count])) == bp->next->elms
                );
                
                XCLUSTER_T *ret = NULL;
                
                if(new_node->count != 0)
                {
                    xcluster_steal_node_links(_xc, new_node, bp);
                    xcluster_assign_sentinel(&new_node->elms[new_node->count], new_node);
                    ret = new_node->elms;
                }
                else
                {
                    ret = bp_next_node->elms;
                    xcluster_unlink_node(_xc, bp); // TODO the error is on this line. because bp was pushed to node reserve in one of the branches above
                }
                
                xcluster_assert(new_node->bridge_prev != new_node);
#ifdef XCLUSTER_DEBUG
                xcluster_validate(_xc); // error found in this branch
#endif
                return ret;
            } // end
        }
    }
    else if(deleted_index == bp->count - 1)
    {
        bp->count -= 1;
        xcluster_assign_sentinel(&bp->elms[bp->count], bp);
        
        if(bp->not_full_index == (size_t)-1)
        {
            xcluster_push_not_full_node(_xc, bp);
        }
        
#ifdef XCLUSTER_DEBUG
        xcluster_validate(_xc);
#endif
        return bp->next->elms;
    }
    else
    {
        xcluster_node_t *new_node = xcluster_alloc_node(_xc);
        
        // let's say old_cap is 8, deleted index is 2
        // that means new_node will be starting at 3 through 8, so 0->5 so cap=5
        // let's say old_count is 6
        // new count is 2
        // new cap is 2
        // new_node count is 3
        new_node->count = bp->count - deleted_index - 1;
        new_node->cap   = bp->cap   - deleted_index - 1;
        
        bp->count = deleted_index;
        bp->cap = bp->count;
        
        new_node->elms = bp->elms + bp->count + 1;
        
        xcluster_assign_next(new_node, bp->next);
        new_node->prev = bp;
        new_node->next->prev = new_node;
        xcluster_assign_next(new_node->prev, new_node);
        
        if(bp == _xc->tail)
        {
            _xc->tail = new_node;
        }
        
        new_node->bridge_next = bp->bridge_next;
        bp->bridge_next = new_node;
        new_node->bridge_prev = bp;
        
        if(new_node->bridge_next != NULL)
        {
            new_node->bridge_next->bridge_prev = new_node;
        }
        
        xcluster_assert((bp->elms + bp->cap + 1) == new_node->elms);
        
        if(bp->not_full_index != (size_t)-1)
        {
            new_node->not_full_index = bp->not_full_index;
            _xc->not_full_nodes.array[new_node->not_full_index] = new_node;
            bp->not_full_index = (size_t)-1;
        }
        
        XCLUSTER_MAYBE_GROW(_xc->nodes_with_prev_bridges);
        XCLUSTER_PUSH(_xc->nodes_with_prev_bridges, new_node);
        
        xcluster_assign_sentinel(_elm, bp);
        xcluster_assign_sentinel(&new_node->elms[new_node->count], new_node);
        
#ifdef XCLUSTER_DEBUG
        xcluster_validate(_xc);
#endif
        return new_node->elms;
    }
}

void xcluster_deinit(XCLUSTER_NAME *_xc)
{
    for(size_t i = 0 ; i < _xc->allocations.count ; i++)
    {
        free(_xc->allocations.array[i]);
    }
    free(_xc->allocations.array);
    free(_xc->end_sentinel);
    free(_xc->not_full_nodes.array);
    free(_xc->nodes_with_prev_bridges.array);
}

XCLUSTER_T *xcluster_begin(XCLUSTER_NAME *_xc)
{
    return (XCLUSTER_T*){_xc->head->elms};
}

XCLUSTER_T *xcluster_end(XCLUSTER_NAME *_xc)
{
    return (XCLUSTER_T*){_xc->end_sentinel->elms};
}

XCLUSTER_T *xcluster_next(XCLUSTER_T *it)
{
    it += 1;
    if(XCLUSTER_IS_SENTINEL(it))
    {
        return (XCLUSTER_T*) XCLUSTER_SENTINEL_GET_PTR(it);
    }
    return it;
}

bool xcluster_validate(XCLUSTER_NAME *_xc)
{
    xcluster_node_t *_node = _xc->head;
    xcluster_node_t *prev = NULL;
    size_t accum = 0;
    while(_node != _xc->end_sentinel)
    {
        accum += _node->count;
        
        for(size_t i = 0 ; i < _node->count ; i++)
        {
            xcluster_assert(!XCLUSTER_IS_SENTINEL((&_node->elms[i])));
        }
        xcluster_assert(_node->count > 0);
        xcluster_assert(XCLUSTER_IS_SENTINEL((&_node->elms[_node->count])));
        XCLUSTER_T *bb = (XCLUSTER_T*) XCLUSTER_SENTINEL_GET_PTR((&_node->elms[_node->count]));
        xcluster_assert(bb == _node->next->elms);
        
        xcluster_assert(_node->prev == prev);
        
        prev = _node;
        _node = _node->next;
        
        xcluster_assert(_node != NULL);
    }
    
    for(size_t i = 0 ; i < _xc->not_full_nodes.count ; i++)
    {
        xcluster_node_t *_node = _xc->not_full_nodes.array[i];
        xcluster_assert(_node->cap > _node->count);
        xcluster_assert(_node->not_full_index == i);
        xcluster_assert((_node->count == 0 && _node->next == NULL) || (_node->count != 0 && _node->next != NULL));
        if(_node->count != 0)
            xcluster_assert(_node == _xc->head || _node->prev != NULL);
    }
    
    for(size_t i = 0 ; i < _xc->nodes_with_prev_bridges.count ; i++)
    {
        xcluster_node_t *node = _xc->nodes_with_prev_bridges.array[i];
        xcluster_node_t *prev = node->bridge_prev;
        xcluster_assert(prev != NULL);
        xcluster_assert((prev->elms + prev->cap + 1) == node->elms);
        xcluster_assert(prev->bridge_next == node);
        if(prev->next == NULL)
        {
            xcluster_assert(prev->count == 0);
        }
        else
        {
            xcluster_assert(XCLUSTER_SENTINEL_GET_PTR(&prev->elms[prev->count]) == prev->next->elms);
        }
    }
    
    xcluster_assert(accum == _xc->count);
    return true;
}

#endif

#undef XCLUSTER_CAT_
#undef XCLUSTER_CAT

#undef XCLUSTER_TYPEOF

#undef xcluster_node_t

#undef xcluster_init
#undef xcluster_put_ptr
#undef xcluster_put
#undef xcluster_del
#undef xcluster_deinit
#undef xcluster_begin
#undef xcluster_end
#undef xcluster_next

#undef xcluster_erase_not_full_node
#undef xcluster_push_not_full_node
#undef xcluster_assign_sentinel
#undef xcluster_unlink_node
#undef xcluster_steal_node_links
#undef xcluster_alloc_node
#undef xcluster_steal_node_not_full_index
#undef xcluster_bridges_with_prev_index
#undef xcluster_link_node
#undef xcluster_validate

#undef XCLUSTER_PUSH
#undef XCLUSTER_POP
#undef XCLUSTER_MAYBE_GROW
