/* 
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#define XR_USE_GRAPHICS_API_VULKAN 1
#define XR_LEFT 0
#define XR_RIGHT 1

#include <cassert>
#include <cmath>
#include <vulkan/vulkan.h>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

// Add android headers - must include before the openxr headers
#ifdef XR_USE_PLATFORM_ANDROID
	#include <android/asset_manager.h>
	#include <android/asset_manager_jni.h>
	#include <android/native_window.h>
	#include <android_native_app_glue.h>
	#include <jni.h>
	#include <sys/system_properties.h>
#endif

// Openxr headers
#include <third_party/openxr/include/openxr/openxr.h>
#include <third_party/openxr/include/openxr/openxr_platform.h>
#include <third_party/openxr/include/openxr/openxr_reflection.h>
#include <third_party/openxr/src/common/xr_linear.h>

namespace xrlib
{
	static const float GetDistance( XrVector3f &a, XrVector3f &b ) 
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
		return std::sqrt( dx * dx + dy * dy + dz * dz );
	};

	#ifdef XR_USE_PLATFORM_ANDROID
	struct AndroidAppState
	{
		ANativeWindow *NativeWindow = nullptr;
		bool Resumed = false;
	};

	static void app_handle_cmd( struct android_app *app, int32_t cmd )
	{
		AndroidAppState *appState = (AndroidAppState *) app->userData;

		switch ( cmd )
		{
			case APP_CMD_RESUME:
			{
				appState->Resumed = true;
				break;
			}
			case APP_CMD_PAUSE:
			{
				appState->Resumed = false;
				break;
			}
			case APP_CMD_DESTROY:
			{
				appState->NativeWindow = NULL;
				break;
			}
			case APP_CMD_INIT_WINDOW:
			{
				appState->NativeWindow = app->window;
				break;
			}
			case APP_CMD_TERM_WINDOW:
			{
				appState->NativeWindow = NULL;
				break;
			}
		}
	}
	#endif

	#define XR_MAKE_VERSION32( major, minor, patch ) ( ( ( major ) << 22 ) | ( ( minor ) << 12 ) | ( patch ) )
	#define XR_VERSION_MAJOR32( version ) ( (uint32_t) ( version ) >> 22 )
	#define XR_VERSION_MINOR32( version ) ( ( (uint32_t) ( version ) >> 12 ) & 0x3ff )
	#define XR_VERSION_PATCH32( version ) ( (uint32_t) (version) &0xfff )
	
	#define XR_ENUM_STRINGIFY( sEnum, val )                                                                                                                                                                \
		case sEnum:                                                                                                                                                                                        \
			return #sEnum;
	#define XR_ENUM_TYPE_STRINGIFY( xrEnumType )                                                                                                                                                           \
		constexpr const char *XrEnumToString( xrEnumType eNum )                                                                                                                                            \
		{                                                                                                                                                                                                  \
			switch ( eNum )                                                                                                                                                                                \
			{                                                                                                                                                                                              \
				XR_LIST_ENUM_##xrEnumType( XR_ENUM_STRINGIFY ) default : return "Unknown Enum. Define in openxr_reflection.h";                                                                             \
			}                                                                                                                                                                                              \
		}

		XR_ENUM_TYPE_STRINGIFY( XrResult );
		XR_ENUM_TYPE_STRINGIFY( XrViewConfigurationType );
		XR_ENUM_TYPE_STRINGIFY( XrSessionState );
		XR_ENUM_TYPE_STRINGIFY( XrReferenceSpaceType );

	#define INIT_PFN( instance, pfn ) xrGetInstanceProcAddr( instance, #pfn, (PFN_xrVoidFunction *) ( &pfn ) );

	#define XR_RETURN_ON_ERROR( xrResult )                                                                                                                                                                 \
		do                                                                                                                                                                                                 \
		{                                                                                                                                                                                                  \
			XrResult result = xrResult;                                                                                                                                                                    \
			if ( result != XR_SUCCESS )                                                                                                                                                                    \
				return result;                                                                                                                                                                             \
		} while ( false );
}