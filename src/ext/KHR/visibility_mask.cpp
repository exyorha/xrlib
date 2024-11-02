/* 
 * Copyright 2023-24 Beyond Reality Labs Ltd (https://beyondreality.io)
 * Copyright 2021-24 Rune Berg (GitHub: https://github.com/1runeberg, YT: https://www.youtube.com/@1RuneBerg, X: https://twitter.com/1runeberg, BSky: https://runeberg.social)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <xrlib/ext/KHR/visibility_mask.hpp>
#include <xrlib/log.hpp>

#include <vector>


namespace xrlib::KHR
{
	CVisibilityMask::CVisibilityMask( XrInstance xrInstance ): 
		ExtBase( xrInstance, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME )
	{
		XrResult result = INIT_PFN( xrInstance, xrGetVisibilityMaskKHR );
		assert( XR_UNQUALIFIED_SUCCESS( result ) );	
	}
	
	XrResult CVisibilityMask::GetVisMask(
		XrSession xrSession,
		std::vector< XrVector2f > &outVertices,
		std::vector< uint32_t > &outIndices,
		XrViewConfigurationType xrViewConfigurationType,
		uint32_t unViewIndex,
		XrVisibilityMaskTypeKHR xrVisibilityMaskType )
	{
		// Clear  output vectors
		outIndices.clear();
		outVertices.clear();

		// Get index and vertex counts
		XrVisibilityMaskKHR xrVisibilityMask { XR_TYPE_VISIBILITY_MASK_KHR };
		xrVisibilityMask.indexCapacityInput = 0;
		xrVisibilityMask.vertexCapacityInput = 0;

		XrResult xrResult = xrGetVisibilityMaskKHR( xrSession, xrViewConfigurationType, unViewIndex, xrVisibilityMaskType, &xrVisibilityMask );

		if ( xrResult != XR_SUCCESS )
		{
			LogDebug( GetName(), "Error retrieving vismask counts: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Check vismask data
		uint32_t unVertexCount = xrVisibilityMask.vertexCountOutput;
		uint32_t unIndexCount = xrVisibilityMask.indexCountOutput;

		if ( unIndexCount == 0 && unVertexCount == 0 )
		{
			LogWarning( GetName(), "Warning - runtime doesn't have a visibility mask for this view configuration!" );
			return XR_SUCCESS;
		}

		// Reserve size for output vectors
		outVertices.resize( unVertexCount );
		outIndices.resize( unIndexCount );

		// Get mask vertices and indices from the runtime
		xrVisibilityMask.vertexCapacityInput = unVertexCount;
		xrVisibilityMask.indexCapacityInput = unIndexCount;
		xrVisibilityMask.indexCountOutput = 0;
		xrVisibilityMask.vertexCountOutput = 0;
		xrVisibilityMask.indices = outIndices.data();
		xrVisibilityMask.vertices = outVertices.data();

		xrResult = xrGetVisibilityMaskKHR( xrSession, xrViewConfigurationType, unViewIndex, xrVisibilityMaskType, &xrVisibilityMask );

		if ( xrResult != XR_SUCCESS )
		{
			LogDebug( GetName(), "Error retrieving vismask data from the runtime: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult CVisibilityMask::GetVisMaskShortIndices(
		XrSession xrSession,
		std::vector< XrVector2f > &outVertices,
		std::vector< unsigned short > &outIndices,
		XrViewConfigurationType xrViewConfigurationType,
		uint32_t unViewIndex,
		XrVisibilityMaskTypeKHR xrVisibilityMaskType )
	{
		std::vector< uint32_t > vecIndices;
		XR_RETURN_ON_ERROR( 
			GetVisMask( 
				xrSession, 
				outVertices, 
				vecIndices, 
				XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
				unViewIndex, 
				XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR ) );

		outIndices.clear();
		for ( const auto &index : vecIndices )
		{
			outIndices.push_back( static_cast< unsigned short >( index ) );
		}

		return XR_SUCCESS;
	}

	XrResult CVisibilityMask::UpdateVisMask(
		XrEventDataBaseHeader xrEventData,
		XrSession xrSession,
		std::vector< XrVector2f > &outVertices,
		std::vector< uint32_t > &outIndices,
		XrViewConfigurationType xrViewConfigurationType,
		uint32_t unViewIndex,
		XrVisibilityMaskTypeKHR xrVisibilityMaskType )
	{
		if ( xrEventData.type != XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR )
			return XR_EVENT_UNAVAILABLE;

		return GetVisMask( 
			xrSession, 
			outVertices, 
			outIndices, 
			XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unViewIndex, 
			XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
	}

	XrResult CVisibilityMask::UpdateVisMaskShortIndices(
		XrEventDataBaseHeader xrEventData,
		XrSession xrSession,
		std::vector< XrVector2f > &outVertices,
		std::vector< unsigned short > &outIndices,
		XrViewConfigurationType xrViewConfigurationType,
		uint32_t unViewIndex,
		XrVisibilityMaskTypeKHR xrVisibilityMaskType )
	{
		if ( xrEventData.type != XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR )
			return XR_EVENT_UNAVAILABLE;

		return GetVisMaskShortIndices( 
			xrSession, 
			outVertices, 
			outIndices, 
			XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unViewIndex, 
			XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
	}

} 
