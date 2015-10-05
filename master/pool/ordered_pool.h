#ifndef __ORDERED_POOL_H__
#define __ORDERED_POOL_H__

/* 等长内存池，参考了boost内存池(boolst/pool/pool.hpp).分配的内存只能是ordered_size
 * 的n倍。每一个n都形成一个空闲链表，利用率比boost低。
 * 1.分配出去的内存不再受池的管理
 * 2.所有内存在池销毁时会释放(包括未归还的)
 * 3.没有约束内存对齐。因此用的是系统默认对齐，在linux 32/64bit应该是OK的
 * 4.最小内存块不能小于一个指针长度(4/8 bytes)
 */

#include "../global/global.h"

template<uint32 ordered_size,uint32 chunk_size = 512>
class ordered_pool
{
public:
    ordered_pool();
    ~ordered_pool();

    char *ordered_malloc( uint32 n = 1 );
    void ordered_free  ( char * const ptr,uint32 n );
private:
    typedef void * NODE;

    NODE *anpts;    /* 空闲内存块链表数组,倍数n为下标 */
    uint32 anptmax;
    
    void *block_list; /* 从系统分配的内存块链表 */

    /* 一块内存的指针是ptr,这块内存的前几个字节储存了下一块内存的指针地址
     * 即ptr可以看作是指针的指针
     * nextof返回这地址的引用
     */
    inline void * & nextof( void * const ptr )
    {
        return *(static_cast<void **>(ptr));
    }
    
    /* 把从系统获取的内存分成小块存到链表中
     * 这些内存块都是空的，故在首部创建一个指针，存放指向下一块空闲内存的地址
     * @ptr           内存块起始地址
     * @partition_sz  单块内存大小
     * @npartition    单块内存数量
     * @n             等长系数
     */
    inline void *segregate( void * const ptr,uint32 partition_sz,
        uint32 npartition,uint32 n )
    {
        char *last = static_cast<char *>(ptr);
        for ( uint32 i = 1;i < npartition;i ++ )
        {
            char *next = last + partition_sz;
            nextof( last ) = next;
            last = next;
        }

        nextof( last ) = anpts[n];
        return anpts[n] = ptr;
    }
};

template<uint32 ordered_size,uint32 chunk_size>
ordered_pool<ordered_size,chunk_size>::ordered_pool()
    : anpts(NULL),anptmax(0),block_list(NULL)
{
    assert( "ordered size less then sizeof(void *)",ordered_size >= sizeof(void *) );
}

template<uint32 ordered_size,uint32 chunk_size>
ordered_pool<ordered_size,chunk_size>::~ordered_pool()
{
    if ( anpts )
        delete []anpts;
    anpts   = NULL;
    anptmax = 0;

    while ( block_list )
    {
        char *_ptr = static_cast<char *>(block_list);
        block_list = nextof( block_list );
        
        delete []_ptr;
    }
}

/* 分配N*ordered_size内存 */
template<uint32 ordered_size,uint32 chunk_size>
char *ordered_pool<ordered_size,chunk_size>::ordered_malloc( uint32 n )
{
    assert( "ordered_malloc n <= 0",n > 0 );
    array_resize( NODE,anpts,anptmax,n+1,array_zero );
    void *ptr = anpts[n];
    if ( ptr )
    {
        anpts[n] = nextof( ptr );
        return static_cast<char *>(ptr);
    }
    
    /* 每次固定申请chunk_size块大小为(n*ordered_size)内存
     * 不用指数增长方式因为内存分配过大可能会失败
     */
    uint32 partition_sz = n*ordered_size;
    assert( "buffer overflow",UINT_MAX/partition_sz > chunk_size );

    uint64 block_size = sizeof(void *) + chunk_size*partition_sz;
    char *block = new char[block_size];

    /* 分配出来的内存，预留一个指针的位置在首部，用作链表将所有从系统获取的
     * 内存串起来
     */
    nextof( block ) = block_list;
    block_list = block;
    
    /* 第一块直接分配出去，其他的分成小块存到anpts对应的链接中 */
    segregate( block + sizeof(void *) + partition_sz,partition_sz,
        chunk_size - 1,n );
    return block + sizeof(void *);
}

template<uint32 ordered_size,uint32 chunk_size>
void ordered_pool<ordered_size,chunk_size>::ordered_free( char * const ptr,uint32 n )
{
    assert( "illegal ordered free",anptmax >= n && ptr );
    nextof( ptr ) = anpts[n];
    anpts[n] = ptr;
}

#endif /* __ORDERED_POOL_H__ */
