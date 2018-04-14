#ifndef __LRU_KV_H__
#define __LRU_KV_H__

#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map    std::unordered_map
#endif
#include <list>
#include "../../global/global.h"

template<class K,class V>
class lru_kv
{
public:
    explicit lru_kv( size_t size = 1024 );
    ~lru_kv() {}

    size_t size();
    V *find( const K &k );      /* just find value,not auto make it last use */
    void update( const K &k );  /* make it last use element */
    void erase ( const K &k );
    void resize( size_t size );
    void insert( const K &k,const V &v );
private:
    size_t _size;  /* std::list.size() == std::distance(begin(), end()) O(n)*/
    size_t _max_size;
    std::list< std::pair<K,V> > _list;
    typedef typename std::list< std::pair<K,V> >::iterator list_iterator;

    map< K,list_iterator > _map;
    typedef typename map< K,list_iterator >::iterator map_iterator;
};

template<class K,class V>
lru_kv<K,V>::lru_kv( size_t size )
    : _max_size(size)
{
    _size = 0;
}

template<class K,class V>
size_t lru_kv<K,V>::size()
{
    return _size;
}

template<class K,class V>
V *lru_kv<K,V>::find( const K &k )
{
    map_iterator itr = _map.find( k );
    if ( itr == _map.end() )
    {
        return NULL;
    }
    
    return &((itr->second)->second);
}

template<class K,class V>
void lru_kv<K,V>::update( const K &k )
{
    map_iterator itr = _map.find( k );
    if ( itr == _map.end() )
    {
        assert( "update lru element not found",false );
        return;
    }
    
    /* if support C++11 move,will be faster here */
    V v = (itr->second)->second;
    _list.erase( itr->second );
    _list.push_front( std::pair<K,V>(k,v) );

    itr->second = _list.begin();
}

template<class K,class V>
void lru_kv<K,V>::erase( const K &k )
{
    map_iterator itr = _map.find( k );
    if ( itr == _map.end() )
    {
        assert( "erase lru element not found",false );
        return;
    }
    
    _list.erase( itr->second );
    _map.erase( itr );
}

template<class K,class V>
void lru_kv<K,V>::resize( size_t size )
{
    _max_size = size;
    while ( _size > _max_size )
    {
        _map.erase( _list.back().first );
        _list.pop_back();
        
        --_size;
    }
}

template<class K,class V>
void lru_kv<K,V>::insert( const K &k,const V &v )
{
    map_iterator itr = _map.find( k );
    if ( itr != _map.end() )
    {
        /* value update and make it last use */
        _list.erase( itr->second );
        
        _list.push_front( std::pair<K,V>(k,v) );
        itr->second = _list.begin();
        return;
    }
    
    /* new element */
    _list.push_front( std::pair<K,V>(k,v) );
    _map.insert( std::pair< K,list_iterator >( k,_list.begin() ) );
    ++_size;
    
    if ( _size > _max_size )  /* whhile ?? */
    {
        _map.erase( _list.back().first );
        _list.pop_back();
        
        --_size;
    }
    
    assert( "size over flow",_size <= _max_size );
}

#endif /* __LRU_KV_H__ */
