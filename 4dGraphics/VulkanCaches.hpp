#pragma once

// for handle types; could use VK_DEFINE_NON_DISPATCHABLE_HANDLE
#include "VulkanConstructs.h"
#include "VulkanHelpers.h"

#include <exception>
#include <stdexcept>
#include <future>
#include <mutex>
#include <random>
#include <stdexcept>
#include <system_error>
#include <taskflow/core/executor.hpp>
#include <utility>
#include <vector>
#include <span>
#include <shared_mutex>
#include <tuple>
#include <concepts>
#include <atomic>
#include <algorithm>
#include <type_traits>
#include <taskflow/taskflow.hpp>
#include <vulkan/vulkan_core.h>
#include <filesystem>

/*
contains handle descriptors

the same types as steam's Fossilize
    VkSampler
    VkDescriptorSetLayout
    VkPipelineLayout
    VkRenderPass
    VkShaderModule
    VkPipeline (compute/graphics/?? raytracing)
    ?? VkSamplerYcbcrConversion
*/
namespace HDesc
{
    namespace util
    {
        struct CacheInfo{
            VkDevice dev;
            VkAllocationCallbacks *ac;
        };

        template<typename T>
        using Ref = std::reference_wrapper<T>;

        enum class handle_kind {            
            // non-dynamic cheap to make handles
            cheap = 0,
            
            // possibly dynamic, costly to create handles
            defered = 1,

            // possibly dynamic, costly to create handles used to create other handles
            // e.g for pipeline libraries
            defered_for_share = 2
        };

        template<typename T>
        concept HandleDescriptor = requires(
            T &ref,
            const T &cref,
            typename T::handle_data &d,
            typename T::handle_type h )
        {
            typename T::handle_type;
            typename T::handle_data;

            T::dynamic;
            T::kind;

            ref.normalize();
            cref == cref;
            { cref.hash() } -> std::convertible_to<std::size_t>;
            { cref.create(d) } -> std::same_as<typename T::handle_type>;
            cref.destroy(d, std::move(h));
        };

        template<HandleDescriptor handle_desc, typename Handle>
        class handle_cache_base {
        public:
            using handle_type = typename handle_desc::handle_type;
            using handle_data = typename handle_desc::handle_data;
            using handle = Handle;

            handle_cache_base( handle_data priv, size_t hold_len = 1 )
                : priv_(std::move(priv)), hold_idx_(0), hold_queues_(hold_len)
            {
                if( hold_len == 0 )
                    throw std::invalid_argument( "handle cache must not have hold queue of length 0" );
            }

            ~handle_cache_base()
            {
                hold_queues_.clear();
                clear_destr_queue();

                if( !old_.empty() || !all_.empty() )
                    TRACE( DebugLevel::FatalError, "not all handles released before cache destruction" );
            }
        
        private:
            struct handle_hash {
                size_t operator()( const handle_desc &h ) const { return h.hash(); }
            };

            using desc_ref = Ref<const handle_desc>;

            template<typename U, typename T>
            using map_type = std::unordered_map<U,T,handle_hash>;

        public:
            using shared_ptr = std::shared_ptr<Handle>;
            using weak_ptr = std::weak_ptr<Handle>;

            // lockless, already created at least frame before
            map_type<desc_ref, weak_ptr> old_;

            // stores handle and atomic bool telling if the object is already constucted
            mutable std::shared_mutex mut_;
            map_type<handle_desc, weak_ptr> all_;

            // stores uncommited handles
            std::vector<std::pair<desc_ref,weak_ptr>> new_;

        private:
            void push_new_handles()
            {
                // lock also destr_mut_ to prevent reference from going stale
                std::scoped_lock l1(destr_mut_, mut_);

                for( const std::pair<desc_ref,weak_ptr> &p : new_ )
                {
                    shared_ptr p2 = p.second.lock();
                    if( p2 ) old_.insert( p.first, p2 );
                }
            }

            // prevent create-destroy-create
            mutable std::mutex hold_mut_;
            size_t hold_idx_;
            std::vector<std::vector<shared_ptr>> hold_queues_;

            void advance_hold_queue()
            {
                if( hold_queues_.empty() ) return;

                std::scoped_lock lock(hold_mut_);
                hold_idx_ = (1 + hold_idx_) % hold_queues_.size();
                hold_queues_[hold_idx_].clear();
            }

        public:
            void add_to_hold( shared_ptr s )
            {
                if( hold_queues_.empty() || !s ) return;

                std::scoped_lock lock(hold_mut_);
                hold_queues_[hold_idx_].emplace_back(std::move(s));
            }

        private:
            mutable std::mutex destr_mut_;
            std::vector<desc_ref> destr_queue_;

            void clear_destr_queue()
            {
                std::scoped_lock lock(destr_mut_);
                for( const handle_desc &h : destr_queue_ )
                {
                    old_.erase(h);
                    all_.erase(h); // after this h is no longer valid
                }

                destr_queue_.clear();
            }

        public:
            void destroy_erase( const handle_desc &desc, handle_type &&h )
            {
                desc.destroy( priv_, std::move(h) );

                std::scoped_lock lock(destr_mut_);
                destr_queue_.emplace_back(h);
            }

            handle_type create_from( const handle_desc &desc ) const
            {
                return desc.create( priv_ );
            }

            void flush()
            {
                push_new_handles();
                advance_hold_queue();
                clear_destr_queue();
            }

        private:
            handle_data priv_;
        };

        template<HandleDescriptor handle_desc, handle_kind kind = handle_desc::kind>
        class handle_cache;

        // cache handles that are cheap to create => no need do defer
        template<HandleDescriptor handle_desc>
        class handle_cache<handle_desc, handle_kind::cheap> {
        public:
            static_assert( !handle_desc::dynamic, "cheap handles must not be dynamic" );

            using handle_type = typename handle_desc::handle_type;
            using handle_data = typename handle_desc::handle_data;

            handle_cache( handle_data priv, size_t hold_len = 1 )
                : cache( std::move( priv ), hold_len ) {}

        private:
            struct destr_handle;

            using cache_t = handle_cache_base<handle_desc, destr_handle>;
            using shared_ptr = std::shared_ptr<destr_handle>;
            using weak_ptr = std::weak_ptr<destr_handle>;

            struct destr_handle {
                destr_handle( handle_type h, const handle_desc &desc, cache_t &cache )
                    : handle(h), desc(desc), cache(cache) {}

                ~destr_handle() {
                    cache.destroy_erase( desc, std::move(handle) );
                }

                handle_type handle;
                const handle_desc &desc;
                cache_t &cache;
            };
        
        public:
            class shared_handle {
            public:
                shared_handle() = default;
                shared_handle(const shared_handle&) = default; 
                shared_handle(shared_handle&&) = default;

                shared_handle &operator=(const shared_handle&) = default;
                shared_handle &operator=(shared_handle&&) = default;
                
                const handle_type &get() const {
                   if( !payload )
                       throw std::logic_error("dereferenced invalid handle");
                   
                   return payload->handle;
                }
                
                operator const handle_type&() const { return get(); }

                bool valid() const { return (bool)payload; }

                ~shared_handle()
                {
                    if( !payload ) return;

                    cache_t &owner = payload->owner.cache;
                    owner.add_to_hold( std::move(payload) );
                }

            private:
                shared_handle( shared_ptr pl )
                    : payload( std::move(pl) ) {}

                shared_ptr payload;
                friend handle_cache;
            };

            void flush() { cache.flush(); }
            shared_handle query( handle_desc desc );

        private:
            cache_t cache;
        };

        template<HandleDescriptor handle_desc>
        auto handle_cache<handle_desc, handle_kind::cheap>::query( handle_desc desc ) -> shared_handle
        {
            desc.normalize();
            if( auto p = cache.old_.find(desc); p != cache.old_.end() )
                if( shared_ptr p2 = p->lock(); p2 )
                    return p2;

            {
                std::shared_lock l1(cache.mut_);

                if( auto p = cache.all_.find(desc); p != cache.all_.end() )
                    if( shared_ptr p2 = p->lock(); p2 )
                        return p2;
            }

            {
                std::unique_lock l2(cache.mut_);

                // check if it was not created during locking
                if( auto p = cache.all_.find(desc); p != cache.all_.end() )
                    if( shared_ptr p2 = p->lock(); p2 )
                        return p2;

                auto it = cache.all_.emplace( std::move(desc), nullptr );

                shared_ptr obj;

                try
                {
                    handle_type handle = cache.create_from( it->first );

                    try {
                        obj = std::make_shared<destr_handle>( handle, it->first, cache ); 
                    } catch(...) {
                        it->first.destroy( cache.priv_, std::move( handle ) );
                        throw;
                    }

                    it->second = obj;
                    cache.new_.emplace_back( it->first, obj );
                }
                catch(...)
                {
                    obj = nullptr;
                    cache.all_.erase( it );
                    throw;
                }

                return obj;
            }
        }

        // cache for objects that might need deffering => creates shared futures
        template<HandleDescriptor handle_desc>
        class handle_cache<handle_desc, handle_kind::defered> {
        public:
            using handle_type = typename handle_desc::handle_type;
            using handle_data = typename handle_desc::handle_data;

        private:
            struct destr_handle_future;
            using cache_t = handle_cache_base<handle_desc,destr_handle_future>;
            using shared_ptr = std::shared_ptr<destr_handle_future>;
            using weak_ptr = std::weak_ptr<destr_handle_future>;

            struct destr_handle_future {
                destr_handle_future( const handle_desc &desc, cache_t &cache )
                    : desc(desc)
                    , cache(cache) {}

                ~destr_handle_future()
                {
                    cache.destroy_erase( desc, std::move(handle) );
                }
                
                std::atomic<bool> ready;
                handle_type handle;
                std::exception_ptr exception;

                const handle_desc &desc;
                cache_t &cache;
            };

        public:
            handle_cache( handle_data priv, size_t frames_in_flight, size_t hold_len = 1 )
                : cache( std::move(priv), hold_len ), upd( frames_in_flight ) {}

            class shared_handle {
                const handle_type &get() const {
                    assert_valid();
                    
                    // TODO test if wait skip is beneficial
                    if( !pl->ready.load( std::memory_order_acquire ) )
                        pl->ready.wait( false, std::memory_order_acquire );
                    
                    return get_handle();
                }

                operator const handle_type&() const { return get(); }

                bool is_ready() const
                {
                    assert_valid();
                    return pl->ready.load( std::memory_order_relaxed );
                }

                const handle_type *get_if_ready() const
                {
                    return is_ready() ? &get_handle() : nullptr;
                }

                shared_handle() = default;
                ~shared_handle()
                {
                    if( !pl ) return;

                    cache_t &cache = pl->cache;
                    cache.add_to_hold( std::move(pl) );
                }

            private:
                shared_handle( shared_ptr pl )
                    : pl( std::move(pl) ) {}

                void assert_valid() const
                {
                    if( !pl ) throw std::runtime_error("dereferenced invalid handle");
                }
                
                // must be ready (and memory acquired) during the call to get_handle
                const handle_type &get_handle() const
                {
                    if( pl->exception ) 
                        std::rethrow_exception( pl->exception );
                    
                    return pl->handle;
                }

                shared_ptr pl;
                friend handle_cache;
            };

            // query resources and create if not present
            // it will launch async task to create it if tf::Executor is provided
            shared_handle query_create( handle_desc desc, tf::Executor *ex = nullptr )
            {
                desc.normalize();
                if( shared_ptr h = query_( desc ); h ) return h;
                return create_(desc, ex);
            }

            std::vector<shared_handle> bulk_query_create( std::vector<handle_desc> descs, tf::Executor *ex = nullptr );

            // those functions are safe to call even when flush() is called
            shared_handle create( handle_desc desc, tf::Executor *ex = nullptr ) { desc.normalize(); return create_(desc,ex); }
            std::vector<shared_handle> bulk_create( std::vector<handle_desc> descs, tf::Executor *ex = nullptr );

            void flush() { upd.flush(); cache.flush(); }

        private:
            shared_ptr query_( const handle_desc &desc )
            {
                auto p = cache.old_.find(desc);
                return p != cache.old_.end() ? p->lock() : nullptr;
            }

            shared_handle create_( const handle_desc &desc, tf::Executor *ex );
            shared_handle create_handle( shared_ptr desc, tf::Executor *ex ) noexcept;

            struct empty_updater_t {
                void flush() {}
                empty_updater_t(size_t) {}
            };

            struct update_handle_t { 
                struct handle_destructor {
                    ~handle_destructor()
                    {
                        if( !h ) return;

                        assert( pl && pl->desc );
                        pl->desc.destroy( pl->owner.priv_, std::move(h) );
                    }

                    bool valid() { return pl && pl->desc; }

                    shared_ptr pl;

                    handle_type h;
                    std::exception_ptr exc;
                };

                std::mutex update_mut_;
                std::vector<handle_destructor> updates_;
                std::vector<std::vector<handle_destructor>> update_hold_queue_;
                size_t update_hold_queue_idx;

                update_handle_t( size_t fif )
                    : update_hold_queue_(fif)
                    , update_hold_queue_idx(0) {}

                void flush()
                {
                    std::vector<handle_destructor> old_handles; // destroy after freeing the lock

                    std::unique_lock l(update_mut_);
                    for( handle_destructor &u : updates_ )
                    {
                        assert( u.valid() );
                        std::swap( u.pl->val.handle, u.h );
                        std::swap( u.pl->val.exception, u.exc );
                    }

                    // hold for frames_in_flight and destroy them afterwards
                    // must also hold the shared_ptr to avoid use-after-free hazard of handle_desc
                    // hold and destroy old_handles afterwards

                    if( !update_hold_queue_.empty() )
                    {
                        old_handles = std::exchange( update_hold_queue_[update_hold_queue_idx++], std::move( updates_ ) );
                        update_hold_queue_idx %= update_hold_queue_.size();
                    }
                    else old_handles = std::move( updates_ );
                }
            };

            using update_t = std::conditional_t<handle_desc::dynamic, update_handle_t, empty_updater_t>;

            [[no_unique_address]] cache_t cache;
            [[no_unique_address]] update_t upd;
        };

        template<HandleDescriptor handle_desc> auto
        handle_cache<handle_desc, handle_kind::defered>::create_( const handle_desc &desc, tf::Executor *ex ) -> shared_handle
        {
            {
                std::shared_lock lock(cache.mut_);
                if( auto p = cache.all_.find(desc); p != cache.all_.end() )
                    if( shared_ptr p2 = p->lock(); p2 )
                        return p2;
            }

            {
                std::unique_lock lock(cache.mut_);

                if( auto p = cache.all_.find(desc); p != cache.all_.end() )
                    if( shared_ptr p2 = p->lock(); p2 )
                        return p2;

                auto it = cache.all_.insert( std::move( desc ), nullptr );
                shared_ptr obj;

                try {
                    obj = std::make_shared<destr_handle_future>( it->first, *this );

                    it->second = obj;
                    cache.new_.emplace_back( it->first, obj );
                } catch( ... ) {
                    obj = nullptr;
                    cache.all_.erase( it );
                    throw;
                }

                return create_handle( std::move( obj ), ex );
            }
        }

        template<HandleDescriptor handle_desc> auto
        handle_cache<handle_desc, handle_kind::defered>::create_handle( shared_ptr h, tf::Executor *ex ) noexcept -> shared_handle
        {
            assert( h );

            auto create_handle_ = []( shared_ptr handle ) -> void {
                try {
                    handle->handle = handle->cache.create_from( handle->desc );
                } catch(...) {
                    handle->exception = std::current_exception();
                }

                handle->ready.store( true, std::memory_order_release );
                handle->ready.notify_all();
            };

            bool create_here = !ex;

            if( ex )
            {
                try { 
                    ex->silent_async("create handle", create_handle_, h );
                } catch( ... ) { // memory exception / ... ???
                    TRACE( DebugLevel::Error, "Handle creation task could not be created" );
                    create_here = true; // make sure handle is really during creation when we exit this function
                }
            }
            
            if( create_here )
                create_handle_( h );

            return h;
        }

        template<HandleDescriptor handle_desc> auto
        handle_cache<handle_desc, handle_kind::defered>::bulk_query_create( std::vector<handle_desc> descs, tf::Executor *ex )
            -> std::vector<shared_handle>
        {
            std::vector<shared_handle> res(descs.size(), nullptr);
            
            std::vector<handle_desc> desc2;
            std::vector<int> resOff;

            for( uint32_t i = 0; i < descs.size(); i++ )
            {
                handle_desc &d = descs[i];
                d.normalize();

                shared_ptr p = query_( descs[i] );
                if( p ) res[i] = std::move(p);
                else
                {
                    desc2.emplace_back( std::move(d) );
                    resOff.push_back( i );
                }
            }

            if( desc2.size() != 0 )
            {
                std::vector<shared_handle> b2 = bulk_create( desc2, ex );
                assert( desc2.size() == resOff.size() );
                assert( desc2.size() == b2.size() );

                for( size_t i = 0; i < resOff.size(); i++ )
                    res[resOff[i]] = std::move( b2[i] );
            }

            return res;
        }

        template<HandleDescriptor handle_desc> auto
        handle_cache<handle_desc, handle_kind::defered>::bulk_create( std::vector<handle_desc> descs, tf::Executor *ex )
            -> std::vector<shared_handle>
        {
            std::vector<shared_handle> res(descs.size(),nullptr);

            //if constexpr( !BulkHandleDescriptor<handle_desc> )
            {
                for( uint32_t i = 0; i < descs.size(); i++ )
                    res[i] = create( std::move(descs[i]), ex );
            }

            return res;
        }

        // stores file paths and update handlers
        // calls update handlers when file or its dependency updates
        // only for development
        class file_watcher
        {
        public:
            using callback_handle = uint64_t;
            using callback_type = std::function<void(file_watcher &, callback_handle)>;

            using path_t = std::filesystem::path;
            using ftime_t = std::filesystem::file_time_type;

            callback_handle add_callback( callback_type, 
                    const std::vector<std::pair<path_t,ftime_t>> & );
            void remove_callback( callback_handle );
            void update();

        private:
            struct file_data {
                file_data( const std::filesystem::path &, std::vector<callback_handle> = {} );

                std::filesystem::file_time_type ftime;
                std::vector<callback_handle> subscribed_callbacks; // flat_set
            };

            callback_handle lastHandle = 0;
            std::unordered_map<callback_handle, 
                std::pair<callback_type, std::vector<std::filesystem::path>>> callbacks;

            std::recursive_mutex mut;
            std::unordered_map<std::filesystem::path, file_data> files;
        };
    }

    class SamplerInfo {
    public:
        using handle_type = VkSampler;
        using handle_data = util::CacheInfo;
        static constexpr bool dynamic = false;
        static constexpr util::handle_kind kind = util::handle_kind::cheap;

        SamplerInfo( const VkSamplerCreateInfo &sci );
        SamplerInfo( VkSamplerCreateFlags flags = 0 );

        SamplerInfo &setFilters( 
                VkFilter minFilter = VK_FILTER_LINEAR, 
                VkFilter magFilter = VK_FILTER_LINEAR, 
                VkSamplerMipmapMode mipMode = VK_SAMPLER_MIPMAP_MODE_LINEAR );

        SamplerInfo &setAddressMode(
                VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT );

        SamplerInfo &setMipLodBias( float bias = 0.f );
        SamplerInfo &setAnisotropy( float ani = 0.f ); // 0 for disable
        SamplerInfo &setCompareOp( VkBool32 enable = false, VkCompareOp op = VK_COMPARE_OP_LESS_OR_EQUAL );
        SamplerInfo &setLodBounds( float minLod = 0.f, float maxLod = VK_LOD_CLAMP_NONE );
        SamplerInfo &setBorderColor( VkBorderColor color = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK );
        
        void normalize();
        bool operator==( const SamplerInfo & ) const;
        std::size_t hash() const;
        handle_type create(handle_data&) const;
        void destroy(handle_data&, handle_type) const;

    private:
        VkSamplerCreateInfo sci;
        //VkSamplerBorderColorComponentMappingCreateInfoEXT bcm;
        //VkSamplerCustomBorderColorCreateInfoEXT cbc;
        //VkSamplerReductionModeCreateInfo rm;
        //VkSamplerYcbcrConversionInfo (shared_ptr)
    };

    class DescriptorSetLayoutInfo {
    public:
        using handle_type = VkDescriptorSetLayout;
        using handle_data = util::CacheInfo;
        static constexpr bool dynamic = false;
        static constexpr util::handle_kind kind = util::handle_kind::cheap;

        DescriptorSetLayoutInfo( const VkDescriptorSetLayoutCreateInfo &DSLci );
        DescriptorSetLayoutInfo( VkDescriptorSetLayoutCreateFlags flags = 0 ) : flags(flags) {};

        DescriptorSetLayoutInfo &add_binding(
            uint32_t binding, VkDescriptorType type, uint32_t count,
            VkShaderStageFlags stages, const VkSampler *pImmutableSamplers = nullptr,
            VkDescriptorBindingFlags flags = 0 );

        void normalize();
        bool operator==(const DescriptorSetLayoutInfo&) const;
        size_t hash() const;
        handle_type create(handle_data& ) const;
        void destroy(handle_data&, handle_type ) const;

    private:
        VkDescriptorSetLayoutCreateFlags flags;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<std::vector<VkSampler>> bindSamplers;
        std::vector<VkDescriptorBindingFlags> bindFlags;
    };

    class PipelineLayoutInfo {
    public:
        using handle_type = VkPipelineLayout;
        using handle_data = util::CacheInfo;
        static constexpr bool dynamic = false;
        static constexpr util::handle_kind kind = util::handle_kind::cheap;

        PipelineLayoutInfo( const VkPipelineLayoutCreateInfo &PLci );
        PipelineLayoutInfo( VkPipelineLayoutCreateFlags flags = 0 ) : flags(flags) {};

        PipelineLayoutInfo &add_set( VkDescriptorSetLayout set ) { setLayouts.push_back(set); return *this; };
        PipelineLayoutInfo &add_push( VkPushConstantRange range ) { pushRanges.push_back(range); return *this; };

        void normalize();
        bool operator==( const PipelineLayoutInfo & ) const;
        size_t hash() const;
        handle_type create(handle_data& ) const;
        void destroy(handle_data&, handle_type ) const;

    private:
        VkPipelineLayoutCreateFlags flags;
        std::vector<VkDescriptorSetLayout> setLayouts; // should be owning reference
        std::vector<VkPushConstantRange> pushRanges;
    };

    // TODO
    class RenderPassInfo {
    public:
        using handle_type = VkPipelineLayout;
        using handle_data = util::CacheInfo;
        static constexpr bool b_dynamic_handle = false;

        RenderPassInfo( const VkRenderPassCreateInfo2 &RPci2 );
        RenderPassInfo( VkRenderPassCreateFlags flags = 0 ) : flags(flags) {};

        void normalize();
        size_t hash() const;
        VkResult create(handle_data&, VkDevice, VkAllocationCallbacks*, handle_type& ) const;
        static void destroy(handle_data&, VkDevice, VkAllocationCallbacks*, handle_type& );

    private:
        VkRenderPassCreateFlags flags;
    };

    // TODO
    class ShaderModuleInfo {
    public:
        using handle_type = VkShaderModule;
        using handle_data = util::CacheInfo;
        static constexpr bool dynamic = true;
        static constexpr util::handle_kind kind = util::handle_kind::defered;

    private:
        std::filesystem::path shaderfile;
    };

    // TODO
    class GraphicsPipelineInfo {
    public:
        using handle_type = VkPipeline;
        using handle_data = util::CacheInfo;
        static constexpr bool b_dynamic_handle = true;
    };

    // TODO
    class ComputePipelineInfo {
    public:
        using handle_type = VkPipeline;
        using handle_data = util::CacheInfo;
        static constexpr bool b_dynamic_handle = true;
    };

    // TODO
    class RayTracingPipelineInfo {
    public:
        using handle_type = VkPipeline;
        using handle_data = util::CacheInfo;
        static constexpr bool b_dynamic_handle = true;
    };

    // TODO
    class SamplerYcbcrConversionInfo {
    public:
        using handle_type = VkSamplerYcbcrConversion;
        using handle_data = util::CacheInfo;
        static constexpr bool b_dynamic_handle = false;
    };

    //typedef util::handle_cache<SamplerYcbcrConversionInfo> SamplerYcbcrConversionCache;
    typedef util::handle_cache<SamplerInfo> SamplerCache;
    typedef util::handle_cache<DescriptorSetLayoutInfo> DescriptorSetLayoutCache;
    typedef util::handle_cache<PipelineLayoutInfo> PipelineLayoutCache;
    //typedef util::handle_cache<RenderPassInfo> RenderPassCache;
    //typedef util::handle_cache<ShaderModuleInfo> ShaderModuleCache;
    //typedef util::handle_cache<GraphicsPipelineInfo> GraphicsPipelineCache;
    //typedef util::handle_cache<ComputePipelineInfo> ComputePipelineCache;
    //typedef util::handle_cache<RayTracingPipelineInfo> RayTracingPipelineCache;
}
