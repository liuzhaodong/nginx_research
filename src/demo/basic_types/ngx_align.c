#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#define ngx_free          free
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
        (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define NGX_ALIGNMENT   sizeof(unsigned long)    /* platform word */
#define NGX_MAX_ALLOC_FROM_POOL  (ngx_pagesize - 1)
#define NGX_DEFAULT_POOL_SIZE    (16 * 1024)
#define NGX_POOL_ALIGNMENT       16
#define NGX_MIN_POOL_SIZE                                                     \
        ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)),            \
                              NGX_POOL_ALIGNMENT)

typedef intptr_t        ngx_int_t;
typedef uintptr_t       ngx_uint_t;
typedef intptr_t        ngx_flag_t;
typedef void (*ngx_pool_cleanup_pt)(void *data);
typedef struct ngx_pool_s           ngx_pool_t;
typedef struct ngx_pool_large_s     ngx_pool_large_t;
typedef struct ngx_chain_s          ngx_chain_t;
typedef struct ngx_pool_cleanup_s   ngx_pool_cleanup_t;
typedef int*                         ngx_log_t ;    //???
struct ngx_chain_s {
       /*ngx_buf_t    *buf;*/
       ngx_chain_t  *next;
};
typedef struct {   
    u_char               *last;   
    u_char               *end;   
//指向下一块内存池   
    ngx_pool_t           *next;   
///失败标记   
    ngx_uint_t            failed;   
} ngx_pool_data_t; 
struct ngx_pool_s {   
///数据区的指针   
    ngx_pool_data_t       d;   
///其实也就是内存池所能容纳的最大值。   
    size_t                max;   
///指向当前的内存池的头。   
    ngx_pool_t           *current;   
///这个主要是为了讲所有的内存池都链接起来。(他会创建多个内存池的)   
    ngx_chain_t          *chain;   
///这个链表表示大的数据块   
    ngx_pool_large_t     *large;   
///这个就是清理函数链表   
    ngx_pool_cleanup_t   *cleanup;   
    ngx_log_t            *log;  
}; 
struct ngx_pool_large_s {   
    ngx_pool_large_t     *next;   
    void                 *alloc;   
}; 

struct ngx_pool_cleanup_s {   
    ngx_pool_cleanup_pt   handler;   
    void                 *data;   
    ngx_pool_cleanup_t   *next;   
};  
ngx_uint_t  ngx_pagesize;

static void * ngx_palloc_block(ngx_pool_t *pool, size_t size); 
static void * ngx_palloc_large(ngx_pool_t *pool, size_t size);  
void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    void  *p;
    p = malloc(size);
    if (p == NULL) {
        printf("malloc error \r\n");
        /*ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                "malloc(%uz) failed", size);*/
    }
    //ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);
    return p;
}
//获得pagesize
void 
ngx_os_init()
{
    ngx_pagesize = getpagesize();
    printf("pagesize = %d .... \r\n",ngx_pagesize);
    getchar();
}    
ngx_pool_t *   
ngx_create_pool(size_t size, ngx_log_t *log)   
{   
    ngx_pool_t  *p;   
///可以看到直接分配size大小，也就是说我们只能使用size-sizeof(ngx_poll_t)大小   
    p = ngx_alloc(size, log);   
    if (p == NULL) {   
        return NULL;   
    }   
  
///开始初始化数据区。   
  
///由于一开始数据区为空，因此last指向数据区的开始。   
    p->d.last = (u_char *) p + sizeof(ngx_pool_t);   
///end也就是数据区的结束位置   
    p->d.end = (u_char *) p + size;   
    p->d.next = NULL;   
    p->d.failed = 0;   
  
///这里才是我们真正能使用的大小。   
    size = size - sizeof(ngx_pool_t);   
  
///然后设置max。内存池的最大值也就是size和最大容量之间的最小值。   
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;   
  
///current表示当前的内存池。   
    p->current = p;   
  
///其他的域置NULL。   
    p->chain = NULL;   
    p->large = NULL;   
    p->cleanup = NULL;   
    /*p->log = log;   */
///返回指针。   
    return p;   
} 
void *   
ngx_palloc(ngx_pool_t *pool, size_t size)   
{   
    u_char      *m;   
    ngx_pool_t  *p;   
  
///首先判断当前申请的大小是否超过max，如果超过则说明是大块，此时进入large   
    if (size <= pool->max) {   
  
///得到当前的内存池指针。   
        p = pool->current;   
  
///开始遍历内存池，   
        do {   
///首先对齐last指针。   
            m = ngx_align_ptr(p->d.last, NGX_ALIGNMENT);   
  
///然后得到当前内存池中的可用大小。如果大于请求大小，则直接返回当前的last，也就是数据的指针。   
            if ((size_t) (p->d.end - m) >= size) {   
///更新last，然后返回前面保存的last。   
                p->d.last = m + size;   
  
                return m;   
            }   
///否则继续遍历   
            p = p->d.next;   
  
        } while (p);   
///到达这里说明内存池已经满掉，因此我们需要重新分配一个内存池然后链接到当前的data的next上。(紧接着我们会分析这个函数）   
        return ngx_palloc_block(pool, size);   
    }   
  
///申请大块。   
    return ngx_palloc_large(pool, size);   
} 
 
static void *   
ngx_palloc_block(ngx_pool_t *pool, size_t size)   
{   
    u_char      *m;   
    size_t       psize;   
    ngx_pool_t  *p, *new, *current;   
  
///计算当前的内存池的大小。   
    psize = (size_t) (pool->d.end - (u_char *) pool);   
    printf("size=%d\r\n",psize); 
///再分配一个同样大小的内存池   
    m = ngx_alloc(psize, pool->log);   
    if (m == NULL) {   
        return NULL;   
    }   
  
    new = (ngx_pool_t *) m;   
  
///接下来和我们create一个内存池做的操作一样。就是更新一些指针   
    new->d.end = m + psize;   
    new->d.next = NULL;   
    new->d.failed = 0;   
  
///这里要注意了，可以看到last指针是指向ngx_pool_data_t的大小再加上要分配的size大小，也就是现在的内存池只包含了ngx_pool_data_t和数据。   
    m += sizeof(ngx_pool_data_t);   
    m = ngx_align_ptr(m, NGX_ALIGNMENT);   
    new->d.last = m + size;   
  
///设置current。   
    current = pool->current;   
  
///这里遍历所有的子内存池，这里主要是通过failed来标记重新分配子内存池的次数，然后找出最后一个大于4的，标记它的下一个子内存池为current。   
    for (p = current; p->d.next; p = p->d.next) {   
        if (p->d.failed++ > 4) {   
            current = p->d.next;   
        }   
    }   
  
///链接到最后一个内存池后面   
    p->d.next = new;   
  
///如果current为空，则current就为new。   
    pool->current = current ? current : new;   
  
    return m;   
} 

static void *   
ngx_palloc_large(ngx_pool_t *pool, size_t size)   
{   
    void              *p;   
    ngx_uint_t         n;   
    ngx_pool_large_t  *large;   
  
///分配一块内存。   
    p = ngx_alloc(size, pool->log);   
    if (p == NULL) {   
        return NULL;   
    }   
  
    n = 0;   
///开始遍历large链表，如果有alloc(也就是内存区指针)为空，则直接指针赋值然后返回。一般第一次请求大块内存都会直接进入这里。并且大块内存是可以被我们手动释放的。   
    for (large = pool->large; large; large = large->next) {   
        if (large->alloc == NULL) {   
            large->alloc = p;   
            return p;   
        }   
   
        if (n++ > 3) {   
            break;   
        }   
    }   
  
///malloc一块ngx_pool_large_t。   
    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));   
    if (large == NULL) {   
        ngx_free(p);   
        return NULL;   
    }   
  
///然后链接数据区指针p到large。这里可以看到直接插入到large链表的头的。   
    large->alloc = p;   
    large->next = pool->large;   
    pool->large = large;   
  
    return p;   
} 
void
ngx_reset_pool(ngx_pool_t *pool)
{
    ngx_pool_t        *p;
    ngx_pool_large_t  *l;
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }
    pool->large = NULL;
    for (p = pool; p; p = p->d.next) {
        p->d.last = (u_char *) p + sizeof(ngx_pool_t);
    }
}
void   
ngx_destroy_pool(ngx_pool_t *pool)   
{   
    ngx_pool_t          *p, *n;   
    ngx_pool_large_t    *l;   
    ngx_pool_cleanup_t  *c;   
  
///先做清理工作。   
    for (c = pool->cleanup; c; c = c->next) {   
        if (c->handler) {   
            /*ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,   
                           "run cleanup: %p", c);*/
            c->handler(c->data);   
        }   
    }   
  
///free大块内存   
    for (l = pool->large; l; l = l->next) {   
  
        /*ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);*/   
  
        if (l->alloc) {   
            ngx_free(l->alloc);   
        }   
    }   
  
///遍历小块内存池。   
    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {   
///直接free掉。   
        ngx_free(p);   
  
        if (n == NULL) {   
            break;   
        }   
    }   
} 
int main(int argc,char *argv[])
{
    char *p;
    int i=0;
    int c=10000;
    ngx_os_init();
    ngx_pool_t* pool =ngx_create_pool(12,NULL);
    while(c-->0)
    {    
        for(i=0;i<100;i++)
        {
            p=(char*)ngx_palloc(pool,11);
            printf("get %p \r\n",p);
        }
        if(c%10 == 0){ngx_reset_pool(pool);printf("Reset pool..................\r\n");}
    }
    getchar();
    return 0;
}
