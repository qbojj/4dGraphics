#pragma once

#include "VulkanConstructs.h"
#include <stdint.h>
#include <mutex>
#include <vector>

class VulkanDevice;

namespace DSAlloc {
	class VulkanDSAllocatorPool;

	class VulkanDSAllocator {
	private:
		VulkanDSAllocator(VulkanDSAllocatorPool *owner, VkDescriptorPool pool)
			: owner(owner), pool(pool) {}

	public:
		VulkanDSAllocator(VulkanDSAllocatorPool &owner) : owner(&owner), pool(VK_NULL_HANDLE) {}
		
		~VulkanDSAllocator() { ret(false); }

		VulkanDSAllocator(const VulkanDSAllocator&) = delete;
		VulkanDSAllocator(VulkanDSAllocator&&);

		VulkanDSAllocator&operator=(VulkanDSAllocator);

		std::vector<VkDescriptorSet> allocate(uint32_t count, 
			const VkDescriptorSetLayout *pSetLayouts, void *pNext = nullptr);
	
	private:
		void ret(bool full);

		VulkanDSAllocatorPool *owner;
		VkDescriptorPool pool;

		friend VulkanDSAllocatorPool;
	};

	class VulkanDSAllocatorPool {
	public:
		VulkanDSAllocatorPool( const VulkanDevice &device, 
			const VkDescriptorPoolCreateInfo *pDPci, uint32_t frames_in_flight,
			std::string name, const VkAllocationCallbacks *pAllocator = nullptr );
		~VulkanDSAllocatorPool();

		void advance_frame(VkBool32 trim = false, VkDescriptorPoolResetFlags ResetFlags = 0);
		
		VulkanDSAllocator get_allocator();
		void ret_allocator(VulkanDSAllocator& handle, bool bIsFull);
		
	private:
		const VulkanDevice &device;
		const VkDescriptorPoolCreateInfo *pDPci;
		const VkAllocationCallbacks *pAllocator;
		const std::string name;

		std::mutex mut_;
		uint32_t frameIdx;

		using dpvec = std::vector<VkDescriptorPool>;
		struct frame_storage {
			dpvec usable, full;
		};

		std::vector<frame_storage> per_frame_pools;
		std::vector<VkDescriptorPool> clean_pools;

		friend VulkanDSAllocator;
	};
}