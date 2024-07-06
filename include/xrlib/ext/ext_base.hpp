/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <assert.h>
#include <string>

#include <openxr/openxr.h>

namespace xrlib
{
	class ExtBase
	{
	  public:
		ExtBase( XrInstance xrInstance, std::string sName ) : 
			m_xrInstance( xrInstance ),
			m_sName( sName )
		{
			assert( xrInstance != XR_NULL_HANDLE );
			assert( !sName.empty() );
		}

		~ExtBase() {};

		std::string GetName() { return m_sName; }

	  protected:
		XrInstance m_xrInstance = XR_NULL_HANDLE;
		std::string m_sName;

	};

} 
