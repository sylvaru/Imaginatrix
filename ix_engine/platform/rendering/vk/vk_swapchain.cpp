// vk_swapchain.cpp
#include "common/engine_pch.h"
#include "vk_swapchain.h"
#include "vk_context.h"
#include "vk_image.h"  

namespace ix
{

	VulkanSwapchain::VulkanSwapchain(VulkanContext& context, VkExtent2D extent, VulkanSwapchain* old, bool vSync)
		: m_context(context)
		, m_extent(extent)
	{
		// Capture the handle from the raw pointer if it exists
		VkSwapchainKHR oldHandle = (old != nullptr) ? old->get() : VK_NULL_HANDLE;

		create(oldHandle, vSync);
	}

	VulkanSwapchain::~VulkanSwapchain()
	{
		VkDevice device = m_context.device();

		for (auto& frame : m_frames) {
			if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
				frame.renderFinishedSemaphore = VK_NULL_HANDLE;
			}
		}

		m_frames.clear();

		if (m_swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device, m_swapchain, nullptr);
			m_swapchain = VK_NULL_HANDLE;
		}
	}
	void VulkanSwapchain::create(VkSwapchainKHR oldHandle, bool vSync)
	{
		VkSurfaceCapabilitiesKHR caps;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.physicalDevice(), m_context.surface(), &caps);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice(), m_context.surface(), &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice(), m_context.surface(), &formatCount, formats.data());

		VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
		m_imageFormat = surfaceFormat.format;

		uint32_t imageCount = caps.minImageCount + 1;
		if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
			imageCount = caps.maxImageCount;
		}

		VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		ci.surface = m_context.surface();
		ci.minImageCount = imageCount;
		ci.imageFormat = surfaceFormat.format;
		ci.imageColorSpace = surfaceFormat.colorSpace;
		ci.imageExtent = m_extent;
		ci.imageArrayLayers = 1;
		ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.preTransform = caps.currentTransform;
		ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		
		if (vSync) {
			ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		}
		else {
			ci.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			// Todo: Check if MAILBOX is actually supported 
			//, otherwise use VK_PRESENT_MODE_IMMEDIATE_KHR.
		}

		ci.clipped = VK_TRUE;
		ci.oldSwapchain = oldHandle;

		if (vkCreateSwapchainKHR(m_context.device(), &ci, nullptr, &m_swapchain) != VK_SUCCESS) {
			throw std::runtime_error("Vulkan: Failed to create swapchain!");
		}

		uint32_t actualImageCount;
		vkGetSwapchainImagesKHR(m_context.device(), m_swapchain, &actualImageCount, nullptr);
		
		m_images.resize(actualImageCount);
		vkGetSwapchainImagesKHR(m_context.device(), m_swapchain, &actualImageCount, m_images.data());

		m_frames.resize(actualImageCount);

		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		for (uint32_t i = 0; i < actualImageCount; i++) {
			m_frames[i].image = std::make_unique<VulkanImage>(
				m_context,
				m_images[i],
				m_imageFormat,
				m_extent
			);

			// Create the per-image semaphore for presentation sync
			if (vkCreateSemaphore(m_context.device(), &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore) != VK_SUCCESS) {
				throw std::runtime_error("Vulkan: Failed to create swapchain semaphores!");
			}
		}
	}
	
	VkResult VulkanSwapchain::acquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex)
	{
		return vkAcquireNextImageKHR(
			m_context.device(),
			m_swapchain,
			UINT64_MAX,
			signalSemaphore,
			VK_NULL_HANDLE,
			imageIndex);
	}

	VkSurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) 
	{
		for (const auto& fmt : formats) {
			if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && 
				fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return fmt;
		}
		return formats[0];
	}

	VulkanImage* VulkanSwapchain::getImageWrapper(uint32_t index)
	{
		if (index >= m_frames.size()) {
			spdlog::error("VulkanSwapchain: Attempted to access image index {} out of bounds ({})", index, m_frames.size());
			return nullptr;
		}
		return m_frames[index].image.get();
	}
}