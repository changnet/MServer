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
        N value;
        class node<N> *last;
        class node<N> *next;
    };
public:
    explicit doubly_link();
    ~doubly_link();

    void push_front( T value );
    T pop_front();
    void push_back( T value );
    T pop_back();
    
    size_t size();
    void clear ();
    
    class node<T> *find( T value );
    void erase( class node<T> *_node );
private:
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
    while ( _size )
    {
        class node<T> *temp = _front;
        _front = _front->next;
        
        delete temp;
        --_size;
    }
    _back = NULL;

    assert( 0 == _size && !_front && !_back );
}

template<class T>
void doubly_link<T>::push_front( T value )
{
    class node<T> *_node = new node<T>();
    _node->value = value;

    _node->last = NULL;
    _node->next = _front;
    
    if ( _front ) _front->last = _node;
    _front = _node;
    
    if ( !_back ) _back = _front;
    
    ++_size;
}

template<class T>
T doubly_link<T>::pop_front()
{
    assert( _size >= 0 );
    if ( !_size ) return NULL;
    
    T value = _front->value;
    
    class node<T> *_node = _front;
    _front = _front->next;
    --_size;

    if ( !_size ) _back = NULL;
    
    return value;
}

template<class T>
void doubly_link<T>::push_back( T value )
{
    class node<T> *_node = new node<T>();
    _node->value = value;

    _node->next = NULL;
    _node->last = _back;
    
    if ( _back ) _back->next = _node;
    _back = _node;
    
    if ( !_front ) _front = _back;
    
    ++_size;
}

template<class T>
T doubly_link<T>::pop_back()
{
    assert( _size >= 0 );
    if ( !_size ) return NULL;
    
    T value = _back.value;
    
    class node<T> *_node = _back;
    _back = _back.last;
    
    delete _node;
    --_size;
    
    if ( !_size ) _front = NULL;
    
    return value;
}

template<class T>
void doubly_link<T>::erase( class node<T> *_node )
{
    
}

template<class T>
size_t doubly_link<T>::size()
{
    return _size;
}

template<class T>
class doubly_link<T>::node<T> *doubly_link<T>::find( T value )
{
    return NULL;
}

template<class T>
void doubly_link<T>::clear()
{
}

#endif /* __DOUBLY_LINK_H__ */
