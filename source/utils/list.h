#ifndef LIST_H
#define LIST_H

#include <gctypes.h>
#include <string>
#include <vector>

template < class T >
class List : public std::vector< T >
{
public:
	List(){}

	// add stuff to the end of the list
	List &Append( const T& newEntry )
	{
		this->push_back( newEntry );
		return *this;
	}

	List &Append( const List< T >& otherList )
	{
		u32 len = otherList.size();
		for( u32 i = 0; i < len; i++ )
		{
			this->push_back( otherList[ i ] );
		}
		return *this;
	}

	List &operator<<( const T& newEntry )
	{
		return Append( newEntry );
	}

	List &operator<<( const List< T >& otherList )
	{
		return Append( otherList );
	}

	List &operator+=( const T& newEntry )
	{
		return Append( newEntry );
	}

	List &operator+=( const List< T >& otherList )
	{
		return Append( otherList );
	}

	// remove an item
	T TakeAt( u32 index )
	{
		u32 size = std::vector< T >::size();
		if( index >= size )
		{
			return T();
		}
		const T ret = std::vector< T >::operator[]( index );
		this->erase( std::vector< T >::begin() + index );
		return ret;
	}

	T TakeLast()
	{
		u32 size = std::vector< T >::size();
		if( !size )
		{
			return T();
		}
		const T ret = std::vector< T >::operator[]( size - 1 );
		this->erase( std::vector< T >::begin() + size - 1 );
		return ret;
	}

	T TakeFirst()
	{
		u32 size = std::vector< T >::size();
		if( !size )
		{
			return T();
		}
		const T ret = std::vector< T >::operator[]( 0 );
		this->erase( std::vector< T >::begin() );
		return ret;
	}

	bool Contains( const T& item )
	{
		u32 size = std::vector< T >::size();
		for( u32 i = 0; i < size; i++ )
		{
			if( std::vector< T >::operator[]( i ) == item )
			{
				return true;
			}
		}
		return false;
	}

	void Remove( const T& item )
	{
		u32 size = std::vector< T >::size();
		for( u32 i = 0; i < size; i++ )
		{
			if( std::vector< T >::operator[]( i ) == item )
			{
				this->erase( std::vector< T >::begin() + i );
				i--;
				size--;
			}
		}
	}
};

typedef List< std::string > StringList;

#define foreach(variable, container) for (variable : (container))


#endif // LIST_H
