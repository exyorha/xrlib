/* 
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <xrlib/session.hpp>
#include <xrlib/vulkan.hpp>

namespace xrlib
{
	class CDeviceBuffer
	{
	  public:


		CDeviceBuffer( CSession *pSession );
		~CDeviceBuffer();

		VkResult Init( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData = nullptr, VkAllocationCallbacks *pCallbacks = nullptr );
		uint32_t FindMemoryType( VkMemoryPropertyFlags memPropFlags, uint32_t unBits );
		VkResult FlushMemory( VkDeviceSize unSize = VK_WHOLE_SIZE, VkDeviceSize unOffset = 0, uint32_t unRangeCount = 1 );
		void UnmapMemory();
		
		VkPhysicalDevice GetPhysicalDevice();
		VkDevice GetLogicalDevice();

		VkBuffer GetVkBuffer() { return m_vkBufferInfo.buffer; }
		VkBuffer *GetVkBufferPtr() { return &m_vkBufferInfo.buffer; };
		VkDescriptorBufferInfo *GetBufferInfo() { return &m_vkBufferInfo; }

	  private:
		CSession *m_pSession = nullptr;

		VkDescriptorBufferInfo m_vkBufferInfo { VK_NULL_HANDLE, 0, 0 };
		VkDeviceMemory m_vkDeviceMemory = VK_NULL_HANDLE;
		VkDeviceSize m_vkMemoryAlignment = 0;
		VkDeviceSize m_vkMemorySize = VK_WHOLE_SIZE;

		void *m_pData = nullptr;
	};

}
