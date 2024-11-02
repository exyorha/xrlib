/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <xrvk/render.hpp>

#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
	#define RENDERDOC_FRAME_SAMPLES 5
	#include <xrvk/renderdoc_api.h>
	#include <Windows.h>

	RENDERDOC_API_1_1_2 *rdoc_api = NULL;
#endif

namespace xrlib
{
	SShader::~SShader() 
	{
		if ( m_vkLogicalDevice != VK_NULL_HANDLE && m_vkShaderModule != VK_NULL_HANDLE )
			vkDestroyShaderModule( m_vkLogicalDevice, m_vkShaderModule, nullptr );
	}

	#ifdef XR_USE_PLATFORM_ANDROID
		VkPipelineShaderStageCreateInfo SShader::Init(
			AAssetManager *assetManager,
			VkDevice vkLogicalDevice, 
			VkShaderStageFlagBits shaderStage,
			VkShaderModuleCreateFlags createFlags,
			void *pNext )
	#else
	VkPipelineShaderStageCreateInfo SShader::Init( 
		VkDevice vkLogicalDevice, 
		VkShaderStageFlagBits shaderStage, 
		VkShaderModuleCreateFlags createFlags, 
		void *pNext ) 
	#endif
	{ 
		assert( vkLogicalDevice != VK_NULL_HANDLE );
		assert( shaderStage != 0 );

        m_vkLogicalDevice = vkLogicalDevice;

		// Read shader from disk
		#ifdef XR_USE_PLATFORM_ANDROID	
			auto spirvShaderCode = ReadBinaryFile( assetManager, m_sFilename );
		#else
			auto spirvShaderCode = ReadBinaryFile( m_sFilename );
		#endif

		// Create shader module
		VkShaderModuleCreateInfo shaderModuleCI { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		shaderModuleCI.pNext = pNext;
		shaderModuleCI.flags = createFlags;
		shaderModuleCI.codeSize = spirvShaderCode.size();
		shaderModuleCI.pCode = reinterpret_cast< const uint32_t * >( spirvShaderCode.data() );

		if ( vkCreateShaderModule( vkLogicalDevice, &shaderModuleCI, nullptr, &m_vkShaderModule ) != VK_SUCCESS )
			throw std::runtime_error( "failed to create shader module!" );

		// Assemble shader stage
		VkPipelineShaderStageCreateInfo shaderStageCI { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		shaderStageCI.pNext = nullptr;
		shaderStageCI.flags = 0;
		shaderStageCI.stage = shaderStage;
		shaderStageCI.module = m_vkShaderModule;
		shaderStageCI.pName = m_sEntrypoint.c_str();

		return shaderStageCI; 
	}

	SShaderSet::SShaderSet( 
		std::string sVertexShaderFilename, 
		std::string sFragmentShaderFilename, 
		std::string sVertexShaderEntrypoint, 
		std::string sFragmentShaderEntrypoint ) 
	{
		assert( !sVertexShaderFilename.empty() );
		assert( !sFragmentShaderFilename.empty() );
		assert( !sVertexShaderEntrypoint.empty() );
		assert( !sFragmentShaderEntrypoint.empty() );

		vertexShader = new SShader( sVertexShaderFilename, sVertexShaderEntrypoint );
		fragmentShader = new SShader( sFragmentShaderFilename, sFragmentShaderEntrypoint );
	}

	SShaderSet::~SShaderSet() 
	{
		if ( vertexShader )
			delete vertexShader;

		if ( fragmentShader )
			delete fragmentShader;
	}

	#ifdef XR_USE_PLATFORM_ANDROID

	void SShaderSet::Init(
		AAssetManager *assetManager,
		VkDevice vkLogicalDevice,
		VkShaderStageFlagBits vertexShaderStage,
		VkShaderStageFlagBits fragmentShaderStage,
		VkShaderModuleCreateFlags vertexShaderCreateFlags,
		VkShaderModuleCreateFlags fragmentShaderCreateFlags,
		void *pVertexNext,
		void *pFragmentNext )
	{
		stages.clear();

		stages.push_back( vertexShader->Init( assetManager, vkLogicalDevice, vertexShaderStage, vertexShaderCreateFlags, pVertexNext ) );
		stages.push_back( fragmentShader->Init( assetManager, vkLogicalDevice, fragmentShaderStage, fragmentShaderCreateFlags, pFragmentNext ) );
	}

	#else

	void SShaderSet::Init(
		VkDevice vkLogicalDevice,
		VkShaderStageFlagBits vertexShaderStage,
		VkShaderStageFlagBits fragmentShaderStage,
		VkShaderModuleCreateFlags vertexShaderCreateFlags,
		VkShaderModuleCreateFlags fragmentShaderCreateFlags,
		void *pVertexNext,
		void *pFragmentNext )
	{ 
		#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
			if ( HMODULE mod = GetModuleHandleA( "renderdoc.dll" ) )
			{
				pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( mod, "RENDERDOC_GetAPI" );
				int ret = RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void **) &rdoc_api );
				assert( ret == 1 );
			}
		#endif

		stages.clear();

		stages.push_back( vertexShader->Init( vkLogicalDevice, vertexShaderStage, vertexShaderCreateFlags, pVertexNext ) );
		stages.push_back( fragmentShader->Init( vkLogicalDevice, fragmentShaderStage, fragmentShaderCreateFlags, pFragmentNext ) );
	}

	#endif

	CStereoRender::CStereoRender( CSession *pSession, VkFormat vkColorFormat, VkFormat vkDepthFormat )
		: m_pSession( pSession), 
		  m_vkColorFormat( vkColorFormat ),
		  m_vkDepthFormat( vkDepthFormat ) 
	{
		// Validate session
		assert( pSession );
		assert( pSession->GetXrSession() != XR_NULL_HANDLE );

		// Validate session's current view configuration
		assert( pSession->xrViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO );

		// Validate texture formats
		std::vector< int64_t > supportedFormats;
		assert ( XR_UNQUALIFIED_SUCCESS( pSession->GetSupportedTextureFormats( supportedFormats ) ) );

		bool bFoundColor = false;
		bool bFoundDepth = false;

		for ( auto &supportedFormat : supportedFormats )
		{
			if ( bFoundColor && bFoundDepth )
				break;

			if ( !bFoundColor && ( (VkFormat) supportedFormat == vkColorFormat ) )
				bFoundColor = true;

			if ( !bFoundDepth && ( (VkFormat) supportedFormat == vkDepthFormat ) )
				bFoundDepth = true;
		}

		assert( bFoundColor && bFoundDepth );
		assert( !pSession->GetVulkan()->IsDepthFormat( vkColorFormat ) );
		assert( pSession->GetVulkan()->IsDepthFormat( vkDepthFormat ) );
	};

	CStereoRender::~CStereoRender() 
	{
		if ( GetLogicalDevice() != VK_NULL_HANDLE )
		{
			// Destroy render views
			for ( auto &renderTarget : m_vecMultiviewRenderTargets )
			{
				if ( renderTarget.vkColorImageDescriptor.imageView != VK_NULL_HANDLE )
					vkDestroyImageView( GetLogicalDevice(), renderTarget.vkColorImageDescriptor.imageView, nullptr );

				if ( renderTarget.vkDepthImageView != VK_NULL_HANDLE )
					vkDestroyImageView(GetLogicalDevice(), renderTarget.vkDepthImageView, nullptr );
			}

			// Destroy render pass
			for ( auto renderPass : vecRenderPasses )
			{
				if ( renderPass != VK_NULL_HANDLE )
					vkDestroyRenderPass( GetLogicalDevice(), renderPass, nullptr );
			}

			// Destroy samplers, frame buffers and fences
			for ( auto &renderTarget : m_vecMultiviewRenderTargets )
			{
				if ( renderTarget.vkColorImageDescriptor.sampler != VK_NULL_HANDLE )
					vkDestroySampler( GetLogicalDevice(), renderTarget.vkColorImageDescriptor.sampler, nullptr );

				if ( renderTarget.vkFrameBuffer != VK_NULL_HANDLE )
					vkDestroyFramebuffer( GetLogicalDevice(), renderTarget.vkFrameBuffer, nullptr );

				if ( renderTarget.vkRenderCommandFence != VK_NULL_HANDLE )
					vkDestroyFence( GetLogicalDevice(), renderTarget.vkRenderCommandFence, nullptr );
			}
		}

		if ( m_xrColorSwapchain != XR_NULL_HANDLE )
			xrDestroySwapchain( m_xrColorSwapchain );

		if ( m_xrDepthSwapchain != XR_NULL_HANDLE )
			xrDestroySwapchain( m_xrDepthSwapchain );

		if ( m_vkTransferCommandPool != VK_NULL_HANDLE )
			vkDestroyCommandPool( GetLogicalDevice(), m_vkTransferCommandPool, nullptr );

		if ( m_vkRenderCommandPool != VK_NULL_HANDLE )
			vkDestroyCommandPool( GetLogicalDevice(), m_vkRenderCommandPool, nullptr );
	}

	XrResult CStereoRender::Init( uint32_t unTextureFaceCount, uint32_t unTextureMipCount ) 
	{ 
		// Check if openxr session is valid
		if ( m_pSession->GetXrSession() == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Create swapchains
		XR_RETURN_ON_ERROR( CreateSwapchains( unTextureFaceCount, unTextureMipCount ) );
		XR_RETURN_ON_ERROR( CreateSwapchainImages( m_vecSwapchainColorImages, m_xrColorSwapchain ) );
		XR_RETURN_ON_ERROR( CreateSwapchainImages( m_vecSwapchainDepthImages, m_xrDepthSwapchain ) );

		// Create command pools
		VkCommandPoolCreateInfo commandPoolCI { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		commandPoolCI.queueFamilyIndex = GetAppSession()->GetVulkan()->GetVkQueueIndex_GraphicsFamily();
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VkResult result = vkCreateCommandPool( GetLogicalDevice(), &commandPoolCI, nullptr, &m_vkRenderCommandPool );
		assert( result == VK_SUCCESS );

		commandPoolCI.queueFamilyIndex = GetAppSession()->GetVulkan()->GetVkQueueIndex_TransferFamily();
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		result = vkCreateCommandPool( GetLogicalDevice(), &commandPoolCI, nullptr, &m_vkTransferCommandPool );
		assert( result == VK_SUCCESS );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateSwapchains( uint32_t unFaceCount, uint32_t unMipCount ) 
	{ 
		assert( m_pSession->GetVulkan()->GetVkPhysicalDevice() != VK_NULL_HANDLE );

		// (1) Update eye view configurations
		uint32_t unEyeConfigNum = 0;
		m_vecEyeConfigs.clear();

		// Get number of configuration views for this session
		XR_RETURN_ON_ERROR( xrEnumerateViewConfigurationViews( 
			GetAppInstance()->GetXrInstance(), 
			GetAppInstance()->GetXrSystemId(), 
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unEyeConfigNum, 
			&unEyeConfigNum, 
			nullptr ) );

		if ( unEyeConfigNum != k_EyeCount )
			return XR_ERROR_VALIDATION_FAILURE;

		// Update eye view configurations
		m_vecEyeConfigs.resize( unEyeConfigNum, { XR_TYPE_VIEW_CONFIGURATION_VIEW } );
		XR_RETURN_ON_ERROR( xrEnumerateViewConfigurationViews( 
			GetAppInstance()->GetXrInstance(), 
			GetAppInstance()->GetXrSystemId(), 
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unEyeConfigNum, 
			&unEyeConfigNum, 
			m_vecEyeConfigs.data() ) );


		// (2) Create eye views that will contain up to date fov, pose, etc
		m_vecEyeViews.resize( unEyeConfigNum, { XR_TYPE_VIEW } );

		// (3) Validate image array support
		VkPhysicalDeviceProperties vkPhysicalDeviceProps;
		vkGetPhysicalDeviceProperties( m_pSession->GetVulkan()->GetVkPhysicalDevice(), &vkPhysicalDeviceProps );
			
		if ( vkPhysicalDeviceProps.limits.maxImageArrayLayers < k_EyeCount )
		{
			LogError( "", "Device does not support image arrays." );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		// (4) Create color swapchain
		XrSwapchainCreateInfo xrSwapchainCreateInfo { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		xrSwapchainCreateInfo.format = m_vkColorFormat;
		xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrSwapchainCreateInfo.arraySize = k_EyeCount;
		xrSwapchainCreateInfo.width = m_unTextureWidth = m_vecEyeConfigs[ 0 ].recommendedImageRectWidth;
		xrSwapchainCreateInfo.height = m_unTextureHeight = m_vecEyeConfigs[ 0 ].recommendedImageRectHeight;
		xrSwapchainCreateInfo.mipCount = unMipCount;
		xrSwapchainCreateInfo.faceCount = unFaceCount;
		xrSwapchainCreateInfo.sampleCount = m_vecEyeConfigs[ 0 ].recommendedSwapchainSampleCount;

		XR_RETURN_ON_ERROR( xrCreateSwapchain( m_pSession->GetXrSession(), &xrSwapchainCreateInfo, &m_xrColorSwapchain ) );

		// (5) Create depth swapchain
		xrSwapchainCreateInfo.format = m_vkDepthFormat;
		xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		XR_RETURN_ON_ERROR( xrCreateSwapchain( m_pSession->GetXrSession(), &xrSwapchainCreateInfo, &m_xrDepthSwapchain ) );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateSwapchainImages( std::vector< XrSwapchainImageVulkan2KHR > &outSwapchainImages, XrSwapchain xrSwapchain ) 
	{ 
		// Validate swpachain
		if ( xrSwapchain == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Determine number of swapchain images generated by the runtime
		uint32_t unNumOfSwapchainImages = 0;
		XR_RETURN_ON_ERROR( xrEnumerateSwapchainImages( xrSwapchain, unNumOfSwapchainImages, &unNumOfSwapchainImages, nullptr ) );

		// Create swapchain images
		outSwapchainImages.clear();
		outSwapchainImages.resize( unNumOfSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR } );

		XR_RETURN_ON_ERROR ( xrEnumerateSwapchainImages(
			xrSwapchain,
			unNumOfSwapchainImages,
			&unNumOfSwapchainImages, 
			reinterpret_cast< XrSwapchainImageBaseHeader * >( outSwapchainImages.data() ) ) );

		// Log swapchain creation 
		if ( CheckLogLevelVerbose( GetMinLogLevel() ) )
			LogVerbose( "", "Swapchain created with %i images.", unNumOfSwapchainImages );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateRenderPass( VkRenderPass &outRenderPass, bool bUseVisMask ) 
	{ 
		// (1) Setup render pass artifacts - render targets
		XR_RETURN_ON_ERROR( InitRendering_Multiview() );

		// (2) Create render pass
		XR_RETURN_ON_ERROR( CreateRenderPass_Multiview( bUseVisMask ) );
		outRenderPass = vecRenderPasses.back();

		// (3) Create frame buffers for this render pass
		XR_RETURN_ON_ERROR( CreateFramebuffers_Multiview( outRenderPass ) );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::InitRendering_Multiview() 
	{ 
		XR_RETURN_ON_ERROR( CreateRenderTargets_Multiview() );

		VkSamplerCreateInfo samplerCI = GenerateImageSamplerCI();
		XR_RETURN_ON_ERROR( CreateRenderTargetSamplers( samplerCI ) ); 

		return XR_SUCCESS; 
	}

	XrResult CStereoRender::CreateRenderTargets_Multiview( 
		VkImageViewCreateFlags colorCreateFlags, 
		VkImageViewCreateFlags depthCreateFlags, 
		void *pColorNext,
		void *pDepthNext,
		VkAllocationCallbacks *pCallbacks ) 
	{ 
		// Call order validation
		if ( m_vecSwapchainColorImages.empty() || m_vecSwapchainDepthImages.empty() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( m_vkRenderCommandPool == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Swapchain image count
		uint32_t unSwapchainImageCount = (uint32_t) m_vecSwapchainColorImages.size();
		if ( unSwapchainImageCount != (uint32_t) m_vecSwapchainDepthImages.size() )
		{
			LogError( "", "Error creating multiview render targets: color & depth swpachain must be of equal length!" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		// Create render targets
		for ( uint32_t i = 0; i < unSwapchainImageCount; i++ )
		{
			m_vecMultiviewRenderTargets.push_back( {} );
			m_vecMultiviewRenderTargets.back().vkColorTexture = m_vecSwapchainColorImages[ i ].image;

			// Color image view
			VkImageViewCreateInfo imageViewCI { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			imageViewCI.pNext = pColorNext;
			imageViewCI.flags = colorCreateFlags;
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			imageViewCI.format = m_vkColorFormat;

			imageViewCI.subresourceRange = {};
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCI.subresourceRange.baseMipLevel = 0;
			imageViewCI.subresourceRange.levelCount = 1;
			imageViewCI.subresourceRange.baseArrayLayer = 0;
			imageViewCI.subresourceRange.layerCount = k_EyeCount;

			imageViewCI.image = m_vecMultiviewRenderTargets.back().vkColorTexture;
			
			VkResult result = vkCreateImageView( 
				GetLogicalDevice(), 
				&imageViewCI, 
				pCallbacks, 
				&m_vecMultiviewRenderTargets.back().vkColorImageDescriptor.imageView );
			assert( result == VK_SUCCESS );

			// Depth image view
			m_vecMultiviewRenderTargets.back().vkDepthTexture = m_vecSwapchainDepthImages[ i ].image;

			imageViewCI.pNext = NULL;
			imageViewCI.flags = depthCreateFlags;
			imageViewCI.format = m_vkDepthFormat;
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

			imageViewCI.image = m_vecMultiviewRenderTargets.back().vkDepthTexture;

			result = vkCreateImageView( 
				GetLogicalDevice(), 
				&imageViewCI, 
				pCallbacks, 
				&m_vecMultiviewRenderTargets.back().vkDepthImageView );
			assert( result == VK_SUCCESS );

			// Command buffers
			VkCommandBufferAllocateInfo commandBufferAlloc { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			commandBufferAlloc.pNext = nullptr;
			commandBufferAlloc.commandPool = m_vkRenderCommandPool;
			commandBufferAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAlloc.commandBufferCount = 1;
			result = vkAllocateCommandBuffers( GetLogicalDevice(), &commandBufferAlloc, &m_vecMultiviewRenderTargets.back().vkRenderCommandBuffer );
			assert( result == VK_SUCCESS );

			commandBufferAlloc.commandPool = m_vkTransferCommandPool;
			result = vkAllocateCommandBuffers( GetLogicalDevice(), &commandBufferAlloc, &m_vecMultiviewRenderTargets.back().vkTransferCommandBuffer );
			assert( result == VK_SUCCESS );

			// Fences
			VkFenceCreateInfo fenceCI { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			result = vkCreateFence( GetLogicalDevice(), &fenceCI, nullptr, &m_vecMultiviewRenderTargets.back().vkRenderCommandFence );
			assert( result == VK_SUCCESS );

			result = vkCreateFence( GetLogicalDevice(), &fenceCI, nullptr, &m_vecMultiviewRenderTargets.back().vkTransferCommandFence );
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateRenderTargetSamplers( VkSamplerCreateInfo &samplerCI, VkAllocationCallbacks *pCallbacks ) 
	{ 
		if ( m_vecMultiviewRenderTargets.size() != m_vecSwapchainColorImages.size() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( samplerCI.sType != VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO )
			return XR_ERROR_VALIDATION_FAILURE;

		for ( auto &renderTarget : m_vecMultiviewRenderTargets )
		{
			VkResult result = vkCreateSampler( GetLogicalDevice(), &samplerCI, pCallbacks, &renderTarget.vkColorImageDescriptor.sampler );
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS; 
	}

	XrResult CStereoRender::CreateRenderPass_Multiview( bool bUseVisMask )
	{
		if ( m_vecMultiviewRenderTargets.size() != m_vecSwapchainColorImages.size() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		m_bUseVisMask = bUseVisMask && m_pSession->GetVulkan()->IsStencilFormat( m_vkDepthFormat );

		// (1) Define Attachment Descriptions (Color, Depth-Stencil)
		std::vector< VkAttachmentDescription > vecAttachmentDescriptions;

		// Color attachment (for final output)
		VkAttachmentDescription colorAttachment = GenerateColorAttachmentDescription();
		vecAttachmentDescriptions.push_back( colorAttachment );

		// Depth (+stencil) attachment
		VkAttachmentDescription depthStencilAttachment = GenerateDepthAttachmentDescription();
		vecAttachmentDescriptions.push_back( depthStencilAttachment );

		// (2) Define Subpass Descriptions
		std::vector< VkSubpassDescription > vecSubpassDescriptions;

		if ( m_bUseVisMask ) // If visibility mask is enabled, create a stencil subpass
		{
			// Left eye stencil setup (no color output, writes to stencil buffer)
			VkAttachmentReference stencilReference = {};
			stencilReference.attachment = 1; // Index of depth-stencil attachment
			stencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription stencilSubpass = {};
			stencilSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			stencilSubpass.colorAttachmentCount = 0;					// No color output
			stencilSubpass.pDepthStencilAttachment = &stencilReference; // Use depth-stencil attachment for stencil
			vecSubpassDescriptions.push_back( stencilSubpass );

			// Right eye stencil setup
			vecSubpassDescriptions.push_back( stencilSubpass );
		}

		// Main subpass (depth & color render)
		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0; // Index of color attachment
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthStencilReference = {};
		depthStencilReference.attachment = 1; // Index of depth-stencil attachment
		depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription mainSubpass = {};
		mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		mainSubpass.colorAttachmentCount = 1;
		mainSubpass.pColorAttachments = &colorReference;
		mainSubpass.pDepthStencilAttachment = &depthStencilReference; // Use both depth and stencil for testing
		vecSubpassDescriptions.push_back( mainSubpass );

		// (3) Define Subpass Dependencies
		std::vector< VkSubpassDependency > vecSubpassDependencies;

		if ( m_bUseVisMask ) 
		{
			// External dependency before the render pass (from VK_SUBPASS_EXTERNAL to subpass 0)
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().dstSubpass = 0; // First subpass (stencil setup)
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Stencil write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Internal dependency (between left eye stencil subpass and right eye stencil subpass)
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = 0;																			  // First subpass (stencil setup)
			vecSubpassDependencies.back().dstSubpass = 1;																			  // Second subpass (main rendering)
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;									  // Write to depth/stencil
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Color output stage
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;								  // Stencil write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color attachment read/write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Internal dependency (between right eye stencil subpass and main rendering subpass)
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = 1;																			  // Second stencil subpass (right eye)
			vecSubpassDependencies.back().dstSubpass = 2;																			  // Main rendering subpass
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;									  // Write to depth/stencil
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Color output stage
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;								  // Stencil write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color attachment read/write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// External dependency after the render pass (from main rendering subpass to VK_SUBPASS_EXTERNAL)
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = 2; // Last subpass (main rendering)
			vecSubpassDependencies.back().dstSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Final color output stage
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;										  // Synchronize with external operations
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;												  // Read after the render pass
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}
		else // Dependencies for a single subpass
		{
			// External dependency before the render pass
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().dstSubpass = 0;	// Single subpass
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Wait for memory to be ready
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;	 // Color output and depth/stencil tests
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Read memory before render pass
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Read/write for color and depth/stencil
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// External dependency after the render pass
			vecSubpassDependencies.push_back( {} );
			vecSubpassDependencies.back().srcSubpass = 0; // Single subpass
			vecSubpassDependencies.back().dstSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;	 // Color output and depth/stencil tests
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Synchronize with external operations
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Read/write for color and depth/stencil
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Read after the render pass
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		// (4) Create the Render Pass
		VkRenderPassCreateInfo renderPassCI = GenerateRenderPassCI( vecAttachmentDescriptions, vecSubpassDescriptions, vecSubpassDependencies );

		// (5) Add Multiview to Render Pass
		VkRenderPassMultiviewCreateInfo multiviewCI = GenerateMultiviewCI();
		renderPassCI.pNext = &multiviewCI;

		// (6) Adjustments to multiview if vismask is enabled
		if ( m_bUseVisMask )
		{
			std::array< uint32_t, 3 > subpassViewMasks = {
				this->k_unStereoViewMask,
				this->k_unStereoViewMask,
				this->k_unStereoViewMask,
			};
			multiviewCI.pViewMasks = subpassViewMasks.data();
			multiviewCI.subpassCount = 3;
		}

		// (7) Create the Render Pass
		VkResult result = AddRenderPass( &renderPassCI );
		if ( !VK_CHECK_SUCCESS( result ) )
		{
			LogError( LOG_CATEGORY_DEFAULT, "Error creating render pass!" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateFramebuffers_Multiview( VkRenderPass vkRenderPass ) 
	{ 
		assert( vkRenderPass != VK_NULL_HANDLE );

		if ( m_vecMultiviewRenderTargets.empty() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		for ( auto &renderTarget : m_vecMultiviewRenderTargets )
		{
			std::array< VkImageView, 2 > arrImageViews { VK_NULL_HANDLE, VK_NULL_HANDLE };
			renderTarget.SetImageViewArray( arrImageViews );

			VkFramebufferCreateInfo frameBufferCI = GenerateMultiviewFrameBufferCI( arrImageViews, vkRenderPass );
			VkResult result = vkCreateFramebuffer( GetLogicalDevice(), &frameBufferCI, nullptr, &renderTarget.vkFrameBuffer);
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS; 
	}

	VkAttachmentDescription CStereoRender::GenerateColorAttachmentDescription() 
	{ 
		VkAttachmentDescription outDesc {};
		outDesc.format = m_vkColorFormat;
		outDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		outDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		outDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		outDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		outDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		return outDesc;
	}

	VkAttachmentDescription CStereoRender::GenerateDepthAttachmentDescription()
	{
		VkAttachmentDescription outDesc {};
		outDesc.format = m_vkDepthFormat;
		outDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		outDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

		if ( m_bUseVisMask )
		{
			// Stencil operations are required when using a visibility mask.
			outDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		else
		{
			// No stencil operations needed in this case
			outDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
		}

		outDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		outDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		return outDesc;
	}

	VkSubpassDescription CStereoRender::GenerateSubpassDescription() 
	{ 
		VkSubpassDescription outDesc {};
		outDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		outDesc.pDepthStencilAttachment = GetDepthAttachmentReference();
		outDesc.colorAttachmentCount = 1;
		outDesc.pColorAttachments = GetColorAttachmentReference();

		return outDesc;
	}

	VkRenderPassMultiviewCreateInfo CStereoRender::GenerateMultiviewCI() 
	{ 
		VkRenderPassMultiviewCreateInfo multiviewCI { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
		multiviewCI.pNext = nullptr;
		multiviewCI.subpassCount = 1;
		
		multiviewCI.pViewMasks = &this->k_unStereoViewMask;
		multiviewCI.correlationMaskCount = 1;
		multiviewCI.pCorrelationMasks = &this->k_unStereoConcurrentMask;

		multiviewCI.dependencyCount = 0;
		multiviewCI.pViewOffsets = nullptr;

		return multiviewCI;
	}

	VkRenderPassCreateInfo
		CStereoRender::GenerateRenderPassCI( 
			std::vector< VkAttachmentDescription > &vecAttachmentDescriptions, 
			std::vector< VkSubpassDescription > &vecSubpassDescriptions, 
			std::vector< VkSubpassDependency > &vecSubpassDependencies,
			const VkRenderPassCreateFlags vkCreateFlags,
			const void *pNext ) 
	{ 
		VkRenderPassCreateInfo renderPassCI { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		renderPassCI.pNext = pNext;
		renderPassCI.flags = vkCreateFlags;
		renderPassCI.attachmentCount = vecAttachmentDescriptions.empty() ? 0 : (uint32_t) vecAttachmentDescriptions.size();
		renderPassCI.pAttachments = vecAttachmentDescriptions.empty() ? nullptr : vecAttachmentDescriptions.data();
		renderPassCI.subpassCount = vecSubpassDescriptions.empty() ? 0 : (uint32_t) vecSubpassDescriptions.size();
		renderPassCI.pSubpasses = vecSubpassDescriptions.empty() ? nullptr : vecSubpassDescriptions.data();
		renderPassCI.dependencyCount = vecSubpassDependencies.empty() ? 0 : (uint32_t) vecSubpassDependencies.size();
		renderPassCI.pDependencies = vecSubpassDependencies.empty() ? nullptr : vecSubpassDependencies.data();

		return renderPassCI;
	}

	VkFramebufferCreateInfo	CStereoRender::GenerateMultiviewFrameBufferCI( 
			std::array< VkImageView, 2 > &arrImageViews,
			VkRenderPass vkRenderPass, 
			VkFramebufferCreateFlags vkCreateFlags, 	
			const void *pNext ) 
	{ 
		assert( vkRenderPass != VK_NULL_HANDLE );

		VkFramebufferCreateInfo frameBufferCI { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		frameBufferCI.pNext = pNext;
		frameBufferCI.flags = vkCreateFlags;
		frameBufferCI.width = m_unTextureWidth;
		frameBufferCI.height = m_unTextureHeight;
		frameBufferCI.renderPass = vkRenderPass;
		frameBufferCI.attachmentCount = (uint32_t) arrImageViews.size();
		frameBufferCI.pAttachments = arrImageViews.data();
		frameBufferCI.layers = 1; // must be one when using multiview

		return frameBufferCI;
	}

	VkSamplerCreateInfo CStereoRender::GenerateImageSamplerCI( VkSamplerCreateFlags flags, void *pNext ) 
	{ 
		VkSamplerCreateInfo samplerCI { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCI.pNext = pNext;
		samplerCI.flags = flags;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = samplerCI.addressModeU;
		samplerCI.addressModeW = samplerCI.addressModeU;
		samplerCI.mipLodBias = 0.0f;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.anisotropyEnable = VK_TRUE;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		return samplerCI;
	}

	VkPipelineColorBlendAttachmentState CStereoRender::GenerateColorBlendAttachment() 
	{ 
		VkPipelineColorBlendAttachmentState colorBlendAttachment {};
		colorBlendAttachment.blendEnable = 0;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		return colorBlendAttachment;
	}

	VkPipelineLayoutCreateInfo CStereoRender::GeneratePipelineLayoutCI(
		std::vector< VkPushConstantRange > &vecPushConstantRanges,
		std::vector< VkDescriptorSetLayout > &vecDescriptorSetLayouts,
		VkPipelineLayoutCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineLayoutCreateInfo pipelineCI { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineCI.pNext = pNext;
		pipelineCI.flags = createFlags;
		pipelineCI.pushConstantRangeCount = vecPushConstantRanges.empty() ? 0 : (uint32_t) vecPushConstantRanges.size();
		pipelineCI.pPushConstantRanges = vecPushConstantRanges.empty() ? nullptr : vecPushConstantRanges.data();
		pipelineCI.setLayoutCount = vecDescriptorSetLayouts.empty() ? 0 : (uint32_t) vecDescriptorSetLayouts.size();
		pipelineCI.pSetLayouts = vecDescriptorSetLayouts.empty() ? nullptr : vecDescriptorSetLayouts.data();

		return pipelineCI;
	}

	VkPipelineVertexInputStateCreateInfo CStereoRender::GeneratePipelineStateCI_VertexInput(
		std::vector< VkVertexInputBindingDescription > &vecVertexBindingDescriptions,
		std::vector< VkVertexInputAttributeDescription > &vecVertexAttributeDescriptions,
		VkPipelineVertexInputStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineVertexInputStateCreateInfo outVertexInputCI { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		outVertexInputCI.pNext = pNext;
		outVertexInputCI.flags = createFlags;
		outVertexInputCI.vertexBindingDescriptionCount = vecVertexBindingDescriptions.empty() ? 0 : (uint32_t) vecVertexBindingDescriptions.size();
		outVertexInputCI.pVertexBindingDescriptions = vecVertexBindingDescriptions.empty() ? nullptr : vecVertexBindingDescriptions.data();
		outVertexInputCI.vertexAttributeDescriptionCount = vecVertexAttributeDescriptions.empty() ? 0 : (uint32_t) vecVertexAttributeDescriptions.size();
		outVertexInputCI.pVertexAttributeDescriptions = vecVertexAttributeDescriptions.empty() ? nullptr : vecVertexAttributeDescriptions.data();

		return outVertexInputCI;
	}

	VkPipelineInputAssemblyStateCreateInfo CStereoRender::GeneratePipelineStateCI_Assembly( 
		VkPrimitiveTopology topology, 
		VkBool32 primitiveRestartEnable, 
		VkPipelineInputAssemblyStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineInputAssemblyStateCreateInfo outInputAssemblyCI { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		outInputAssemblyCI.pNext = pNext;
		outInputAssemblyCI.flags = createFlags;
		outInputAssemblyCI.topology = topology;
		outInputAssemblyCI.primitiveRestartEnable = primitiveRestartEnable;
		
		return outInputAssemblyCI;
	}

	VkPipelineTessellationStateCreateInfo CStereoRender::GeneratePipelineStateCI_TesselationCI( 
		uint32_t patchControlPoints, 
		VkPipelineTessellationStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineTessellationStateCreateInfo tesselationCI { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		tesselationCI.pNext = pNext;
		tesselationCI.flags = createFlags;
		tesselationCI.patchControlPoints = patchControlPoints;
		
		return tesselationCI;
	}

	VkPipelineViewportStateCreateInfo CStereoRender::GeneratePipelineStateCI_ViewportCI( 
		std::vector< VkViewport > &vecViewports, 
		std::vector< VkRect2D > &vecScissors, 
		VkPipelineViewportStateCreateFlags createFlags, void *pNext )
	{
		VkPipelineViewportStateCreateInfo viewportCI { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportCI.pNext = pNext;
		viewportCI.flags = createFlags;
		viewportCI.viewportCount = vecViewports.empty() ? 0 : (uint32_t) vecViewports.size();
		viewportCI.pViewports = vecViewports.empty() ? nullptr : vecViewports.data();
		viewportCI.scissorCount = vecScissors.empty() ? 0 : (uint32_t) vecScissors.size();
		viewportCI.pScissors = vecScissors.empty() ? nullptr : vecScissors.data();

		return viewportCI;
	}

	VkPipelineRasterizationStateCreateInfo CStereoRender::GeneratePipelineStateCI_RasterizationCI(
		VkPolygonMode polygonMode,
		VkCullModeFlags cullMode,
		VkFrontFace frontFace,
		float lineWidth,
		VkBool32 depthClampEnable,
		float depthBiasClamp,
		VkBool32 depthBiasEnable,
		float depthBiasConstantFactor,
		float depthBiasSlopeFactor,
		VkBool32 rasterizerDiscardEnable,
		VkPipelineRasterizationStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineRasterizationStateCreateInfo rasterizationCI { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationCI.pNext = pNext;
		rasterizationCI.flags = createFlags;
		rasterizationCI.polygonMode = polygonMode;
		rasterizationCI.cullMode = cullMode;
		rasterizationCI.frontFace = frontFace;
		rasterizationCI.lineWidth = lineWidth;
		rasterizationCI.depthClampEnable = depthClampEnable;
		rasterizationCI.depthBiasEnable = depthBiasEnable;
		rasterizationCI.depthBiasClamp = depthBiasClamp;
		rasterizationCI.depthBiasConstantFactor = depthBiasConstantFactor;
		rasterizationCI.depthBiasSlopeFactor = depthBiasSlopeFactor;
		rasterizationCI.rasterizerDiscardEnable = rasterizerDiscardEnable;

		return rasterizationCI;
	}

	VkPipelineMultisampleStateCreateInfo CStereoRender::GeneratePipelineStateCI_MultisampleCI(
		VkSampleCountFlagBits rasterizationSamples,
		VkBool32 sampleShadingEnable,
		float minSampleShading,
		VkSampleMask *pSampleMask,
		VkBool32 alphaToCoverageEnable,
		VkBool32 alphaToOneEnable,
		VkPipelineMultisampleStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineMultisampleStateCreateInfo multisampleCI { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleCI.pNext = pNext;
		multisampleCI.flags = createFlags;
		multisampleCI.rasterizationSamples = rasterizationSamples;
		multisampleCI.sampleShadingEnable = sampleShadingEnable;
		multisampleCI.minSampleShading = minSampleShading;
		multisampleCI.pSampleMask = pSampleMask;
		multisampleCI.alphaToCoverageEnable = alphaToCoverageEnable;
		multisampleCI.alphaToOneEnable = alphaToOneEnable;

		return multisampleCI;
	}

	VkPipelineDepthStencilStateCreateInfo CStereoRender::GeneratePipelineStateCI_DepthStencilCI(
		VkBool32 depthTestEnable,
		VkBool32 depthWriteEnable,
		VkCompareOp depthCompareOp,
		VkBool32 depthBoundsTestEnable,
		VkBool32 stencilTestEnable,
		VkStencilOpState front,
		VkStencilOpState back,
		float minDepthBounds,
		float maxDepthBounds,
		VkPipelineDepthStencilStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineDepthStencilStateCreateInfo depthStencilCI { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilCI.pNext = pNext;
		depthStencilCI.flags = createFlags;
		depthStencilCI.depthTestEnable = depthTestEnable;
		depthStencilCI.depthWriteEnable = depthWriteEnable;
		depthStencilCI.depthCompareOp = depthCompareOp;
		depthStencilCI.depthBoundsTestEnable = depthBoundsTestEnable;
		depthStencilCI.stencilTestEnable = stencilTestEnable;
		depthStencilCI.front = front;
		depthStencilCI.back = back;
		depthStencilCI.minDepthBounds = minDepthBounds;
		depthStencilCI.maxDepthBounds = maxDepthBounds;
		
		return depthStencilCI;
	}

	VkPipelineColorBlendStateCreateInfo CStereoRender::GeneratePipelineStateCI_ColorBlendCI(
		std::vector< VkPipelineColorBlendAttachmentState > &vecColorBlenAttachmentStates,
		VkBool32 logicOpEnable,
		VkLogicOp logicOp,
		VkPipelineColorBlendStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineColorBlendStateCreateInfo colorBlendCI { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendCI.pNext = pNext;
		colorBlendCI.flags = createFlags;
		colorBlendCI.attachmentCount = vecColorBlenAttachmentStates.empty() ? 0 : (uint32_t)vecColorBlenAttachmentStates.size();
		colorBlendCI.pAttachments = vecColorBlenAttachmentStates.empty() ? nullptr : vecColorBlenAttachmentStates.data();
		colorBlendCI.logicOpEnable = logicOpEnable;
		colorBlendCI.logicOp = logicOp;
		
		return colorBlendCI;
	}

	VkPipelineDynamicStateCreateInfo CStereoRender::GeneratePipelineStateCI_DynamicStateCI( 
		std::vector< VkDynamicState > &vecDynamicStates, 
		VkPipelineDynamicStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineDynamicStateCreateInfo dynamicStateCI { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateCI.pNext = pNext;
		dynamicStateCI.flags = createFlags;
		dynamicStateCI.dynamicStateCount = vecDynamicStates.empty() ? 0 : (uint32_t) vecDynamicStates.size();
		dynamicStateCI.pDynamicStates = vecDynamicStates.empty() ? nullptr : vecDynamicStates.data();

		return dynamicStateCI;
	}

	VkResult CStereoRender::CreateGraphicsPipeline(
		VkPipeline &outPipeline,
		VkPipelineLayout pipelineLayout,
		VkRenderPass renderPass,
		std::vector< VkPipelineShaderStageCreateInfo > &vecShaderStages,
		const VkPipelineVertexInputStateCreateInfo *pVertexInputCI,
		const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyCI,
		const VkPipelineTessellationStateCreateInfo *pTessellationCI,
		const VkPipelineViewportStateCreateInfo *pViewportStateCI,
		const VkPipelineRasterizationStateCreateInfo *pRasterizationCI,
		const VkPipelineMultisampleStateCreateInfo *pMultisampleCI,
		const VkPipelineDepthStencilStateCreateInfo *pDepthStencilCI,
		const VkPipelineColorBlendStateCreateInfo *pColorBlendCI,
		const VkPipelineDynamicStateCreateInfo *pDynamicStateCI,
		const VkPipelineCache pipelineCache,
		const uint32_t unSubpassIndex,
		const VkPipelineCreateFlags createFlags,
		const void *pNext,
		const VkAllocationCallbacks *pCallbacks )
	{
		assert( GetLogicalDevice() != VK_NULL_HANDLE );

		VkGraphicsPipelineCreateInfo pipelineCI { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCI.pNext = pNext;
		pipelineCI.flags = createFlags;

		pipelineCI.layout = pipelineLayout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.subpass = unSubpassIndex;

		pipelineCI.stageCount = (uint32_t) vecShaderStages.size();
		pipelineCI.pStages = vecShaderStages.data();
		pipelineCI.pVertexInputState = pVertexInputCI;
		pipelineCI.pInputAssemblyState = pInputAssemblyCI;
		pipelineCI.pTessellationState = pTessellationCI;
		pipelineCI.pViewportState = pViewportStateCI;
		pipelineCI.pRasterizationState = pRasterizationCI;
		pipelineCI.pMultisampleState = pMultisampleCI;
		pipelineCI.pDepthStencilState = pDepthStencilCI;
		pipelineCI.pColorBlendState = pColorBlendCI;
		pipelineCI.pDynamicState = pDynamicStateCI;

		return vkCreateGraphicsPipelines( GetLogicalDevice(), pipelineCache, 1, &pipelineCI, pCallbacks, &outPipeline );
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Stencil(
		#ifdef XR_USE_PLATFORM_ANDROID
				AAssetManager *assetManager,
		#endif
		uint32_t unSubpassIndex,
		VkPipeline &outPipeline,
		SGraphicsPipelineLayout &pipelineLayout,
		VkRenderPass vkRenderPass,
		std::string sVertexShaderFilename,
		std::string sFragmentShaderFilename )
	{
		assert( GetLogicalDevice() != VK_NULL_HANDLE );
		assert( vkRenderPass != VK_NULL_HANDLE );
		assert( !sVertexShaderFilename.empty() );
		assert( !sFragmentShaderFilename.empty() );

		// (1) Create pipeline layout with push constant definitions for the shader
		std::vector< VkPushConstantRange > vecPCRs = { { VK_SHADER_STAGE_VERTEX_BIT, 0, pipelineLayout.size } };
		std::vector< VkDescriptorSetLayout > vecLayouts;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = GeneratePipelineLayoutCI( vecPCRs, vecLayouts );

		VkResult result = vkCreatePipelineLayout( GetLogicalDevice(), &pipelineLayoutCI, nullptr, &pipelineLayout.layout );
		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// (2) Setup shader set
		SShaderSet *pShaders = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );

		#ifdef XR_USE_PLATFORM_ANDROID
				pShaders->Init( assetManager, GetLogicalDevice() );
		#else
				pShaders->Init( GetLogicalDevice() );
		#endif

		// (3) Define shader vertex inputs
		pShaders->vertexBindings.push_back( { 0, sizeof( XrVector2f ), VK_VERTEX_INPUT_RATE_VERTEX } );
		pShaders->vertexAttributes.push_back( { 0, 0, VK_FORMAT_R32G32_SFLOAT } );

		auto vertexInputCI = GeneratePipelineStateCI_VertexInput( pShaders->vertexBindings, pShaders->vertexAttributes );

		// (4) Define viewport
		std::vector< VkViewport > vecViewports { { 0.0f, 0.0f, (float) GetTextureWidth(), (float) GetTextureHeight(), 0.0f, 1.0f } }; // Will invert y after projection
		std::vector< VkRect2D > vecScissors { { { 0, 0 }, GetTextureExtent() } };

		auto viewportCI = GeneratePipelineStateCI_ViewportCI( vecViewports, vecScissors );

		// (5) Define other pipeline states
		auto assemblyCI = GeneratePipelineStateCI_Assembly();
		auto multisampleCI = GeneratePipelineStateCI_MultisampleCI();
		auto depthStencilCI = GeneratePipelineStateCI_DepthStencilCI();

		auto rasterizationCI = GeneratePipelineStateCI_RasterizationCI();
		rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationCI.cullMode = VK_CULL_MODE_NONE;

		// (6) Configure Depth
		if ( m_pSession->GetVulkan()->IsDepthFormat( m_vkDepthFormat ) )
		{
			// Enable depth testing and writing
			depthStencilCI.depthTestEnable = VK_FALSE;			
			depthStencilCI.depthWriteEnable = VK_FALSE;			
			depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

			// Enable stencil test
			depthStencilCI.stencilTestEnable = VK_TRUE;

			// Stencil op state
			depthStencilCI.front.failOp = VK_STENCIL_OP_KEEP;
			depthStencilCI.front.passOp = VK_STENCIL_OP_REPLACE;  
			depthStencilCI.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthStencilCI.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilCI.front.compareMask = 0xFF;			   // Test all bits
			depthStencilCI.front.writeMask = 0xFF;				   // Write all bits to stencil buffer
			depthStencilCI.front.reference = 1;					   

			depthStencilCI.back = depthStencilCI.front;			   // Use the same configuration for back face

			// Depth bounds - no testing to save a bit of overhead
			depthStencilCI.depthBoundsTestEnable = VK_FALSE;
		}

		// (7) Create graphics pipeline
		result = CreateGraphicsPipeline(
			outPipeline, 
			pipelineLayout.layout, 
			vkRenderPass, 
			pShaders->stages, 
			&vertexInputCI, 
			&assemblyCI, 
			nullptr, 
			&viewportCI, 
			&rasterizationCI, 
			&multisampleCI, 
			&depthStencilCI, 
			nullptr,
			nullptr,
			VK_NULL_HANDLE,
			unSubpassIndex); 

		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// (8) Cleanup
		delete pShaders;
		return VK_SUCCESS;
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Stencils(
		#ifdef XR_USE_PLATFORM_ANDROID
				AAssetManager *assetManager,
		#endif
		std::vector< VkPipeline > &outPipelines,
		SGraphicsPipelineLayout &pipelineLayout,
		VkRenderPass vkRenderPass,
		std::vector< std::string > vecVertexShaders,
		std::vector< std::string > vecFragmentShaders )
	{
		assert( vecVertexShaders.size() == vecFragmentShaders.size() );
		VkResult result = VK_SUCCESS;

		outPipelines.clear();
		for ( uint32_t i = 0; i < vecVertexShaders.size(); i++ )
		{
			outPipelines.push_back( VK_NULL_HANDLE );
			result = CreateGraphicsPipeline_Stencil(
			#ifdef XR_USE_PLATFORM_ANDROID
				assetManager,
			#endif
				i,
				outPipelines[ i ],
				pipelineLayout,
				vkRenderPass,
				vecVertexShaders[ i ],
				vecFragmentShaders[ i ] );

			if ( !VK_CHECK_SUCCESS( result ) )
				break;
		}

		return result;
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Primitives( 
		#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
		#endif
		VkPipeline &outPipeline,
		SGraphicsPipelineLayout &pipelineLayout,
		VkRenderPass vkRenderPass,
		std::string sVertexShaderFilename,
		std::string sFragmentShaderFilename ) 
	{ 
		assert( GetLogicalDevice() != VK_NULL_HANDLE );
		assert( vkRenderPass != VK_NULL_HANDLE );
		assert( !sVertexShaderFilename.empty() );
		assert( !sFragmentShaderFilename.empty() );

		// (1) Create pipeline layout with push constant definitions for the shader
		std::vector< VkPushConstantRange > vecPCRs = { { VK_SHADER_STAGE_VERTEX_BIT, 0, pipelineLayout.size } };
		std::vector< VkDescriptorSetLayout > vecLayouts;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = GeneratePipelineLayoutCI( vecPCRs, vecLayouts );

		VkResult result = vkCreatePipelineLayout( GetLogicalDevice(), &pipelineLayoutCI, nullptr, &pipelineLayout.layout );
		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// (2) Setup shader set
		SShaderSet *pShaders = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );

		#ifdef XR_USE_PLATFORM_ANDROID
			pShaders->Init( assetManager, GetLogicalDevice() );
		#else
			pShaders->Init( GetLogicalDevice() );
		#endif

		// (3) Define shader vertex inputs
		pShaders->vertexBindings.push_back( { 0, sizeof( SColoredVertex ), VK_VERTEX_INPUT_RATE_VERTEX } );
		pShaders->vertexAttributes.push_back( { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( SColoredVertex, position ) } );
		pShaders->vertexAttributes.push_back( { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( SColoredVertex, color ) } );

		pShaders->vertexBindings.push_back( { 1, sizeof( XrMatrix4x4f ), VK_VERTEX_INPUT_RATE_INSTANCE } );
		uint32_t sizeFloat = (uint32_t) sizeof( float );
		pShaders->vertexAttributes.push_back( { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 } );
		pShaders->vertexAttributes.push_back( { 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeFloat } );
		pShaders->vertexAttributes.push_back( { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 8 * sizeFloat } );
		pShaders->vertexAttributes.push_back( { 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 12 * sizeFloat } );

		auto vertexInputCI = GeneratePipelineStateCI_VertexInput( pShaders->vertexBindings, pShaders->vertexAttributes );

		// (4) Define viewport
		std::vector< VkViewport > vecViewports { { 0.0f, 0.0f, (float) GetTextureWidth(), (float) GetTextureHeight(), 0.0f, 1.0f } }; // Will invert y after projection
		std::vector< VkRect2D > vecScissors { { { 0, 0 }, GetTextureExtent() } };

		auto viewportCI = GeneratePipelineStateCI_ViewportCI( vecViewports, vecScissors );

		// (5) Setup color blend attachments
		std::vector< VkPipelineColorBlendAttachmentState > vecColorBlendAttachments { GenerateColorBlendAttachment() };
		auto colorBlendCI = GeneratePipelineStateCI_ColorBlendCI( vecColorBlendAttachments );

		// (6) Define Dynamic states
		std::vector< VkDynamicState > vecDynamicStates; // { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		auto dynamicStateCI = GeneratePipelineStateCI_DynamicStateCI( vecDynamicStates );

		// (7) Define other pipeline states
		auto assemblyCI = GeneratePipelineStateCI_Assembly();
		auto rasterizationCI = GeneratePipelineStateCI_RasterizationCI();
		auto multisampleCI = GeneratePipelineStateCI_MultisampleCI();
		auto depthStencilCI = GeneratePipelineStateCI_DepthStencilCI();

		// (8) Configure Depth
		if ( m_pSession->GetVulkan()->IsDepthFormat( m_vkDepthFormat ) )
		{
			// Enable depth testing and writing
			depthStencilCI.depthTestEnable = VK_TRUE;			// Enable depth testing
			depthStencilCI.depthWriteEnable = VK_TRUE;			// Enable depth writing
			depthStencilCI.depthCompareOp = VK_COMPARE_OP_NOT_EQUAL; 

			//Set stencil test
			depthStencilCI.stencilTestEnable = m_bUseVisMask ? VK_TRUE : VK_FALSE;

			// Depth bounds - no testing to save a bit of overhead 
			depthStencilCI.depthBoundsTestEnable = VK_FALSE;

			 if ( m_bUseVisMask )
			{
				// Configure stencil operations if the stencil test is enabled
				depthStencilCI.front.failOp = VK_STENCIL_OP_KEEP;	
				depthStencilCI.front.passOp = VK_STENCIL_OP_KEEP;	   
				depthStencilCI.front.depthFailOp = VK_STENCIL_OP_KEEP; 
				depthStencilCI.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;  

				depthStencilCI.front.compareMask = 0xFF; // Use all stencil bits
				depthStencilCI.front.writeMask = 0x00;	 // Disable writes to stencil bits
				depthStencilCI.front.reference = 1;		

				depthStencilCI.back = depthStencilCI.front; // Configure back face with the same settings
			}
		}

		// (9) Create graphics pipeline
		result = CreateGraphicsPipeline(
			outPipeline, 
			pipelineLayout.layout, 
			vkRenderPass, 
			pShaders->stages, 
			&vertexInputCI, 
			&assemblyCI, 
			nullptr, 
			&viewportCI, 
			&rasterizationCI, 
			&multisampleCI, 
			&depthStencilCI, 
			&colorBlendCI, 
			&dynamicStateCI, 
			VK_NULL_HANDLE, 
			m_bUseVisMask ? 2 : 0	// @todo: two subpasses, one for each eye until khr vismask spec supports multiview
		); 

		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// (10) Cleanup
		delete pShaders;
		return VK_SUCCESS; 
	}

	VkResult CStereoRender::AddRenderPass( VkRenderPassCreateInfo *renderPassCI, VkAllocationCallbacks *pAllocator ) 
	{ 
		assert( renderPassCI );
		assert( renderPassCI->sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO );

		vecRenderPasses.push_back( VK_NULL_HANDLE );

		VkResult result = vkCreateRenderPass( GetLogicalDevice(), renderPassCI, nullptr, &vecRenderPasses.back() ); 
		if ( result != VK_SUCCESS )
		{
			vecRenderPasses.pop_back();
			return result;
		}

		return VK_SUCCESS;
	}

	void CStereoRender::RenderFrame( 
		VkRenderPass renderPass, 
		SRenderFrameInfo &renderInfo, 
		std::vector< CColoredPrimitive * > &primitives, 
		std::vector< CPlane2D * > &stencils ) 
	{
		// (1) Start frame
		if ( !XR_UNQUALIFIED_SUCCESS( m_pSession->StartFrame( &renderInfo.frameState ) ) )
			return;

		// (2) Render frame
		if ( renderInfo.frameState.shouldRender )
		{
			#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
				if ( rdoc_api && rdoc_api->GetNumCaptures() < RENDERDOC_FRAME_SAMPLES )
				{
					rdoc_api->StartFrameCapture( RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE( m_pSession->GetVulkan()->GetVkInstance() ), NULL );
					LogDebug( "RENDERDOC", "Renderdoc capture started for frame: %i", rdoc_api->GetNumCaptures() + 1 );
				}
			#endif

			// (2.1) Update eye view pose, fov, etc
			m_pSession->UpdateEyeStates( 
				m_vecEyeViews, 
				renderInfo.eyeProjectionMatrices, 
				&renderInfo.sharedEyeState, 
				&renderInfo.frameState, 
				m_pSession->GetAppSpace(),
				renderInfo.nearZ,
				renderInfo.farZ );

			// Only render if updated eye orientation is valid
			if ( renderInfo.sharedEyeState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT )
			{
				// (XX) Update Hmd pose
				m_pSession->UpdateHmdPose( renderInfo.frameState.predictedDisplayTime + renderInfo.frameState.predictedDisplayPeriod );
				m_pSession->GetHmdPose( renderInfo.hmdPose );

				
				// (2.3) Acquire swapchain images
				m_pSession->AcquireFrameImage( &renderInfo.unCurrentSwapchainImage_Color, &renderInfo.unCurrentSwapchainImage_Depth, GetColorSwapchain(), GetDepthSwapchain() );

				// (2.11) Wait for swapchain images. This ensures that the openxr runtime is finish with them before we submit the draw commands to the gpu
				m_pSession->WaitForFrameImage( GetColorSwapchain(), GetDepthSwapchain() );

				// (2.2) Update frame layers (eye projection views)
				for ( uint32_t i = 0; i < k_EyeCount; i++ )
				{
					renderInfo.projectionLayerViews[ i ].pose = m_vecEyeViews[ i ].pose;
					renderInfo.projectionLayerViews[ i ].fov = m_vecEyeViews[ i ].fov;
					renderInfo.projectionLayerViews[ i ].subImage.swapchain = GetColorSwapchain();
					renderInfo.projectionLayerViews[ i ].subImage.imageArrayIndex = i;
					renderInfo.projectionLayerViews[ i ].subImage.imageRect.offset = { 0, 0 };
					renderInfo.projectionLayerViews[ i ].subImage.imageRect.extent = GetTexutreExtent2Di();

					XrCompositionLayerDepthInfoKHR depthInfo { XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR };
					depthInfo.subImage.swapchain = GetDepthSwapchain();
					depthInfo.subImage.imageArrayIndex = i;
					depthInfo.subImage.imageRect.offset = renderInfo.imageRectOffsets[ i ];
					depthInfo.subImage.imageRect.extent = GetTexutreExtent2Di();
					depthInfo.minDepth = renderInfo.minDepth;
					depthInfo.maxDepth = renderInfo.maxDepth;
					depthInfo.nearZ = renderInfo.nearZ;
					depthInfo.farZ = renderInfo.farZ;

					renderInfo.projectionLayerViews[ i ].next = &depthInfo;
				}

				// (2.6) Calculate view projection matrices for each eye
				CalculateViewMatrices( renderInfo.eyeViewMatrices, &renderInfo.eyeScale );

				XrMatrix4x4f_Multiply( &renderInfo.arrEyeVPs[ k_Left ], &renderInfo.eyeProjectionMatrices[ k_Left ], &renderInfo.eyeViewMatrices[ k_Left ] );
				XrMatrix4x4f_Multiply( &renderInfo.arrEyeVPs[ k_Right ], &renderInfo.eyeProjectionMatrices[ k_Right ], &renderInfo.eyeViewMatrices[ k_Right ] );

				// (2.5) Begin draw commands for rendering
				BeginDraw( renderInfo.unCurrentSwapchainImage_Color, renderInfo.clearValues, true, renderPass );

				// (2.7) Draw vismask (if activated)
				if ( m_bUseVisMask && stencils.size() == 2 )
				{
					for ( size_t eyeIndex = 0; eyeIndex < stencils.size(); ++eyeIndex )
					{
						// Push constants for the vertex shader
						vkCmdPushConstants(
							GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer,
							renderInfo.pipelineLayout.layout,
							VK_SHADER_STAGE_VERTEX_BIT,
							0,
							renderInfo.pipelineLayout.size,
							renderInfo.eyeProjectionMatrices.data() );

						// Set stencil reference to write to the stencil mask
						vkCmdSetStencilReference( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1 );

						// Bind the vismask pipeline
						vkCmdBindPipeline( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.stencilPipelines[ eyeIndex ] );

						// Bind vismask vertices and indices for the current eye
						VkDeviceSize offsets[] = { 0 };

						// Bind the index buffer for the current eye stencil
						vkCmdBindIndexBuffer( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, stencils[ eyeIndex ]->GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );

						// Bind the vertex buffer for the current eye stencil
						vkCmdBindVertexBuffers( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 0, 1, stencils[ eyeIndex ]->GetVertexBuffer()->GetVkBufferPtr(), offsets );

						// Draw the stencil for the current eye
						vkCmdDrawIndexed(
							GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer,
							static_cast< uint32_t >( stencils[ eyeIndex ]->GetIndices()->size() ), // Number of indices to draw
							1,																		  // Instance count
							0,																		  // First index
							0,																		  // Vertex offset
							0																		  // First instance
						);

						if ( eyeIndex == 0 )
						{
							// Transition to the right eye subpass
							vkCmdNextSubpass( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_SUBPASS_CONTENTS_INLINE );
						}
					}

					// Transition to the next subpass for main rendering
					vkCmdNextSubpass( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_SUBPASS_CONTENTS_INLINE );
				}

	
				// (2.4) Copy model matrices to gpu buffer
				renderInfo.ClearStagingBuffers();
				{
					// (2.4.1) Begin buffer recording to gpu
					BeginBufferUpdates( renderInfo.unCurrentSwapchainImage_Color );

					// (2.4.2) Update asset buffers
					XrTime renderTime = renderInfo.frameState.predictedDisplayTime + renderInfo.frameState.predictedDisplayPeriod;

					for ( uint32_t i = 0; i < primitives.size(); i++ )
					{
						// Update matrices (for each instance)
						for ( uint32_t j = 0; j < primitives[ i ]->instances.size(); j++ )
							primitives[ i ]->UpdateModelMatrix( j, m_pSession->GetAppSpace(), renderTime );

						// Add to render
						renderInfo.vecStagingBuffers.push_back( primitives[ i ]->UpdateBuffer( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkTransferCommandBuffer ) );
					}

					// (2.4.3) Submit to gpu
					SubmitBufferUpdates( renderInfo.unCurrentSwapchainImage_Color );
				}

				// (2.8) Main rendering subpass: Update push constants
				vkCmdPushConstants( 
					GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 
					renderInfo.pipelineLayout.layout, 
					VK_SHADER_STAGE_VERTEX_BIT, 
					0, 
					renderInfo.pipelineLayout.size, 
					renderInfo.arrEyeVPs.data() );

				// (2.9) Main rendering subpass: Draw render assets
				for ( CColoredPrimitive* coloredPrimitive : primitives )
				{
					vkCmdSetStencilReference( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1 );

					// Bind the graphics pipeline for this shape
					vkCmdBindPipeline(
						GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						coloredPrimitive->pipeline == VK_NULL_HANDLE ? renderInfo.renderPipeline : coloredPrimitive->pipeline );

					// Bind shape's index and vertex buffers
					vkCmdBindIndexBuffer( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, coloredPrimitive->GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );

					vkCmdBindVertexBuffers( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 0, 1, coloredPrimitive->GetVertexBuffer()->GetVkBufferPtr(), coloredPrimitive->vertexOffsets );   // primitive verts

					vkCmdBindVertexBuffers( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 1, 1, coloredPrimitive->GetInstanceBuffer()->GetVkBufferPtr(), coloredPrimitive->instanceOffsets );   // primitive instances

					// Draw the shape
					vkCmdDrawIndexed( GetMultiviewRenderTargets().at( renderInfo.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 
						coloredPrimitive->GetIndices().size(), coloredPrimitive->GetInstanceCount(), 0, 0, 0 );
				}



				// (2.12) Submit draw calls to gpu - this will also clear the staging buffers (if any)
				SubmitDraw( renderInfo.unCurrentSwapchainImage_Color, renderInfo.vecStagingBuffers );

				// (2.13) Release the swapchian image to let the openxr runtime know we're through with it
				m_pSession->ReleaseFrameImage( GetColorSwapchain(), GetDepthSwapchain() );
			}

			// (2.14) Assemble frame layers
			renderInfo.projectionLayer.next = nullptr;
			renderInfo.projectionLayer.layerFlags = 0;

			renderInfo.projectionLayer.space = m_pSession->GetAppSpace();
			renderInfo.projectionLayer.viewCount = (uint32_t) renderInfo.projectionLayerViews.size();
			renderInfo.projectionLayer.views = renderInfo.projectionLayerViews.data();

			renderInfo.frameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &renderInfo.projectionLayer ) );
		}

		// (3) End frame
		m_pSession->EndFrame( &renderInfo.frameState, renderInfo.frameLayers );
		renderInfo.frameLayers.clear();

		#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
			if ( rdoc_api && rdoc_api->GetNumCaptures() < RENDERDOC_FRAME_SAMPLES )
			{
				if ( rdoc_api->EndFrameCapture( RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE( m_pSession->GetVulkan()->GetVkInstance() ), NULL ) )
					LogDebug( "RENDERDOC", "Renderdoc capture ended for frame: %i", rdoc_api->GetNumCaptures() );
				else
					LogError( "RENDERDOC", "FAILED FRAME CAPTURE for frame: %i", rdoc_api->GetNumCaptures() + 1 );
			}
		#endif
	}

	void CStereoRender::BeginDraw(
		const uint32_t unSwpachainImageIndex,
		std::vector< VkClearValue > &vecClearValues,
		const bool startCommandBufferRecording,
		const VkRenderPass renderpass,
		const VkSubpassContents subpass)
	{
		// @todo - debug assert swapchain image index vs size of render targets

		if ( startCommandBufferRecording )
		{
			// Set command buffer to recording
			VkCommandBufferBeginInfo cmdBeginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			vkBeginCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, &cmdBeginInfo );
			// @todo assert on VkResult for debug only
		}

		if ( renderpass != VK_NULL_HANDLE )
		{
			// Set render pass begin info
			VkRenderPassBeginInfo renderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.clearValueCount = (uint32_t) vecClearValues.size();
			renderPassBeginInfo.pClearValues = vecClearValues.data();
			renderPassBeginInfo.renderPass = renderpass;
			renderPassBeginInfo.framebuffer = m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkFrameBuffer;
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = GetTextureExtent();

			// (3) Start render pass
			vkCmdBeginRenderPass( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, &renderPassBeginInfo, subpass );
		}
	}

	void CStereoRender::SubmitDraw(
		const uint32_t unSwpachainImageIndex,
		std::vector< CDeviceBuffer * > &vecStagingBuffers,
		const uint32_t timeoutNs,
		const VkCommandBufferResetFlags transferBufferResetFlags, const VkCommandBufferResetFlags renderBufferResetFlags ) 
	{
		// End render recording
		vkCmdEndRenderPass( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer );
		vkEndCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer );

		// Wait for any data transfer operations to finish
		// @todo start recording for next frame
		vkWaitForFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence, VK_TRUE, timeoutNs );
		vkResetFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence );
		vkResetCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer, transferBufferResetFlags );

		// Clear/free staging buffer memory
		for ( CDeviceBuffer *pStagingBuffer : vecStagingBuffers )
			delete pStagingBuffer;

		vecStagingBuffers.clear();

		// Execute render commands (requires exclusive access to vkQueue)
		// safest after wait swapchain image
		VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer;
		vkQueueSubmit( GetAppSession()->GetVulkan()->GetVkQueue_Graphics(), 1, &submitInfo, m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence );

		// Wait for rendering to finish
		// @todo move to separate thread - or at start unSwapchainIndex from prior frame
		vkWaitForFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence, VK_TRUE, timeoutNs );
		vkResetFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence );
		vkResetCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, renderBufferResetFlags );
	}

	void CStereoRender::BeginBufferUpdates( const uint32_t unSwpachainImageIndex ) 
	{ 
		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer, &beginInfo );
	}

	void CStereoRender::SubmitBufferUpdates( const uint32_t unSwpachainImageIndex ) 
	{
		vkEndCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer );

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer;

		vkQueueSubmit( GetAppSession()->GetVulkan()->GetVkQueue_Transfer(), 1, &submitInfo, m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence );
	}

	void CStereoRender::CalculateViewMatrices( std::vector< XrMatrix4x4f > &outViewMatrices, const XrVector3f *eyeScale ) 
	{
		std::vector< XrMatrix4x4f > eyeViews;
		eyeViews.resize( outViewMatrices.size() );
		for ( uint32_t i = 0; i < outViewMatrices.size(); i++ )
		{
			XrMatrix4x4f_CreateTranslationRotationScale( &eyeViews[ i ], &m_vecEyeViews[ i ].pose.position, &m_vecEyeViews[ i ].pose.orientation, eyeScale );
			XrMatrix4x4f_InvertRigidBody( &outViewMatrices[ i ], &eyeViews[ i ] );
		}
	}

	VkPhysicalDevice CStereoRender::GetPhysicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkPhysicalDevice(); 
	}

	VkDevice CStereoRender::GetLogicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkLogicalDevice(); 
	}

} // namespace xrlib
