#ifndef __LRU_H__
#define __LRU_H__

#include "lru_link.h"
#include <map>

template<class K>
class lru
{
private:
    typedef typename doubly_link<K>::template node<K> lru_node;

    class lru_link<K> _link;
    std::map< K,lru_node* > _map;
public:
    size_t size();
    K *oldest  ();
    void pop   ( const K &k);
    void update( const K &k);
    void remove_oldest();
};

template<class K>
size_t lru<K>::size()
{
    return _link.size();
}

template<class K>
void lru<K>::update( const K &k )
{
    typename std::map< K,lru_node* >::iterator itr = _map.find( k );
    if ( itr != _map.end() )
    {
        _link.move_back( itr->second );
        return;
    }

    _link.push_back( k );
    _map.insert( std::pair< K,lru_node* >( k,_link.end()) );
}

template<class K>
K *lru<K>::oldest()
{
    return _link.front();
}

template<class K>
void lru<K>::pop( const K &k )
{
    typename std::map< K,lru_node* >::iterator itr = _map.find( k );
    if ( itr != _map.end() )
    {
        _link.erase( itr->second );
        _map.erase( itr );
        return;
    }
}

template<class K>
void lru<K>::remove_oldest()
{
    lru_node *_node = _link.begin();
    if ( _node )
    {
        _map.erase( _node->value() );  // remove by key
        _link.pop_front();
    }
}
#endif /* __LRU_H__ */
