#ifndef FIXEDNAME_H
#define FIXEDNAME_H

#include <stddef.h>
#include <string.h>
#include <string>

inline size_t FixedNameLength( const char *name, size_t capacity )
{
	size_t length = 0;
	while( length < capacity && name[ length ] )
		++length;
	return length;
}

inline bool FixedNameEquals( const char *left, size_t leftCapacity,
							 const char *right, size_t rightCapacity )
{
	if( !left || !right )
		return false;
	const size_t leftLength = FixedNameLength( left, leftCapacity );
	const size_t rightLength = FixedNameLength( right, rightCapacity );
	return leftLength == rightLength && !memcmp( left, right, leftLength );
}

inline bool FixedNameEquals( const char *field, size_t capacity, const char *name )
{
	return name && FixedNameEquals( field, capacity, name, strlen( name ) + 1 );
}

inline bool FixedNameEquals( const char *field, size_t capacity, const std::string &name )
{
	const size_t fieldLength = FixedNameLength( field, capacity );
	return fieldLength == name.size() && !memcmp( field, name.data(), fieldLength );
}

inline std::string FixedNameString( const char *field, size_t capacity )
{
	return std::string( field, FixedNameLength( field, capacity ) );
}

#endif
