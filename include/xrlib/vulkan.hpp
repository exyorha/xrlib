/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <xrlib/session.hpp>

namespace xrlib
{
	class CVulkan
	{
	  public:
		CVulkan( CSession* pSession );
		~CVulkan();

		VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures {};

		std::vector< const char * > vecExtensions;
		std::vector< const char * > vecLayers;
		std::vector< const char * > vecLogicalDeviceExtensions;

		#ifdef XRVK_VULKAN_VALIDATION_ENABLE
			const std::vector< const char * > m_vecValidationLayers = { "VK_LAYER_KHRONOS_validation" };
			const std::vector< const char * > m_vecValidationExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
		#else
			const std::vector< const char * > m_vecValidationLayers;
			const std::vector< const char * > m_vecValidationExtensions;
		#endif
			          
		XrResult Init( 
			VkSurfaceKHR *pSurface = nullptr,                                                   
			void *pVkInstanceNext = nullptr, 
			void *pXrVkInstanceNext = nullptr, 
			void *pVkLogicalDeviceNext = nullptr, 
			void *pXrLogicalDeviceNext = nullptr );

		static VKAPI_ATTR VkBool32 VKAPI_CALL Callback_Debug(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
			void *pUserData );

		const bool IsDepthFormat( VkFormat vkFormat );
		const bool IsStencilFormat( VkFormat vkFormat );

		CSession *GetAppSession() { return m_pSession;  }
		CInstance *GetAppInstance() { return m_pSession->m_pInstance;  }
		
		XrGraphicsRequirementsVulkan2KHR *GetGraphicsRequirements() { return &m_xrGraphicsRequirements; }
		XrGraphicsBindingVulkan2KHR *GetGraphicsBinding() { return &m_xrGraphicsBinding; }

		const VkBool32 GetSupportsSurfacePresent() { return m_vkSupportsSurfacePresent; }

		const VkInstance GetVkInstance() { return m_vkInstance; }
		const VkPhysicalDevice GetVkPhysicalDevice() { return m_vkPhysicalDevice; }
		const VkDevice GetVkLogicalDevice() { return m_vkDevice; }

		const VkQueue GetVkQueue_Graphics(){ return m_vkQueue_Graphics; }
		const uint32_t GetVkQueueIndex_GraphicsFamily() { return m_vkQueueIndex_GraphicsFamily; }
		const uint32_t GetVkQueueIndex_Graphics() { return m_vkQueueIndex_Graphics; }

		const VkQueue GetVkQueue_Transfer() { return m_vkQueue_Transfer; }
		const uint32_t GetVkQueueIndex_TransferFamily() { return m_vkQueueIndex_TransferFamily; }
		const uint32_t GetVkQueueIndex_Transfer() { return m_vkQueueIndex_Transfer; }

		const VkQueue GetVkQueue_Present() { return m_vkQueue_Present; }
		const uint32_t GetVkQueueIndex_PresentFamily() { return m_vkQueueIndex_PresentFamily; }
		const uint32_t GetVkQueueIndex_Present() { return m_vkQueueIndex_Present; }

	  private:
		CSession *m_pSession = nullptr;

		XrGraphicsRequirementsVulkan2KHR m_xrGraphicsRequirements { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };
		XrGraphicsBindingVulkan2KHR m_xrGraphicsBinding { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };

		VkBool32 m_vkSupportsSurfacePresent = VK_FALSE;

		VkInstance m_vkInstance = VK_NULL_HANDLE;
		VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_vkDevice = VK_NULL_HANDLE;

		VkQueue m_vkQueue_Graphics = VK_NULL_HANDLE;
		uint32_t m_vkQueueIndex_GraphicsFamily = 0;
		uint32_t m_vkQueueIndex_Graphics = 0;

		VkQueue m_vkQueue_Transfer = VK_NULL_HANDLE;
		uint32_t m_vkQueueIndex_TransferFamily = 0;
		uint32_t m_vkQueueIndex_Transfer = 0;

		VkQueue m_vkQueue_Present = VK_NULL_HANDLE;
		uint32_t m_vkQueueIndex_PresentFamily = 0;
		uint32_t m_vkQueueIndex_Present = 0;

		XrResult GetVulkanGraphicsRequirements();
		XrResult CreateVkInstance( VkResult &outVkResult, const XrInstance xrInstance, const XrVulkanInstanceCreateInfoKHR *xrVulkanInstanceCreateInfo );
		XrResult GetVulkanGraphicsPhysicalDevice();
		XrResult CreateVulkanLogicalDevice( VkSurfaceKHR *pSurface, void *pVkLogicalDeviceNext, void *pXrLogicalDeviceNext );
		XrResult CreateVkDevice( VkResult &outVkResult, const XrVulkanDeviceCreateInfoKHR *xrVulkanDeviceCreateInfo );
	};

}