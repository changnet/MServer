#ifndef __LRU_K_H__
#define __LRU_K_H__

#include <list>
#include <map>
#include "../../global/global.h"

template<class K>
class lru_k
{
public:
    lru_k();
    ~lru_k() {}

    size_t size();
    K &oldest  ();
    void erase ( const K &k);
    void update( const K &k);
    void remove_oldest();
private:
    size_t _size;  /* std::list.size() == std::distance(begin(), end()) O(n)*/
    std::list< K > _list;
    typedef typename std::list< K >::iterator list_iterator;

    std::map< K,list_iterator > _map;
    typedef typename std::map< K,list_iterator >::iterator map_iterator;
};

template<class K>
lru_k<K>::lru_k()
{
    _size = 0;
}

template<class K>
size_t lru_k<K>::size()
{
    return _size;
}

template<class K>
K &lru_k<K>::oldest()
{
    assert ( "get oldest from a empty list",_size > 0 );

    return _list.back();
}

template<class K>
void lru_k<K>::erase( const K &k )
{
    map_iterator itr = _map.find( k );
    if ( itr != _map.end() )
    {
        _list.erase( itr->second );
        _map.erase( itr );
        --_size;
        
        return;
    }
    
    assert( "erase a no exist element",false ); // debug
}

template<class K>
void lru_k<K>::update( const K &k )
{
    map_iterator itr = _map.find( k );
    if ( itr != _map.end() )
    {
        _list.erase( itr->second );
        
        _list.push_front( k );
        itr->second = _list.begin();
        
        return;
    }
    
    /* list、map的begin是有效的，而end是无效的，因此map中存begin而不是end */
    _list.push_front( k );
    _map.insert( std::pair< K,list_iterator >(k,_list.begin()) );
    ++_size;
}

template<class K>
void lru_k<K>::remove_oldest()
{
    assert( "remove oldest element from a empty container",_size > 0 ); //debug
    if ( _size <= 0 ) return;

    const K &k = _list.back();
    erase( k );
}

#endif /* __LRU_K_H__ */
