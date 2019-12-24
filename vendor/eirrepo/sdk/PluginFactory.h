/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/PluginFactory.h
*  PURPOSE:     Plugin factory templates
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_PLUGIN_FACTORY_HEADER_
#define _EIR_PLUGIN_FACTORY_HEADER_

#include "eirutils.h"
#include "MemoryUtils.h"
#include "MetaHelpers.h"

#include <atomic>
#include <algorithm>
#include <tuple>

// Prefer our own SDK types, because they are not tied to ridiculous allocator semantics.
#include "Vector.h"

// Include the helpers for actually handling plugin registrations and their validity.
#include "PluginFlavors.h"

// Class used to register anonymous structs that can be placed on top of a C++ type.
#define APSR_TEMPLARGS \
    template <typename abstractionType, typename pluginDescriptorType_meta, typename pluginAvailabilityDispatchType, typename allocatorType, typename... AdditionalArgs>
#define APSR_TEMPLUSE \
    <abstractionType, pluginDescriptorType_meta, pluginAvailabilityDispatchType, allocatorType, AdditionalArgs...>

APSR_TEMPLARGS
struct AnonymousPluginStructRegistry
{
    // Export for DynamicTypeSystem (DTS).
    typedef pluginDescriptorType_meta pluginDescriptorType;

    // A plugin struct registry is based on an infinite range of memory that can be allocated on, like a heap.
    // The structure of this memory heap is then applied onto the underlying type.
    typedef InfiniteCollisionlessBlockAllocator <size_t> blockAlloc_t;

    // TODO: actually make our own allocation semantics manager so we get rid of blockAlloc_t
    typedef blockAlloc_t::allocSemanticsManager allocSemanticsManager;

    blockAlloc_t pluginRegions;

    typedef typename pluginDescriptorType::pluginOffset_t pluginOffset_t;

    pluginOffset_t preferredAlignment;

    inline AnonymousPluginStructRegistry( void ) noexcept
    {
        this->preferredAlignment = (pluginOffset_t)sizeof(void*);
    }

    inline ~AnonymousPluginStructRegistry( void )
    {
        // TODO: allow custom memory allocators.

        // The runtime is allowed to destroy with plugins still attached, so clean up.
        while ( this->regPlugins.GetCount() != 0 )
        {
            registered_plugin& thePlugin = this->regPlugins[ 0 ];

            thePlugin.descriptor->DeleteOnUnregister();

            this->regPlugins.RemoveByIndex( 0 );
        }
    }

    // Oh my fucking god. "template" can be used other than declaring a templated type?
    // Why does C++ not make it a job for the compiler to determine things... I mean it would be possible!
    // You cannot know what a pain in the head it is to find the solution to add the "template" keyword.
    // Bjarne, seriously.
    typedef typename pluginAvailabilityDispatchType::template pluginInterfaceBase <AnonymousPluginStructRegistry> pluginInterfaceBase;

    // Virtual interface used to describe plugin classes.
    // The construction process must be immutable across the runtime.
    struct pluginInterface : public pluginInterfaceBase
    {
        virtual ~pluginInterface( void )            {}

        virtual bool OnPluginConstruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // By default, construction of plugins should succeed.
            return true;
        }

        virtual void OnPluginDestruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            return;
        }

        virtual bool OnPluginAssign( abstractionType *dstObject, const abstractionType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // Assignment of data to another plugin struct is optional.
            return false;
        }

        virtual void DeleteOnUnregister( void )
        {
            // Overwrite this method if unregistering should delete this class.
            return;
        }
    };

    // Struct that holds information about registered plugins.
    struct registered_plugin : public blockAlloc_t::block_t
    {
        inline registered_plugin( void )
        {
            this->pluginSize = 0;
        }
        inline registered_plugin( registered_plugin&& right ) noexcept :
            pluginId( std::move( right.pluginId ) ), pluginOffset( std::move( right.pluginOffset ) ),
            descriptor( std::move( right.descriptor ) )
        {
            // NOTE THAT EVERY registered_plugin INSTANCE MUST BE ALLOCATED.

            // We know that invalid registered_plugin objects have a pluginSize of zero.
            size_t pluginSize = right.pluginSize;

            if ( pluginSize != 0 )
            {
                this->moveFrom( std::move( right ) );
            }

            this->pluginSize = pluginSize;

            right.pluginSize = 0;       // this means invalidation.
            right.pluginId = pluginDescriptorType();    // TODO: probably set this to invalid id.
            right.pluginOffset = 0;
            right.descriptor = nullptr;
        }
        inline registered_plugin( const registered_plugin& right ) = delete;

        // When move assignment is happening, the object is expected to be properly constructed.
        inline registered_plugin& operator =( registered_plugin&& right ) noexcept
        {
            // Is there even anything to deallocate?
            this->~registered_plugin();

            return *new (this) registered_plugin( std::move( right ) );
        }
        inline registered_plugin& operator =( const registered_plugin& right ) = delete;

        size_t pluginSize;
        pluginDescriptorType pluginId;
        pluginOffset_t pluginOffset;
        pluginInterface *descriptor;
    };

    DEFINE_HEAP_REDIR_ALLOC( regPluginsRedirAlloc );

    // Container that holds plugin information.
    typedef eir::Vector <registered_plugin, regPluginsRedirAlloc> registeredPlugins_t;

    registeredPlugins_t regPlugins;

    // Holds things like the way to determine the size of all plugins.
    pluginAvailabilityDispatchType regBoundFlavor;

    inline size_t GetPluginSizeByRuntime( void ) const
    {
        return this->regBoundFlavor.GetPluginAllocSize( this );
    }

    inline size_t GetPluginSizeByObject( const abstractionType *object ) const
    {
        return this->regBoundFlavor.GetPluginAllocSizeByObject( this, object );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, const pluginDescriptorType& pluginId, pluginInterface *plugInterface )
    {
        // Make sure we have got valid parameters passed.
        if ( pluginSize == 0 || plugInterface == nullptr )
            return 0;   // TODO: fix this crap, 0 is ambivalent since its a valid index!

        // Determine the plugin offset that should be used for allocation.
        blockAlloc_t::allocInfo blockAllocInfo;

        bool hasSpace = pluginRegions.FindSpace( pluginSize, blockAllocInfo, this->preferredAlignment );

        // Handle obscure errors.
        if ( hasSpace == false )
            return 0;

        // The beginning of the free space is where our plugin should be placed at.
        pluginOffset_t useOffset = blockAllocInfo.slice.GetSliceStartPoint();

        // Register ourselves.
        registered_plugin info;
        info.pluginSize = pluginSize;
        info.pluginId = pluginId;
        info.pluginOffset = useOffset;
        info.descriptor = plugInterface;

        // Register the pointer to the registered plugin.
        pluginRegions.PutBlock( &info, blockAllocInfo );

        regPlugins.AddToBack( std::move( info ) );

        // Notify the flavor to update.
        this->regBoundFlavor.UpdatePluginRegion( this );

        return useOffset;
    }

    inline bool UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        bool hasDeleted = false;

        // Get the plugin information that represents this plugin offset.
        size_t numPlugins = regPlugins.GetCount();

        for ( size_t n = 0; n < numPlugins; n++ )
        {
            registered_plugin& thePlugin = regPlugins[ n ];

            if ( thePlugin.pluginOffset == pluginOffset )
            {
                // We found it!
                // Now remove it and (probably) delete it.
                thePlugin.descriptor->DeleteOnUnregister();

                pluginRegions.RemoveBlock( &thePlugin );

                regPlugins.RemoveByIndex( n );

                hasDeleted = true;
                break;  // there can be only one.
            }
        }

        if ( hasDeleted )
        {
            // Since we really have deleted, we need to readjust our struct memory size.
            // This is done by getting the span size of the plugin allocation region, which is a very fast operation!
            this->regBoundFlavor.UpdatePluginRegion( this );
        }

        return hasDeleted;
    }

private:
    typedef typename pluginAvailabilityDispatchType::template regionIterator <AnonymousPluginStructRegistry> regionIterator_t;

    inline void DestroyPluginBlockInternal( abstractionType *pluginObj, regionIterator_t& iter, AdditionalArgs... addArgs )
    {
        while ( true )
        {
            iter.Decrement();

            if ( iter.IsEnd() )
                break;

            const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

            const registered_plugin *constructedInfo = (const registered_plugin*)blockItem;

            try
            {
                // Destroy that plugin again.
                constructedInfo->descriptor->OnPluginDestruct(
                    pluginObj,
                    iter.ResolveOffset(),
                    constructedInfo->pluginId,
                    addArgs...
                );
            }
            catch( ... )
            {
                // We must not fail destruction, in any way.
                assert( 0 );
            }
        }
    }

public:
    inline bool ConstructPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs )
    {
        // Construct all plugins.
        bool pluginConstructionSuccessful = true;

        regionIterator_t iter( this );

        try
        {
            while ( !iter.IsEnd() )
            {
                const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockItem;

                // TODO: add dispatch based on the reg bound flavor.
                // it should say whether this plugin exists and where it is ultimatively located.
                // in the cached handler, this is an O(1) operation, in the conditional it is rather funky.
                // this is why you really should not use the conditional handler too often.

                bool success =
                    pluginInfo->descriptor->OnPluginConstruct(
                        pluginObj,
                        iter.ResolveOffset(),
                        pluginInfo->pluginId,
                        addArgs...
                    );

                if ( !success )
                {
                    pluginConstructionSuccessful = false;
                    break;
                }

                iter.Increment();
            }
        }
        catch( ... )
        {
            // There was an exception while trying to construct a plugin.
            // We do not let it pass and terminate here.
            pluginConstructionSuccessful = false;
        }

        if ( !pluginConstructionSuccessful )
        {
            // The plugin failed to construct, so destroy all plugins that
            // constructed up until that point.
            DestroyPluginBlockInternal( pluginObj, iter, addArgs... );
        }

        return pluginConstructionSuccessful;
    }

    inline bool AssignPluginBlock( abstractionType *dstPluginObj, const abstractionType *srcPluginObj, AdditionalArgs... addArgs )
    {
        // Call all assignment operators.
        bool cloneSuccess = true;

        regionIterator_t iter( this );

        try
        {
            for ( ; !iter.IsEnd(); iter.Increment() )
            {
                const blockAlloc_t::block_t *blockInfo = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockInfo;

                bool assignSuccess = pluginInfo->descriptor->OnPluginAssign(
                    dstPluginObj, srcPluginObj,
                    iter.ResolveOffset(),
                    pluginInfo->pluginId,
                    addArgs...
                );

                if ( !assignSuccess )
                {
                    cloneSuccess = false;
                    break;
                }
            }
        }
        catch( ... )
        {
            // There was an exception while cloning plugin data.
            // We do not let it pass and terminate here.
            cloneSuccess = false;
        }

        return cloneSuccess;
    }

    inline void DestroyPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs )
    {
        // Call destructors of all registered plugins.
        regionIterator_t endIter( this );

#if 0
        // By decrementing this double-linked-list iterator by one, we get to the end.
        endIter.Decrement();
#else
        // We want to iterate to the end.
        while ( endIter.IsEnd() == false )
        {
            endIter.Increment();
        }
#endif

        DestroyPluginBlockInternal( pluginObj, endIter, addArgs... );
    }

    // Use this function whenever you receive a handle offset to a plugin struct.
    // It is optimized so that you cannot go wrong.
    inline pluginOffset_t ResolvePluginStructOffsetByRuntime( pluginOffset_t handleOffset )
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffset( this, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }

    inline pluginOffset_t ResolvePluginStructOffsetByObject( const abstractionType *obj, pluginOffset_t handleOffset )
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffsetByObject( this, obj, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }
};

APSR_TEMPLARGS
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN AnonymousPluginStructRegistry APSR_TEMPLUSE::regPluginsRedirAlloc::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( regPluginsRedirAlloc, AnonymousPluginStructRegistry, regPlugins, allocatorType )
APSR_TEMPLARGS
IMPL_HEAP_REDIR_METH_RESIZE_RETURN AnonymousPluginStructRegistry APSR_TEMPLUSE::regPluginsRedirAlloc::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS
IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( regPluginsRedirAlloc, AnonymousPluginStructRegistry, regPlugins, allocatorType )
APSR_TEMPLARGS
IMPL_HEAP_REDIR_METH_FREE_RETURN AnonymousPluginStructRegistry APSR_TEMPLUSE::regPluginsRedirAlloc::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( regPluginsRedirAlloc, AnonymousPluginStructRegistry, regPlugins, allocatorType )

struct copy_assignment_exception {};

// Helper struct for common plugin system functions.
// THREAD-SAFE because this class itself is immutable and the systemType is THREAD-SAFE.
template <typename classType, typename systemType, typename pluginDescriptorType, typename... AdditionalArgs>
struct CommonPluginSystemDispatch
{
    systemType& sysType;

    typedef typename systemType::pluginOffset_t pluginOffset_t;

    inline CommonPluginSystemDispatch( systemType& sysType ) : sysType( sysType )
    {
        return;
    }

    template <typename interfaceType, typename... Args>
    inline pluginOffset_t RegisterCommonPluginInterface( size_t structSize, const pluginDescriptorType& pluginId, Args&&... constrArgs )
    {
        // Register our plugin!
        return sysType.template RegisterCustomPlugin <interfaceType> (
            structSize, pluginId,
            std::forward <Args> ( constrArgs )...
        );
    }

private:
    // Object construction helper.
    template <typename structType, typename... Args>
    static AINLINE structType* construct_helper( void *structMem, classType *obj, Args&&... theArgs )
    {
        if constexpr ( std::is_constructible <structType, classType*, Args...>::value )
        {
            return new (structMem) structType( obj, std::forward <Args> ( theArgs )... );
        }
        else if constexpr ( std::is_constructible <structType, classType*>::value )
        {
            return new (structMem) structType( obj );
        }
        else
        {
            return new (structMem) structType;
        }
    }

    // Since a lot of functionality could not support the copy assignment, we help the user out by throwing a common exception.
    template <typename subStructType>
    static AINLINE void copy_assign( subStructType& left, const subStructType& right )
    {
        if constexpr ( std::is_copy_assignable <subStructType>::value )
        {
            left = right;
        }
        else
        {
            throw copy_assignment_exception();
        }
    }

public:
    // Helper functions used to create common plugin templates.
    template <typename structType>
    inline pluginOffset_t RegisterStructPlugin( const pluginDescriptorType& pluginId, size_t structSize = sizeof(structType) )
    {
        struct structPluginInterface : systemType::pluginInterface
        {
            bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                void *structMem = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                if ( structMem == nullptr )
                    return false;

                // Construct the struct!
                // We prefer giving the class object as argument to the default constructor.
                structType *theStruct = construct_helper <structType, AdditionalArgs...> ( structMem, obj, std::forward <AdditionalArgs> ( addArgs )... );

                return ( theStruct != nullptr );
            }

            void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                structType *theStruct = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                // Destruct the struct!
                theStruct->~structType();
            }

            bool OnPluginAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                // Do an assignment operation.
                structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
                const structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

                copy_assign( *dstStruct, *srcStruct );
                return true;
            }
        };

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, pluginId );
    }

    template <typename structType>
    struct dependantStructPluginInterface : systemType::pluginInterface
    {
        bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            void *structMem = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            if ( structMem == nullptr )
                return false;

            // Construct the struct!
            structType *theStruct = construct_helper <structType, AdditionalArgs...> ( structMem, obj, std::forward <AdditionalArgs> ( addArgs )... );

            if ( theStruct )
            {
                try
                {
                    // Initialize the manager.
                    theStruct->Initialize( obj );
                }
                catch( ... )
                {
                    // We have to destroy our struct again.
                    theStruct->~structType();

                    throw;
                }
            }

            return ( theStruct != nullptr );
        }

        void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            structType *theStruct = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            // Deinitialize the manager.
            theStruct->Shutdown( obj );

            // Destruct the struct!
            theStruct->~structType();
        }

        bool OnPluginAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            // To an assignment operation.
            structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
            const structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

            copy_assign( *dstStruct, *srcStruct );
            return true;
        }
    };

    // TODO: remove the global new allocator calls as they are destroying the strategy of non-CRT memory usage.

    template <typename structType>
    inline pluginOffset_t RegisterDependantStructPlugin( const pluginDescriptorType& pluginId, size_t structSize = sizeof(structType) )
    {
        typedef dependantStructPluginInterface <structType> structPluginInterface;

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, pluginId );
    }

    struct conditionalPluginStructInterface abstract
    {
        // Conditional interface. This is based on the state of the runtime.
        // Both functions here have to match logically.
        virtual bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const = 0;
        virtual bool IsPluginAvailableAtObject( const classType *object, pluginDescriptorType pluginId ) const = 0;
    };

    // Helper to register a conditional type that acts as dependant struct.
    template <typename structType>
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( const pluginDescriptorType& pluginId, conditionalPluginStructInterface *conditional, size_t structSize = sizeof(structType) )
    {
        struct structPluginInterface : public dependantStructPluginInterface <structType>
        {
            inline structPluginInterface( conditionalPluginStructInterface *conditional )
            {
                this->conditional = conditional;
            }

            bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const override
            {
                return conditional->IsPluginAvailableDuringRuntime( pluginId );
            }

            bool IsPluginAvailableAtObject( const classType *object, pluginDescriptorType pluginId ) const override
            {
                return conditional->IsPluginAvailableAtObject( object, pluginId );
            }

            conditionalPluginStructInterface *conditional;
        };

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, pluginId, conditional );
    }
};

// Static plugin system that constructs classes that can be extended at runtime.
// This one is inspired by the RenderWare plugin system.
// This container is NOT MULTI-THREAD SAFE.
// All operations are expected to be ATOMIC.
#define SPCF_TEMPLARGS \
    template <typename classType, typename allocatorType, typename flavorType = cachedMinimalStructRegistryFlavor <classType>, typename... constrArgs>
#define SPCF_TEMPLUSE \
    <classType, allocatorType, flavorType, constrArgs...>

SPCF_TEMPLARGS
struct StaticPluginClassFactory
{
    typedef classType hostType_t;

    static constexpr unsigned int ANONYMOUS_PLUGIN_ID = std::numeric_limits <unsigned int>::max();

    inline StaticPluginClassFactory( void )
    {
        this->data.aliveClasses = 0;
    }

    template <typename... Args>
    inline StaticPluginClassFactory( eir::constr_with_alloc _, Args&&... allocArgs )
        : data( size_opt_constr_with_default::DEFAULT, std::forward <Args> ( allocArgs )... )
    {
        this->data.aliveClasses = 0;
    }

    inline StaticPluginClassFactory( const StaticPluginClassFactory& right ) = delete;
    inline StaticPluginClassFactory( StaticPluginClassFactory&& right ) noexcept : structRegistry( std::move( right.structRegistry ) ), data( std::move( right.data ) )
    {
        this->data.aliveClasses = right.data.aliveClasses;

        right.data.aliveClasses = 0;
    }

    inline ~StaticPluginClassFactory( void )
    {
        assert( this->data.aliveClasses == 0 );
    }

    inline StaticPluginClassFactory& operator = ( const StaticPluginClassFactory& right ) = delete;
    inline StaticPluginClassFactory& operator = ( StaticPluginClassFactory&& right ) noexcept
    {
        this->structRegistry = std::move( right.structRegistry );
        this->data.aliveClasses = right.data.aliveClasses;

        // Take over the allocator stuff.
        this->data = std::move( right.data );

        right.data.aliveClasses = 0;

        return *this;
    }

    inline unsigned int GetNumberOfAliveClasses( void ) const
    {
        return this->data.aliveClasses;
    }

private:
    // We do want to support object-allocators in the StaticPluginClassFactory aswell.
    INSTANCE_SUBSTRUCTCHECK( is_object );

    static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

public:
    // Number type used to store the plugin offset.
    typedef ptrdiff_t pluginOffset_t;

    // Helper functoid.
    struct pluginDescriptor
    {
        typedef typename StaticPluginClassFactory::pluginOffset_t pluginOffset_t;

        inline pluginDescriptor( void )
        {
            this->pluginId = StaticPluginClassFactory::ANONYMOUS_PLUGIN_ID;
        }

        inline pluginDescriptor( unsigned int pluginId )
        {
            this->pluginId = pluginId;
        }

        operator const unsigned int& ( void ) const
        {
            return this->pluginId;
        }

        template <typename pluginStructType>
        AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset, constrArgs&&... args )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <pluginStructType> ( object, offset );
        }

        template <typename pluginStructType>
        AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset, constrArgs&&... args )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <const pluginStructType> ( object, offset );
        }

        unsigned int pluginId;
    };

private:
    DEFINE_HEAP_REDIR_ALLOC( spcfAllocRedir );

public:
    typedef AnonymousPluginStructRegistry <classType, pluginDescriptor, flavorType, spcfAllocRedir, constrArgs...> structRegistry_t;

    // Localize certain plugin registry types.
    typedef typename structRegistry_t::pluginInterface pluginInterface;

    static constexpr pluginOffset_t INVALID_PLUGIN_OFFSET = (pluginOffset_t)-1;

    AINLINE static bool IsOffsetValid( pluginOffset_t offset )
    {
        return ( offset != INVALID_PLUGIN_OFFSET );
    }

    template <typename pluginStructType>
    AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (pluginStructType*)( (char*)object + sizeof( classType ) + offset );
    }

    template <typename pluginStructType>
    AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (const pluginStructType*)( (const char*)object + sizeof( classType ) + offset );
    }

    // Just helpers, no requirement.
    AINLINE static classType* BACK_RESOLVE_STRUCT( void *pluginObj, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (classType*)( (char*)pluginObj - ( sizeof( classType ) + offset ) );
    }

    AINLINE static const classType* BACK_RESOLVE_STRUCT( const void *pluginObj, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (const classType*)( (const char*)pluginObj - ( sizeof( classType ) + offset ) );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, unsigned int pluginId, pluginInterface *plugInterface )
    {
        assert( this->data.aliveClasses == 0 );

        return structRegistry.RegisterPlugin( pluginSize, pluginId, plugInterface );
    }

    template <typename structPluginInterfaceType, typename... Args>
    inline pluginOffset_t RegisterCustomPlugin( size_t pluginSize, unsigned int pluginId, Args&&... cArgs )
    {
        struct selfDeletingPlugin : public structPluginInterfaceType
        {
            inline selfDeletingPlugin( StaticPluginClassFactory *fact, Args&&... cArgs ) : structPluginInterfaceType( std::forward <Args> ( cArgs )... )
            {
                this->fact = fact;
            }

            void DeleteOnUnregister( void ) override
            {
                StaticPluginClassFactory *fact = this->fact;

                eir::dyn_del_struct <selfDeletingPlugin> ( fact->data.allocData, fact, this );
            }

            StaticPluginClassFactory *fact;
        };

        selfDeletingPlugin *pluginInfo = eir::dyn_new_struct <selfDeletingPlugin> ( this->data.allocData, this, this, std::forward <Args> ( cArgs )... );

        try
        {
            return RegisterPlugin( pluginSize, pluginId, pluginInfo );
        }
        catch( ... )
        {
            eir::dyn_del_struct <selfDeletingPlugin> ( this->data.allocData, this, pluginInfo );

            throw;
        }
    }

    inline void UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        assert( this->data.aliveClasses == 0 );

        structRegistry.UnregisterPlugin( pluginOffset );
    }

    typedef CommonPluginSystemDispatch <classType, StaticPluginClassFactory, pluginDescriptor, constrArgs...> functoidHelper_t;

    // Helper functions used to create common plugin templates.
    template <typename pluginStructType>
    inline pluginOffset_t RegisterStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof(pluginStructType) )
    {
        return functoidHelper_t( *this ).template RegisterStructPlugin <pluginStructType> ( pluginId, structSize );
    }

    template <typename pluginStructType>
    inline pluginOffset_t RegisterDependantStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof( pluginStructType ) )
    {
        return functoidHelper_t( *this ).template RegisterDependantStructPlugin <pluginStructType> ( pluginId, structSize );
    }

    // Note that this function only guarrantees to return an object size that is correct at this point in time.
    inline size_t GetClassSize( void ) const
    {
        return ( sizeof( classType ) + this->structRegistry.GetPluginSizeByRuntime() );
    }

private:
    inline void DestroyBaseObject( classType *toBeDestroyed )
    {
        try
        {
            toBeDestroyed->~classType();
        }
        catch( ... )
        {
            // Throwing exceptions from destructors is lethal.
            // We have to notify the developer about this.
            assert( 0 );
        }
    }

public:
    template <typename constructorType>
    inline classType* ConstructPlacementEx( void *classMem, const constructorType& constructor, constrArgs&&... args )
    {
        classType *resultObject = nullptr;
        {
            classType *intermediateClassObject = nullptr;

            try
            {
                intermediateClassObject = constructor.Construct( classMem );
            }
            catch( ... )
            {
                // The base object failed to construct, so terminate here.
                intermediateClassObject = nullptr;
            }

            if ( intermediateClassObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( intermediateClassObject, std::forward <constrArgs> ( args )... );

                if ( pluginConstructionSuccessful )
                {
                    // We succeeded, so return our pointer.
                    // We promote it to a real class object.
                    resultObject = intermediateClassObject;
                }
                else
                {
                    // Else we cannot keep the intermediate class object anymore.
                    DestroyBaseObject( intermediateClassObject );
                }
            }
        }

        if ( resultObject )
        {
            this->data.aliveClasses++;
        }

        return resultObject;
    }

    template <typename subAllocatorType, typename constructorType>
    inline classType* ConstructTemplate( subAllocatorType& memAllocator, const constructorType& constructor, constrArgs&&... args )
    {
        // Attempt to allocate the necessary memory.
        const size_t wholeClassSize = this->GetClassSize();

        void *classMem = memAllocator.Allocate( this, wholeClassSize, alignof(classType) );

        if ( !classMem )
            return nullptr;

        classType *resultObj = ConstructPlacementEx( classMem, constructor, std::forward <constrArgs> ( args )... );

        if ( !resultObj )
        {
            // Clean up.
            memAllocator.Free( this, classMem );
        }

        return resultObj;
    }

    template <typename subAllocatorType, typename... Args>
    inline classType* ConstructArgs( subAllocatorType& memAllocator, Args&&... theArgs )
    {
        // We try saving the arguments in a struct and then unpacking them on call
        // to the constructor, using C++17 std::apply!

        struct packed_args_constr
        {
            inline packed_args_constr( Args&&... theArgs ) : data( std::forward_as_tuple( std::forward <Args> ( theArgs )... ) )
            {
                return;
            }

            inline classType* Construct( void *mem ) const
            {
                auto helper = [&]( Args&&... theArgs )
                {
                    return new (mem) classType( std::forward <Args> ( theArgs )... );
                };

                return std::apply( helper, std::move( data ) );
            }

            std::tuple <Args&&...> data;
        };

        packed_args_constr constr( std::forward <Args> ( theArgs )... );

        return ConstructTemplate( memAllocator, constr );
    }

    template <typename constructorType>
    inline classType* ClonePlacementEx( void *classMem, const classType *srcObject, const constructorType& constructor, constrArgs&&... args )
    {
        classType *clonedObject = nullptr;
        {
            // Construct a basic class where we assign stuff to.
            classType *dstObject = nullptr;

            try
            {
                dstObject = constructor.CopyConstruct( classMem, srcObject );
            }
            catch( ... )
            {
                dstObject = nullptr;
            }

            if ( dstObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( dstObject, args... );

                if ( pluginConstructionSuccessful )
                {
                    bool cloneSuccess = structRegistry.AssignPluginBlock( dstObject, srcObject, args... );

                    if ( cloneSuccess )
                    {
                        clonedObject = dstObject;
                    }

                    if ( clonedObject == nullptr )
                    {
                        structRegistry.DestroyPluginBlock( dstObject );
                    }
                }

                if ( clonedObject == nullptr )
                {
                    // Since cloning plugin data has not succeeded, we have to destroy the constructed base object again.
                    // Make sure that we do not throw exceptions.
                    DestroyBaseObject( dstObject );

                    dstObject = nullptr;
                }
            }
        }

        if ( clonedObject )
        {
            this->data.aliveClasses++;
        }

        return clonedObject;
    }

    template <typename subAllocatorType, typename constructorType>
    inline classType* CloneTemplate( subAllocatorType& memAllocator, const classType *srcObject, const constructorType& constructor, constrArgs&&... args )
    {
        // Attempt to allocate the necessary memory.
        const size_t baseClassSize = sizeof( classType );
        const size_t wholeClassSize = this->GetClassSize();

        void *classMem = memAllocator.Allocate( wholeClassSize );

        if ( !classMem )
            return nullptr;

        classType *clonedObject = ClonePlacementEx( classMem, srcObject, constructor, std::forward <constrArgs> ( args )... );

        if ( clonedObject == nullptr )
        {
            memAllocator.Free( classMem );
        }

        return clonedObject;
    }

    struct basicClassConstructor
    {
        inline classType* Construct( void *mem ) const
        {
            return new (mem) classType;
        }

        inline classType* CopyConstruct( void *mem, const classType *srcMem ) const
        {
            return new (mem) classType( *srcMem );
        }
    };

    template <typename subAllocatorType>
    inline classType* Construct( subAllocatorType& memAllocator, constrArgs&&... args )
    {
        basicClassConstructor constructor;

        return ConstructTemplate( memAllocator, constructor, std::forward <constrArgs> ( args )... );
    }

    inline classType* ConstructPlacement( void *memPtr, constrArgs&&... args )
    {
        basicClassConstructor constructor;

        return ConstructPlacementEx( memPtr, constructor, std::forward <constrArgs> ( args )... );
    }

    template <typename subAllocatorType>
    inline classType* Clone( subAllocatorType& memAllocator, const classType *srcObject, constrArgs&&... args )
    {
        basicClassConstructor constructor;

        return CloneTemplate( memAllocator, srcObject, constructor, std::forward <constrArgs> ( args )... );
    }

    inline classType* ClonePlacement( void *memPtr, const classType *srcObject, constrArgs&&... args )
    {
        basicClassConstructor constructor;

        return ClonePlacementEx( memPtr, srcObject, constructor, std::forward <constrArgs> ( args )... );
    }

    // Assignment is good.
    inline bool Assign( classType *dstObj, const classType *srcObj )
    {
        // First we assign the language object.
        // Not that hard.
        try
        {
            *dstObj = *srcObj;
        }
        catch( ... )
        {
            return false;
        }

        // Next we should assign the plugin blocks.
        return structRegistry.AssignPluginBlock( dstObj, srcObj );
    }

    inline void DestroyPlacement( classType *classObject )
    {
        // Destroy plugin data first.
        structRegistry.DestroyPluginBlock( classObject );

        try
        {
            // Destroy the base class object.
            classObject->~classType();
        }
        catch( ... )
        {
            // There was an exception while destroying the base class.
            // This must not happen either; we have to notify the guys!
            assert( 0 );
        }

        // Decrease the number of alive classes.
        this->data.aliveClasses--;
    }

    template <typename subAllocatorType>
    inline void Destroy( subAllocatorType& memAllocator, classType *classObject )
    {
        if ( classObject == nullptr )
            return;

        // Invalidate the memory that is in "classObject".
        DestroyPlacement( classObject );

        // Free our memory.
        void *classMem = classObject;

        memAllocator.Free( this, classMem );
    }

    template <typename subAllocatorType>
    struct DeferredConstructor
    {
        StaticPluginClassFactory *pluginRegistry;
        subAllocatorType& memAllocator;

        inline DeferredConstructor( StaticPluginClassFactory *pluginRegistry, subAllocatorType& memAllocator ) : memAllocator( memAllocator )
        {
            this->pluginRegistry = pluginRegistry;
        }

        inline subAllocatorType& GetAllocator( void )
        {
            return memAllocator;
        }

        inline classType* Construct( void )
        {
            return pluginRegistry->Construct( memAllocator );
        }

        template <typename constructorType>
        inline classType* ConstructTemplate( constructorType& constructor )
        {
            return pluginRegistry->ConstructTemplate( memAllocator, constructor );
        }

        inline classType* Clone( const classType *srcObject )
        {
            return pluginRegistry->Clone( memAllocator, srcObject );
        }

        inline void Destroy( classType *object )
        {
            return pluginRegistry->Destroy( memAllocator, object );
        }
    };

    template <typename subAllocatorType>
    inline DeferredConstructor <subAllocatorType>* CreateConstructor( subAllocatorType& memAllocator )
    {
        typedef DeferredConstructor <subAllocatorType> Constructor;

        return eir::dyn_new_struct <Constructor> ( memAllocator, this, this, memAllocator );
    }

    template <typename subAllocatorType>
    inline void DeleteConstructor( DeferredConstructor <subAllocatorType> *handle )
    {
        typedef DeferredConstructor <subAllocatorType> Constructor;

        eir::dyn_del_struct <Constructor> ( handle->GetAllocator(), this, handle );
    }

private:
    // Member fields that actually matter belong to the end of structs/classes.
    structRegistry_t structRegistry;

    struct semi_fields
    {
        std::atomic <unsigned int> aliveClasses;
    };

    size_opt <hasObjectAllocator, allocatorType, semi_fields> data;
};

SPCF_TEMPLARGS
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN StaticPluginClassFactory SPCF_TEMPLUSE::spcfAllocRedir::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
IMPL_HEAP_REDIR_DYN_ALLOC_METH_ALLOCATE_BODY( StaticPluginClassFactory, structRegistry, data.allocData )
SPCF_TEMPLARGS
IMPL_HEAP_REDIR_METH_RESIZE_RETURN StaticPluginClassFactory SPCF_TEMPLUSE::spcfAllocRedir::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS
IMPL_HEAP_REDIR_DYN_ALLOC_METH_RESIZE_BODY( StaticPluginClassFactory, structRegistry, data.allocData )
SPCF_TEMPLARGS
IMPL_HEAP_REDIR_METH_FREE_RETURN StaticPluginClassFactory SPCF_TEMPLUSE::spcfAllocRedir::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
IMPL_HEAP_REDIR_DYN_ALLOC_METH_FREE_BODY( StaticPluginClassFactory, structRegistry, data.allocData )

#endif //_EIR_PLUGIN_FACTORY_HEADER_
