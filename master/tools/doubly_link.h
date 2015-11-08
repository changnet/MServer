#ifndef __DOUBLY_LINK_H__
#define __DOUBLY_LINK_H__

#include <cstddef>
#include <cassert>

template<class T>
class doubly_link
{
public:
    template<class N>
    class node
    {
    public:
        N &value() { return _value; }
        class node<N> *next() { return _next; }
        class node<N> *last() { return _last; }
        friend class doubly_link;
    public:
        N _value;
        class node<N> *_last;
        class node<N> *_next;
    };
public:
    explicit doubly_link();
    ~doubly_link();

    T *front();
    T *back();

    void pop_front();
    void pop_back();

    void push_front( const T &value );
    void push_back( const T &value );

    size_t size();
    void clear ();

    class node<T> *begin();
    class node<T> *end  ();
    void erase( class node<T> *_node );
    void insert( class node<T> *_node,const T &value );
protected:
    class node<T> *_back ;
    class node<T> *_front;
    size_t _size;
    // TODO 增加一个cache
};

template<class T>
doubly_link<T>::doubly_link()
{
    _size  = 0;
    _front = NULL;
    _back  = NULL;
}

template<class T>
doubly_link<T>::~doubly_link()
{
    while ( _front )
    {
        class node<T> *_node = _front;
        _front = _front->_next;

        delete _node;
        --_size;  // to assert
    }
    _back = NULL;

    assert( 0 == _size && !_front && !_back );
}

template<class T>
void doubly_link<T>::push_front( const T &value )
{
    class node<T> *_node = new node<T>();
    _node->_value = value;

    _node->_last = NULL;
    _node->_next = _front;

    if ( _front ) _front->_last = _node;
    _front = _node;

    if ( !_back ) _back = _front;

    ++_size;
}

template<class T>
T *doubly_link<T>::front()
{
    assert( _size >= 0 );
    if ( !_front ) return NULL;

    return &(_front->_value);
}

template<class T>
void doubly_link<T>::pop_front()
{
    assert( _size >= 0 );
    if ( !_front ) return;

    class node<T> *_node = _front;
    _front = _front->_next;
    _front->_last = NULL;

    delete _node;
    --_size;

    if ( !_size ) _back = NULL;
}

template<class T>
void doubly_link<T>::push_back( const T &value )
{
    class node<T> *_node = new node<T>();
    _node->_value = value;

    _node->_next = NULL;
    _node->_last = _back;

    if ( _back ) _back->_next = _node;
    _back = _node;

    if ( !_front ) _front = _back;

    ++_size;
}

template<class T>
T *doubly_link<T>::back()
{
    assert( _size >= 0 );
    if ( !_back ) return NULL;

    return &(_back->_value);
}

template<class T>
void doubly_link<T>::pop_back()
{
    assert( _size >= 0 );
    if ( !_back ) return;

    class node<T> *_node = _back;
    _back = _back->_last;
    _back->_next = NULL;

    delete _node;
    --_size;

    if ( !_size ) _front = NULL;
}

template<class T>
void doubly_link<T>::erase( class node<T> *_node )
{
    assert( _node );

    class node<T> *_last = _node->_last;
    class node<T> *_next = _node->_next;

    if ( _last )
    {
        _last->_next = _next;
    }
    else
    {
        _front = _next;
    }

    if ( _next )
    {
        _next->_last = _last;
    }
    else
    {
        _back = _last;
    }

    delete _node;
    --_size;

    assert( _size >= 0 );
}

template<class T>
size_t doubly_link<T>::size()
{
    return _size;
}

template<class T>
class doubly_link<T>::node<T> *doubly_link<T>::begin()
{
    return _front;
}

template<class T>
class doubly_link<T>::node<T> *doubly_link<T>::end()
{
    return _back;
}

template<class T>
void doubly_link<T>::clear()
{
    while ( _front )
    {
        class node<T> *_node = _front;
        _front = _front->_next;

        delete _node;
    }

    _size = 0;
    _back = NULL;
}

/*  inserting new elements before the element at the specified position */
template<class T>
void doubly_link<T>::insert( class node<T> *_node,const T &value )
{
    class node<T> *_node_ = new class node<T>();
    _node_->_value = value;

    _node_->_next = _node;
    _node_->_last = _node->_last;

    class node<T> *last = _node->_last;
    if ( last )
    {
        last->_next = _node_;
    }
    else
    {
        _front = _node_;  // new node become front
    }

    _node->_last = _node_;

    ++_size;
}

#endif /* __DOUBLY_LINK_H__ */
