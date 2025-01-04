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


#pragma once

#include <xrlib/vulkan.hpp>

namespace vkutils
{
	VkCommandBuffer BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool );

	void EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer );

	uint32_t FindMemoryType( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties );

	uint32_t FindMemoryTypeWithFallback( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties );

	VkResult CreateImage( 
		VkImage &outImage, 
		VkDeviceMemory &outImageMemory,
		VkDevice device, 
		VkPhysicalDevice physicalDevice, 
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties );

	VkResult CreateImageView( VkImageView &outImageView, VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags );

	VkResult CreateSampler( VkSampler &outSampler, VkDevice device, VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode );

	void UploadTextureDataToImage( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, const std::vector< uint8_t > &imageData, uint32_t width, uint32_t height, VkFormat format );

	void TransitionImageLayout(
		VkDevice device,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout );

	void TransitionImageLayout(
		VkDevice device,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkImageAspectFlags aspectMask,
		uint32_t baseArrayLayer = 0,
		uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS );

	void CopyBufferToImage( 
		VkDevice device, 
		VkCommandPool commandPool, 
		VkQueue graphicsQueue, 
		VkBuffer buffer, 
		VkImage image, 
		uint32_t width, 
		uint32_t height );

} // namespace vkutils