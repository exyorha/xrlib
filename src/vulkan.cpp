/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/

#include <xrlib/vulkan.hpp>
#include <xrlib/common.hpp>
#include <xrlib/utility_functions.hpp>

#include <volk.h>

namespace xrlib
{
	CVulkan::CVulkan( CSession *pSession ) : 
		m_pSession( pSession)
	{ 
		assert( pSession );
	}

	CVulkan::~CVulkan() 
	{
		// Destroy logical device
		if ( m_vkDevice )
			vkDestroyDevice( m_vkDevice, nullptr );
	}

	XrResult CVulkan::Init( 
		VkSurfaceKHR *pSurface, 
		void *pVkInstanceNext, 
		void *pXrVkInstanceNext, 
		void *pVkLogicalDeviceNext, 
		void *pXrLogicalDeviceNext ) 
	{ 
		// Check if there's a valid openxr instance
		if ( GetAppInstance()->GetXrInstance() == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Call *required* function GetVulkanGraphicsRequirements
		//     This returns the min-and-max vulkan api level required by the active runtime
		XR_RETURN_ON_ERROR( GetVulkanGraphicsRequirements() );

		// Define app info for vulkan
		VkApplicationInfo vkApplicationInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		vkApplicationInfo.pApplicationName = GetAppInstance()->GetAppName();
		vkApplicationInfo.applicationVersion = static_cast< uint32_t >( GetAppInstance()->GetAppVersion() );
		vkApplicationInfo.pEngineName = XRLIB_NAME;
		vkApplicationInfo.engineVersion = XR_MAKE_VERSION32( XRLIB_VERSION_MAJOR, XRLIB_VERSION_MINOR, XRLIB_VERSION_PATCH );
		vkApplicationInfo.apiVersion = VK_API_VERSION_1_2;

		// Add vulkan layers and extensions the library needs
		for ( auto validationLayer : m_vecValidationLayers )
			vecLayers.push_back( validationLayer );

		for ( auto validationExtension : m_vecValidationExtensions )
			vecExtensions.push_back( validationExtension );

		#ifdef XRVK_VULKAN_DEBUG_ENABLE
				vecExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		#endif

		#ifdef XRVK_VULKAN_DEBUG2_ENABLE
				vecExtensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
				vecExtensions.push_back( VK_EXT_DEBUG_MARKER_EXTENSION_NAME );
		#endif

		#ifdef XRVK_VULKAN_DEBUG3_ENABLE
				vecExtensions.push_back( VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME );
		#endif

		// Create vulkan instance
		VkInstanceCreateInfo vkInstanceCreateInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
		vkInstanceCreateInfo.enabledLayerCount = (uint32_t) vecLayers.size();
		vkInstanceCreateInfo.ppEnabledLayerNames = vecLayers.empty() ? nullptr : vecLayers.data();
		vkInstanceCreateInfo.enabledExtensionCount = (uint32_t) vecExtensions.size();
		vkInstanceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

		XrVulkanInstanceCreateInfoKHR xrVulkanInstanceCreateInfo { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
		xrVulkanInstanceCreateInfo.next = pXrVkInstanceNext;
		xrVulkanInstanceCreateInfo.systemId = GetAppInstance()->GetXrSystemId();
		xrVulkanInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
		xrVulkanInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;
		xrVulkanInstanceCreateInfo.vulkanAllocator = nullptr;

		// ... validation
		VkDebugUtilsMessengerCreateInfoEXT vkDebugCreateInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		vkDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		vkDebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		vkDebugCreateInfo.pfnUserCallback = Callback_Debug;
		
		// ... next chain
		vkDebugCreateInfo.pNext = pVkInstanceNext;
		vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &vkDebugCreateInfo;

		VkResult vkResult = VK_SUCCESS;
		XrResult xrResult = CreateVkInstance( vkResult, GetAppInstance()->GetXrInstance(), &xrVulkanInstanceCreateInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) || vkResult != VK_SUCCESS )
		{
			LogError( "Error creating vulkan instance with openxr result (%s) and vulkan result (%i)", XrEnumToString( xrResult ), (int32_t) vkResult );
			return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
		}

		volkLoadInstance( m_vkInstance );
		LogInfo( XRLIB_NAME, "Vulkan instance successfully created." );

		// Get the vulkan physical device (gpu) used by the runtime
		xrResult = GetVulkanGraphicsPhysicalDevice();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Create vulkan logical device
		xrResult = CreateVulkanLogicalDevice( pSurface, pVkLogicalDeviceNext, pXrLogicalDeviceNext );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult CVulkan::GetVulkanGraphicsRequirements() 
	{ 
		// Get the vulkan graphics requirements (min/max vulkan api version, etc) of the runtime
		PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrGetVulkanGraphicsRequirements2KHR ) );

		m_xrGraphicsRequirements = XrGraphicsRequirementsVulkan2KHR{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };
		XR_RETURN_ON_ERROR( xrGetVulkanGraphicsRequirements2KHR( GetAppInstance()->GetXrInstance(), GetAppInstance()->GetXrSystemId(), &m_xrGraphicsRequirements ) );

		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVkInstance( VkResult &outVkResult, const XrInstance xrInstance, const XrVulkanInstanceCreateInfoKHR *xrVulkanInstanceCreateInfo ) 
	{ 
		PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( xrInstance, xrCreateVulkanInstanceKHR ) );

		return xrCreateVulkanInstanceKHR( xrInstance, xrVulkanInstanceCreateInfo, &m_vkInstance, &outVkResult );
	}

	XrResult CVulkan::GetVulkanGraphicsPhysicalDevice() 
	{ 
		assert( m_vkInstance != VK_NULL_HANDLE );

		PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrGetVulkanGraphicsDevice2KHR ) );
		XrVulkanGraphicsDeviceGetInfoKHR info{
			.type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
			.systemId = GetAppInstance()->GetXrSystemId(),
			.vulkanInstance = m_vkInstance
		};

		XR_RETURN_ON_ERROR( xrGetVulkanGraphicsDevice2KHR( GetAppInstance()->GetXrInstance(), &info, &m_vkPhysicalDevice ) );
		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVulkanLogicalDevice( VkSurfaceKHR *pSurface, void *pVkLogicalDeviceNext, void *pXrLogicalDeviceNext ) 
	{ 
		assert( m_vkPhysicalDevice != VK_NULL_HANDLE );

		// Setup device queues ( one for graphics and one for presentation to a surface/mirror if available)
		VkDeviceQueueCreateInfo vkDeviceQueueCI_Graphics { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Graphics = 1.f; // highest priority for rendering to hmd
		vkDeviceQueueCI_Graphics.queueCount = 1;
		vkDeviceQueueCI_Graphics.pQueuePriorities = &fQueuePriority_Graphics;

		VkDeviceQueueCreateInfo vkDeviceQueueCI_Transfer { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Transfer = 0.5f; // mid priority for data transfers
		vkDeviceQueueCI_Transfer.queueCount = 1;
		vkDeviceQueueCI_Transfer.pQueuePriorities = &fQueuePriority_Transfer;

		VkDeviceQueueCreateInfo vkDeviceQueueCI_Present { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Present = 0.f; // lowest priority for rendering to a mirror/surface
		vkDeviceQueueCI_Present.queueCount = 1;
		vkDeviceQueueCI_Present.pQueuePriorities = &fQueuePriority_Present;

		// Retrieve all queue families for the physical device
		uint32_t unQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, nullptr );
		std::vector< VkQueueFamilyProperties > vecQueueFamilyProps( unQueueFamilyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, vecQueueFamilyProps.data() );

		bool bGraphicsFamilyFound = false;
		bool bPresentFamilyFound = false;
		bool bTransferFamilyFound = false;

		for ( uint32_t i = 0; i < unQueueFamilyCount; ++i )
		{
			// Find an appropriate graphics queue
			if ( !bGraphicsFamilyFound && ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0u ) )
			{
				m_vkQueueIndex_GraphicsFamily = vkDeviceQueueCI_Graphics.queueFamilyIndex = i;
				bGraphicsFamilyFound = true;
			}

			// Find an appropriate transfer queue
			if ( !bTransferFamilyFound && ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_TRANSFER_BIT ) != 0u ) )
			{
				m_vkQueueIndex_TransferFamily = vkDeviceQueueCI_Transfer.queueFamilyIndex = i;
				bTransferFamilyFound = true;
			}

			// Find an appropriate present queue
			if ( !bPresentFamilyFound && pSurface )
			{
				vkGetPhysicalDeviceSurfaceSupportKHR( m_vkPhysicalDevice, i, *pSurface, &m_vkSupportsSurfacePresent );
				if ( m_vkSupportsSurfacePresent )
				{
					m_vkQueueIndex_PresentFamily = vkDeviceQueueCI_Present.queueFamilyIndex = i;
					bPresentFamilyFound = true;
				}
			}
			else
			{
				m_vkSupportsSurfacePresent = VK_FALSE;
				if ( bGraphicsFamilyFound && bTransferFamilyFound )
					break;
			}

			if ( bGraphicsFamilyFound && bPresentFamilyFound && bTransferFamilyFound )
				break;
		}

		#if defined( _WIN32 )
			vecLogicalDeviceExtensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
		#endif

		// Setup logical device
		vkPhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
		// VkPhysicalDeviceFeatures2 physical_features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		// physical_features2.features.samplerAnisotropy = VK_TRUE;
		// physical_features2.features.multiViewport = VK_TRUE;
		// vkGetPhysicalDeviceFeatures2( m_SharedState.vkPhysicalDevice, &physical_features2 );

		//// log warning(s) if feature(s) is/are not supported
		//if ( physical_features2.features.samplerAnisotropy == VK_FALSE || m_SharedState.vkPhysicalDeviceFeatures.samplerAnisotropy == VK_FALSE )
		//{
		//	LogWarning( "xrvk", "features.samplerAnisotropy is not available in this gpu, application may not run or render as intended." );
		// }

		// Add multiview (required)
		//vkPhysicalDeviceFeatures.multiViewport = VK_TRUE;
		///vkPhysicalDeviceFeatures.sampleRateShading = VK_TRUE;
		//vecLogicalDeviceExtensions.push_back( VK_KHR_MULTIVIEW_EXTENSION_NAME );

		std::vector< VkDeviceQueueCreateInfo > vecDeviceQueueCIs;
		vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Graphics );

		bool bTransferIsSameAsGraphics = vkDeviceQueueCI_Graphics.queueFamilyIndex == vkDeviceQueueCI_Transfer.queueFamilyIndex;
		if (  !bTransferIsSameAsGraphics )
			vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Transfer );

		bool bPresentIsUnique =	( vkDeviceQueueCI_Graphics.queueFamilyIndex != vkDeviceQueueCI_Present.queueFamilyIndex ) &&
			( vkDeviceQueueCI_Transfer.queueFamilyIndex != vkDeviceQueueCI_Present.queueFamilyIndex );

		if ( m_vkSupportsSurfacePresent && !bPresentIsUnique )
			vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Present );

		VkDeviceCreateInfo vkLogicalDeviceCI { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		vkLogicalDeviceCI.pNext = pVkLogicalDeviceNext; //&physical_features2;
		vkLogicalDeviceCI.queueCreateInfoCount = (uint32_t) vecDeviceQueueCIs.size();
		vkLogicalDeviceCI.pQueueCreateInfos = vecDeviceQueueCIs.data();
		vkLogicalDeviceCI.enabledLayerCount = 0;
		vkLogicalDeviceCI.ppEnabledLayerNames = nullptr;
		vkLogicalDeviceCI.enabledExtensionCount = (uint32_t) vecLogicalDeviceExtensions.size();
		vkLogicalDeviceCI.ppEnabledExtensionNames = vecLogicalDeviceExtensions.empty() ? nullptr : vecLogicalDeviceExtensions.data();
		vkLogicalDeviceCI.pEnabledFeatures = &vkPhysicalDeviceFeatures;

		XrVulkanDeviceCreateInfoKHR xrVulkanDeviceCreateInfo { XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
		xrVulkanDeviceCreateInfo.next = pXrLogicalDeviceNext;
		xrVulkanDeviceCreateInfo.systemId = GetAppInstance()->GetXrSystemId();
		xrVulkanDeviceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
		xrVulkanDeviceCreateInfo.vulkanCreateInfo = &vkLogicalDeviceCI;
		xrVulkanDeviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
		xrVulkanDeviceCreateInfo.vulkanAllocator = nullptr;

		// Create logical device
		VkResult vkResult = VK_SUCCESS;
		XrResult xrResult = CreateVkDevice( vkResult, &xrVulkanDeviceCreateInfo );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) || vkResult != VK_SUCCESS )
		{
			LogError( XRLIB_NAME, "Error creating vulkan device with openxr result (%s) and vulkan result (%i)", xrlib::XrEnumToString( xrResult ), (int32_t) vkResult );
			return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
		}

		LogInfo( XRLIB_NAME, "Vulkan (logical) device successfully created." );

		volkLoadDevice(m_vkDevice);

		// Get device queue(s)
		vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_GraphicsFamily, 0, &m_vkQueue_Graphics );
		
		if ( !bTransferIsSameAsGraphics )
		{
			vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_TransferFamily, 0, &m_vkQueue_Transfer );
		}
		else
		{
			m_vkQueue_Transfer = m_vkQueue_Graphics;
		}
		

		if ( m_vkSupportsSurfacePresent )
		{
			if ( !bPresentIsUnique )
				m_vkQueue_Present = m_vkQueue_Graphics;
			else
				vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_PresentFamily, 0, &m_vkQueue_Present );
		}

		// Create graphics binding that we will use to create an openxr session
		m_xrGraphicsBinding.instance = m_vkInstance;
		m_xrGraphicsBinding.physicalDevice = m_vkPhysicalDevice;
		m_xrGraphicsBinding.device = m_vkDevice;
		m_xrGraphicsBinding.queueFamilyIndex = m_vkQueueIndex_GraphicsFamily;
		m_xrGraphicsBinding.queueIndex = m_vkQueueIndex_Graphics;

		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVkDevice( VkResult &outVkResult, const XrVulkanDeviceCreateInfoKHR *xrVulkanDeviceCreateInfo ) 
	{ 
		assert( xrVulkanDeviceCreateInfo );
		assert( m_vkInstance != VK_NULL_HANDLE );
		assert( m_vkPhysicalDevice != VK_NULL_HANDLE );

		PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrCreateVulkanDeviceKHR ) );

		return xrCreateVulkanDeviceKHR( GetAppInstance()->GetXrInstance(), xrVulkanDeviceCreateInfo, &m_vkDevice, &outVkResult );
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL CVulkan::Callback_Debug(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData )
	{
		if ( messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
		{
			LogDebug( "Vulkan", "%s", pCallbackData->pMessage );
		}

		return VK_FALSE;
	}

	const bool CVulkan::IsDepthFormat( VkFormat vkFormat ) 
	{ 
		if ( vkFormat == VK_FORMAT_D16_UNORM || 
			 vkFormat == VK_FORMAT_X8_D24_UNORM_PACK32 || 
			 vkFormat == VK_FORMAT_D32_SFLOAT || 
			 vkFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
			 vkFormat == VK_FORMAT_D24_UNORM_S8_UINT || 
			 vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT )
		{
			return true;
		}

		return false;
	}

	const bool CVulkan::IsStencilFormat( VkFormat vkFormat )
	{
		if ( vkFormat == VK_FORMAT_D32_SFLOAT || 
			 vkFormat == VK_FORMAT_D16_UNORM_S8_UINT || 
			 vkFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
			 vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT )
		{
			return true;
		}

		return false;
	}


}

