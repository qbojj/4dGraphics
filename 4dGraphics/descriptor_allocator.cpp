#include "descriptor_allocator.h"
#include "VulkanConstructs.h"

#include "Debug.h"

namespace DSAlloc {
	VulkanDSAllocator::VulkanDSAllocator(VulkanDSAllocator&&o)
		: owner(o.owner), pool(o.pool)
	{
		o.owner = nullptr;
		pool = VK_NULL_HANDLE;
	}

	
	VulkanDSAllocator&VulkanDSAllocator::operator=(VulkanDSAllocator o)
	{
		std::swap(owner, o.owner);
		std::swap(pool, o.pool);
		return *this;
	}

	std::vector<VkDescriptorSet> VulkanDSAllocator::allocate(uint32_t count, 
			const VkDescriptorSetLayout *pSetLayouts, void *pNext)
	{
		VkDescriptorSetAllocateInfo DSai {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = pNext,
			.descriptorPool = nullptr,
			.descriptorSetCount = count,
			.pSetLayouts = pSetLayouts
		};

		std::vector<VkDescriptorSet> out( count, VK_NULL_HANDLE );
		VkResult res = VK_SUCCESS;

		for( uint32_t i = 0; i < 64; i++ )
		{
			if( !pool ) *this = owner->get_allocator();

			DSai.descriptorPool = pool;
			res = vkAllocateDescriptorSets(owner->device.device, &DSai, out.data() );
			if( res == VK_SUCCESS ) return out;
			
			// not a memory error?
			if( res != VK_ERROR_FRAGMENTED_POOL && res != VK_ERROR_OUT_OF_POOL_MEMORY )
				vk_err::throwResultException(res, "DS allocator could not allocate");
			
			// pool is full - try again
			ret(true);
		}

		// we failed multiple times. something is very wrong.
		vk_err::throwResultException(res, "DS allocator could not allocate");
	}

	void VulkanDSAllocator::ret(bool full) { owner->ret_allocator(*this, full); }

	VulkanDSAllocatorPool::VulkanDSAllocatorPool( const VulkanDevice &device, 
		const VkDescriptorPoolCreateInfo *pDPci, uint32_t frames_in_flight,
		std::string name, const VkAllocationCallbacks *pAllocator )
		: device(device)
		, pDPci(pDPci)
		, pAllocator(pAllocator)
		, name(std::move(name))
		, per_frame_pools(frames_in_flight) {}

	VulkanDSAllocatorPool::~VulkanDSAllocatorPool()
	{
		for (VkDescriptorPool pool : clean_pools)
			vkDestroyDescriptorPool(device.device, pool, pAllocator);
		
		for (const frame_storage &storage : per_frame_pools)
		{
			for (VkDescriptorPool pool : storage.usable)
				vkDestroyDescriptorPool(device.device, pool, pAllocator);

			for (VkDescriptorPool pool : storage.full)
				vkDestroyDescriptorPool(device.device, pool, pAllocator);
		}
	}

	void VulkanDSAllocatorPool::advance_frame(VkBool32 trim, VkDescriptorPoolResetFlags ResetFlags)
	{
		std::unique_lock lock(mut_);

		if( trim )
			for(VkDescriptorPool pool : clean_pools)
				vkDestroyDescriptorPool(device.device, pool, pAllocator);

		for (VkDescriptorPool pool : per_frame_pools[frameIdx].full ) {
			vkResetDescriptorPool(device.device, pool, ResetFlags);
			clean_pools.push_back(pool);
		}

		for (VkDescriptorPool pool : per_frame_pools[frameIdx].usable ) {
			vkResetDescriptorPool(device.device, pool, ResetFlags);
			clean_pools.push_back(pool);
		}

		per_frame_pools[frameIdx].usable.clear();
		per_frame_pools[frameIdx].full.clear();
	}

	void VulkanDSAllocatorPool::ret_allocator(VulkanDSAllocator& handle, bool bIsFull)
	{
		std::unique_lock lock(mut_);
		
		if (bIsFull)
			per_frame_pools[frameIdx].full.push_back( handle.pool );
		else 
			per_frame_pools[frameIdx].usable.push_back( handle.pool );
		
		handle.pool = VK_NULL_HANDLE;
	}

	VulkanDSAllocator VulkanDSAllocatorPool::get_allocator()
	{
		std::unique_lock lock(mut_);
		
		VkDescriptorPool pool = VK_NULL_HANDLE;
		
		if (per_frame_pools[frameIdx].usable.size() > 0)
		{
			pool = per_frame_pools[frameIdx].usable.back();
			per_frame_pools[frameIdx].usable.pop_back();

			return { this, pool };
		}
		
		if (clean_pools.size() != 0)
		{
			pool = clean_pools.back();
			clean_pools.pop_back();

			return { this, pool };
		}
		
		CREATE_THROW( vkCreateDescriptorPool(device.device, pDPci, pAllocator, &tmp_ ),
			pool, name + ": could not allocate descriptor pool" );
		
		set_vk_name( device, pool, name + ": descriptor pool" );

		return { this, pool };
	}
}