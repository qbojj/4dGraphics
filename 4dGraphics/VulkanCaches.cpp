#include "VulkanCaches.hpp"
#include "VulkanHelpers.h"
#include "volk.h"
#include <span>
#include <string>
#include <vector>
#include <exception>
#include <utility>
#include <filesystem>
#include <set>
#include <vulkan/vulkan_core.h>
#include "Debug.h"

using namespace std;
namespace fs = std::filesystem;
using namespace HDesc;
using namespace HDesc::util;

template<typename T>
static inline void hash_combine(size_t &seed, const T&obj)
{
    seed ^= hash<T>{}(obj) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

static inline void report_unknown_stype(VkStructureType t, const char *where)
{
    throw runtime_error(std::string() + 
        "not supported sType (" + string_from_VkStructureType( t ) + ") in " + where );
}

SamplerInfo::SamplerInfo( const VkSamplerCreateInfo &sc ) : SamplerInfo( sc.flags )
{
    if( sc.pNext ) 
        report_unknown_stype( ((const VkBaseInStructure*)sc.pNext)->sType, 
                "pNext of VkSamplerCreateInfo in SamplerInfo" );

    setFilters( sc.minFilter, sc.magFilter );
    setAddressMode( sc.addressModeU, sc.addressModeV, sc.addressModeW );
    setMipLodBias( sc.mipLodBias );
    setAnisotropy( sc.anisotropyEnable ? sc.maxAnisotropy : 0.f );
    setCompareOp( sc.compareEnable, sc.compareOp );
    setLodBounds( sc.minLod, sc.maxLod );
    setBorderColor( sc.borderColor );
}

SamplerInfo::SamplerInfo( VkSamplerCreateFlags flags )
{
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.pNext = nullptr;
    sci.flags = flags;

    setFilters();
    setAddressMode();
    setMipLodBias();
    setAnisotropy();
    setCompareOp();
    setLodBounds();
    setBorderColor();
 }

SamplerInfo &SamplerInfo::setFilters( VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipMode )
{
    sci.minFilter = minFilter;
    sci.magFilter = magFilter;
    sci.mipmapMode = mipMode;
    return *this;
}

SamplerInfo &SamplerInfo::setAddressMode(
        VkSamplerAddressMode addressModeU,
        VkSamplerAddressMode addressModeV,
        VkSamplerAddressMode addressModeW )
{
    sci.addressModeU = addressModeU;
    sci.addressModeV = addressModeV;
    sci.addressModeW = addressModeW;
    return *this;
}

SamplerInfo &SamplerInfo::setMipLodBias( float bias ) { sci.mipLodBias = bias; return *this; }
SamplerInfo &SamplerInfo::setAnisotropy( float ani )
{
    sci.anisotropyEnable = ani != 0.f;
    sci.maxAnisotropy = ani;
    return *this;
}

SamplerInfo &SamplerInfo::setCompareOp( VkBool32 enable, VkCompareOp op )
{
    sci.compareEnable = enable;
    sci.compareOp = op;
    return *this;
}

SamplerInfo &SamplerInfo::setLodBounds( float minLod, float maxLod )
{
    sci.minLod = minLod;
    sci.maxLod = maxLod;
    return *this;
}

SamplerInfo &SamplerInfo::setBorderColor( VkBorderColor color ) { sci.borderColor = color; return *this; }
void SamplerInfo::normalize() {}

size_t SamplerInfo::hash() const
{
    size_t seed = 0;
    for( size_t off = offsetof( SamplerInfo, sci ); off < sizeof( sci ); ++off )
        hash_combine( seed, ( (const char *)&sci )[off] );
    
    return seed;
}

bool SamplerInfo::operator==( const SamplerInfo &rhs ) const
{
    return memcmp( &sci.flags, &rhs.sci.flags, sizeof(sci) - offsetof( SamplerInfo, sci ) ) == 0;
}

VkSampler SamplerInfo::create( handle_data &data ) const
{
    VkSampler s;
    CHECK_THROW( vkCreateSampler( data.dev, &sci, data.ac, &s ), "could not create sampler" );
    return s;
}

void SamplerInfo::destroy( handle_data &data, VkSampler sampler ) const
{
    if( sampler ) vkDestroySampler( data.dev, sampler, data.ac );
}

DescriptorSetLayoutInfo::DescriptorSetLayoutInfo( const VkDescriptorSetLayoutCreateInfo &DSLci )
    : flags( DSLci.flags )
{
    const VkBaseInStructure *cur = (const VkBaseInStructure *)DSLci.pNext;
    
    const VkDescriptorSetLayoutBindingFlagsCreateInfo *pBFci;
    while( cur )
    {
        switch( cur->sType )
        {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
            pBFci = (const VkDescriptorSetLayoutBindingFlagsCreateInfo *)cur;
            break;
        default:
            report_unknown_stype( cur->sType, 
                    "pNext in VkDescriptorSetLayoutInfo in DescriptorSetLayoutInfo");
        }
    }

    for( uint32_t i = 0; i < DSLci.bindingCount; i++ )
    {
        const auto &b = DSLci.pBindings[i];
        add_binding( b.binding, b.descriptorType, b.descriptorCount, b.stageFlags,
            b.pImmutableSamplers, pBFci && pBFci->bindingCount ? pBFci->pBindingFlags[i] : 0 );
    }
}

void DescriptorSetLayoutInfo::normalize()
{
	std::vector<int> perm(bindings.size());
	std::iota(perm.begin(),perm.end(),0);
	
	std::sort( perm.begin(), perm.end(), [this](int a, int b){
		return bindings[a].binding < bindings[b].binding;
	});

	// apply the permutation
	for( int a = 0; a < (int)perm.size(); a++ )
		for( int last = a, i = perm[a]; i != -1; last = i, i = perm[i], perm[last] = -1 )
		{
			swap( bindings[last], bindings[i] );
			swap( bindFlags[last], bindFlags[i] );
            swap( bindSamplers[last], bindSamplers[i] );
		}

    bool bindFlagsEmpty = false;
	for( auto &f : bindFlags )
		if( f != 0 )
			bindFlagsEmpty = true;

	bindFlags.resize( bindFlagsEmpty ? bindings.size() : 0, 0 );

    bindings.shrink_to_fit();
    bindFlags.shrink_to_fit();
    bindSamplers.shrink_to_fit();
}

size_t DescriptorSetLayoutInfo::hash() const
{
	size_t seed = bindings.size();
	hash_combine( seed, flags );

    for( uint32_t i = 0; i < bindings.size(); i++ )
    {
        hash_combine( seed, bindings[i].binding );
        hash_combine( seed, bindings[i].descriptorType );
        hash_combine( seed, bindings[i].descriptorCount );
        hash_combine( seed, bindings[i].stageFlags );
        // do not add to hash pImmutable samplers => pointer (hash samplers themselves)

        hash_combine( seed, bindFlags.empty() ? 0 : bindFlags[i] );

        for( const VkSampler &s : bindSamplers[i] )
            hash_combine( seed, s );
    }

	return seed;
}

DescriptorSetLayoutInfo &DescriptorSetLayoutInfo::add_binding( 
		uint32_t binding, VkDescriptorType type, uint32_t count,
		VkShaderStageFlags stages, const VkSampler *pImmutableSamplers,
		VkDescriptorBindingFlags flags )
{
	bindSamplers.emplace_back( pImmutableSamplers, pImmutableSamplers + (pImmutableSamplers ? count : 0) );
	bindFlags.emplace_back( flags );
	bindings.push_back( VkDescriptorSetLayoutBinding{ 
            binding, 
            type, 
            count, 
            stages, 
            bindSamplers.back().data()
    } );

	return *this;
}

bool DescriptorSetLayoutInfo::operator==( const DescriptorSetLayoutInfo &o ) const
{
	if( flags != o.flags ||
        bindFlags != o.bindFlags ||
        bindSamplers != o.bindSamplers ||
        bindings.size() != o.bindings.size() )
        return false;

    // bindings == o.bindings without samplers
    for( uint32_t i = 0; i < (uint32_t)bindings.size(); i++ )
    {
        const auto &a = bindings[i];
        const auto &b = o.bindings[i];

        if( a.binding != b.binding ||
            a.descriptorType != b.descriptorType ||
            a.descriptorCount != b.descriptorCount ||
            a.stageFlags != b.stageFlags )
            return false;
    }

    return true;
}

VkDescriptorSetLayout DescriptorSetLayoutInfo::create( handle_data &data ) const
{
    VkDescriptorSetLayoutBindingFlagsCreateInfo dslbfci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = (uint32_t)bindFlags.size(),
        .pBindingFlags = bindFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo dslci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &dslbfci,
        .flags = flags,
        .bindingCount = (uint32_t)bindings.size(),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout h;

    CHECK_THROW( vkCreateDescriptorSetLayout( data.dev, &dslci, data.ac, &h ),
            "cannot create descriptor set layout" );

    return h;
}

void DescriptorSetLayoutInfo::destroy(handle_data &data, VkDescriptorSetLayout h ) const
{
    if( h ) vkDestroyDescriptorSetLayout( data.dev, h, data.ac );
}

PipelineLayoutInfo::PipelineLayoutInfo( const VkPipelineLayoutCreateInfo &PLci )
    : PipelineLayoutInfo( PLci.flags )
{
    if( PLci.pNext )
        report_unknown_stype( ((VkBaseInStructure *)PLci.pNext)->sType, 
                "pNext of VkPipelineLayoutCreateInfo in PipelineLayoutInfo" );

    setLayouts.assign( PLci.pSetLayouts, PLci.pSetLayouts + PLci.setLayoutCount );
    pushRanges.assign( PLci.pPushConstantRanges, PLci.pPushConstantRanges + PLci.pushConstantRangeCount );
}

void normalize() {};
bool PipelineLayoutInfo::operator==( const PipelineLayoutInfo &o ) const
{
    return flags == o.flags && setLayouts == o.setLayouts && 
        pushRanges.size() == o.pushRanges.size() && 
        memcmp( pushRanges.data(), o.pushRanges.data(), 
                pushRanges.size() * sizeof(VkPushConstantRange) ) == 0;
}

size_t PipelineLayoutInfo::hash() const
{
    size_t seed = 0;
    hash_combine( seed, flags );
    for( const VkDescriptorSetLayout &dsl : setLayouts )
        hash_combine( seed, dsl );
    for( const VkPushConstantRange &psr : pushRanges )
        hash_combine( seed, psr.offset ),
        hash_combine( seed, psr.size ),
        hash_combine( seed, psr.stageFlags );
    return seed;
}

VkPipelineLayout PipelineLayoutInfo::create( handle_data &data ) const
{
    VkPipelineLayoutCreateInfo plci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .setLayoutCount = (uint32_t)setLayouts.size(),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = (uint32_t)pushRanges.size(),
        .pPushConstantRanges = pushRanges.data()
    };

    VkPipelineLayout h;
    CHECK_THROW( vkCreatePipelineLayout( data.dev, &plci, data.ac, &h ),
            "cannot create pipeline layout" );
    return h;
}

void PipelineLayoutInfo::destroy( handle_data &data, VkPipelineLayout h ) const
{
    if( h ) vkDestroyPipelineLayout( data.dev, h, data.ac );
}

file_watcher::file_data::file_data( const fs::path &file, vector<callback_handle> callbacks )
    : subscribed_callbacks( std::move( callbacks ) ) 
{
    error_code ec;
    ftime = last_write_time( file, ec );
    if( ec ) ftime = fs::file_time_type::min();
}

auto file_watcher::add_callback(callback_type callback, 
        const vector<pair<fs::path,fs::file_time_type>> &files ) -> callback_handle
{
    // TODO add strong exception safety
    lock_guard lock( mut );

    callback_handle h = ++lastHandle;
    vector<fs::path> paths;
    paths.reserve( files.size() );

    bool update = false;

    for( const auto &[ file, time ] : files )
    {
        fs::path file_abs = absolute( weakly_canonical( file ) );
        auto it = this->files.find( file_abs );

        if( it == this->files.end() )
            it = this->files.emplace( file_abs, file_abs ).first;

        assert( it != this->files.end() );
        if( it->second.ftime > time )
            update = true;

        it->second.subscribed_callbacks.push_back( h );
    }

    auto cb = callbacks.emplace( h, std::move( callback ), std::move( paths ) );

    if( update )
        cb.first->second.first( *this, h );

    return h;
}

void file_watcher::remove_callback( callback_handle h )
{
    lock_guard lock( mut );

    auto it = callbacks.find( h );
    if( it == callbacks.end() ) return;

    vector<fs::path> paths = std::move( it->second.second );
    callbacks.erase( it );

    for( const fs::path &p : paths )
    {
        auto fit = files.find( p );
        if( fit == files.end() ) continue;

        vector<callback_handle> &sub = fit->second.subscribed_callbacks;
        ptrdiff_t idx = find( sub.begin(), sub.end(), h ) - sub.begin();
        if( (size_t)idx + 1 != sub.size() )
           swap( sub[idx], sub.back() );

        sub.resize( sub.size() - 1 );

        if( sub.size() == 0 )
            files.erase( fit );
    }
}

void file_watcher::update()
{
    lock_guard lock( mut );
    set<callback_handle> to_update; // flat_set

    for( auto &[ file, fdata ] : files )
    {
        error_code ec;
        fs::file_time_type cur_ftime = last_write_time( file, ec );
        if( ec || cur_ftime <= fdata.ftime ) continue; // file removed or not changed

        // file changed
        fdata.ftime = cur_ftime;
       
        for( callback_handle h : fdata.subscribed_callbacks )
            to_update.insert( h );
    }

    for( callback_handle h : to_update )
        if( auto it = callbacks.find( h ); it != callbacks.end() )            
            it->second.first( *this, h );
}
