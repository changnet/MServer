#ifndef __LRU_LINK_H__
#define __LRU_LINK_H__

#include "../doubly_link.h"

template<class T>
class lru_link : public doubly_link<T>
{
private:
    typedef typename doubly_link<T>::template node<T> lru_node;
public:
    void move_back( lru_node *_node );
};

template<class T>
void lru_link<T>::move_back( lru_node *_node )
{
    assert( _node && this->_back );

    if ( this->_back == _node ) return;

    lru_node *_last = _node->_last;
    lru_node *_next = _node->_next;

    if ( _last ) _last->_next = _next; else this->_front = _next;

    assert( _next );  // move back must have a next element
    _next->_last = _last;

    (this->_back)->_next = _node;
    _node->_last  =  this->_back;
    _node->_next  =  NULL;

    this->_back = _node;
}

#endif /* __LRU_LINK_H__ */
