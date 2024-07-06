
/* 
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <cstring>
#include <string>
#include <vector>

namespace xrlib
{
	static void StringReset( char *outString, uint32_t stringLength ) 
	{ 
		std::memset( outString, '\0', stringLength * sizeof( char ) ); 
	}

	static bool StringCopy( char *outString, const uint32_t unOutStringLength, const std::string &sSourceString )
	{
		if ( unOutStringLength < static_cast< uint32_t >( sSourceString.size() ) )
		{
			StringReset( outString, unOutStringLength );
			return false;
		}

		strcpy( outString, sSourceString.c_str() );
		return true;
	}

	static bool StringCopy( char *outString, const uint32_t unOutStringLength, const char *pccSourceString, const uint32_t unSourceStringLength )
	{
		if ( unOutStringLength < unSourceStringLength )
		{
			StringReset( outString, unOutStringLength );
			return false;
		}

		std::memcpy( outString, pccSourceString, unSourceStringLength * sizeof( char ) );
		return true;
	}

	static bool FindStringInVector( std::vector< std::string > &vecStrings, std::string &sString )
	{
		for ( auto &str : vecStrings )
		{
			if ( str == sString )
			{
				return true;
			}
		}

		return false;
	}

	static std::vector< const char * > ConvertDelimitedCharArray( char *arrChars, char cDelimiter )
	{
		std::vector< const char * > vecOutput;

		// Loop 'til end of array
		while ( *arrChars != 0 )
		{
			vecOutput.push_back( arrChars );
			while ( *( ++arrChars ) != 0 )
			{
				if ( *arrChars == cDelimiter )
				{
					*arrChars++ = '\0';
					break;
				}
			}
		}
		return vecOutput;
	}

	static inline XrPosef IdentityPosef()
	{
		XrPosef xrPose {};
		xrPose.orientation.w = 1.f;
		return xrPose;
	}
}