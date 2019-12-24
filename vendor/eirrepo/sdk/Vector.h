/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Vector.h
*  PURPOSE:     Optimized Vector implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Once we had the native heap allocator done, we realized that we had quite some cool
// memory semantics that the standard memory allocators (even from OS side) do not support.
// So we decided to roll our own SDK classes to optimize for those facts.
// It is also a good idea to consolidate the API; do not trust the STL too much!

#ifndef _EIR_VECTOR_HEADER_
#define _EIR_VECTOR_HEADER_

#include "eirutils.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include <algorithm>
#include <type_traits>
#include <assert.h>

namespace eir
{

template <typename structType, typename allocatorType>
struct Vector
{
    // Make sure that templates are friends of each-other.
    template <typename, typename> friend struct Vector;

    inline Vector( void ) noexcept
    {
        this->data.data_entries = nullptr;
        this->data.data_count = 0;
    }

private:
    static AINLINE structType* make_data_copy( Vector *refPtr, const structType *right_data, size_t right_count )
    {
        structType *data_entries = (structType*)refPtr->data.allocData.Allocate( refPtr, right_count * sizeof(structType), alignof(structType) );

        if ( data_entries == nullptr )
        {
            throw eir_exception();
        }

        size_t cur_idx = 0;

        try
        {
            while ( cur_idx < right_count )
            {
                new ( data_entries + cur_idx ) structType( right_data[cur_idx] );

                cur_idx++;
            }
        }
        catch( ... )
        {
            // Destroy all constructed items again.
            while ( cur_idx > 0 )
            {
                cur_idx--;

                ( data_entries + cur_idx )->~structType();
            }

            refPtr->data.allocData.Free( refPtr, data_entries );

            throw;
        }

        return data_entries;
    }

    AINLINE void initialize_with( const structType *data, size_t data_count )
    {
        structType *our_data_entries = nullptr;

        if ( data_count > 0 )
        {
            our_data_entries = make_data_copy( this, data, data_count );
        }

        this->data.data_entries = our_data_entries;
        this->data.data_count = data_count;
    }

    INSTANCE_SUBSTRUCTCHECK( is_object );

    static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

public:
    template <typename... Args>
    inline Vector( const structType *data, size_t data_count, Args... allocArgs ) : data( 0, fields(), std::forward <Args> ( allocArgs )... )
    {
        initialize_with( data, data_count );
    }

    template <typename... Args>
    inline Vector( constr_with_alloc, Args... allocArgs ) : data( 0, fields(), std::forward <Args> ( allocArgs )... )
    {
        initialize_with( nullptr, 0 );
    }

    inline Vector( const Vector& right ) : data( right.data )
    {
        size_t right_count = right.data.data_count;
        structType *right_data = right.data.data_entries;

        initialize_with( right_data, right_count );
    }

    template <typename otherAllocatorType>
    inline Vector( const Vector <structType, otherAllocatorType>& right )
    {
        size_t right_count = right.data.data_count;
        structType *right_data = right.data.data_entries;

        initialize_with( right_data, right_count );
    }

    // WARNING: only move if allocator stays the same.
    inline Vector( Vector&& right ) noexcept : data( std::move( right.data ) )
    {
        this->data.data_entries = right.data.data_entries;
        this->data.data_count = right.data.data_count;

        right.data.data_entries = nullptr;
        right.data.data_count = 0;
    }

private:
    static AINLINE void release_data( Vector *refPtr, structType *data_entries, size_t data_count )
    {
        if ( data_entries )
        {
            for ( size_t n = 0; n < data_count; n++ )
            {
                data_entries[n].~structType();
            }

            refPtr->data.allocData.Free( refPtr, data_entries );
        }
    }

public:
    inline ~Vector( void )
    {
        release_data( this, this->data.data_entries, this->data.data_count );
    }

    inline Vector& operator = ( const Vector& right )
    {
        size_t right_count = right.data.data_count;
        structType *new_data_entries = nullptr;

        if ( right_count > 0 )
        {
            new_data_entries = make_data_copy( this, right.data.data_entries, right_count );
        }

        // Swap old with new.
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = new_data_entries;
        this->data.data_count = right_count;

        return *this;
    }

    template <typename otherAllocatorType>
    inline Vector& operator = ( const Vector <structType, otherAllocatorType>& right )
    {
        size_t right_count = right.data.data_count;
        structType *new_data_entries = nullptr;

        if ( right_count > 0 )
        {
            new_data_entries = make_data_copy( this, right.data.data_entries, right_count );
        }

        // Swap old with new.
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = new_data_entries;
        this->data.data_count = right_count;

        return *this;
    }

    // WARNING: only move if allocator stays the same.
    inline Vector& operator = ( Vector&& right ) noexcept
    {
        // Release our previous data.
        release_data( this, this->data.data_entries, this->data.data_count );

        // Move over allocator, if needed.
        this->data = std::move( right.data );

        this->data.data_entries = right.data.data_entries;
        this->data.data_count = right.data.data_count;

        right.data.data_entries = nullptr;
        right.data.data_count = 0;

        return *this;
    }

private:
    template <typename callbackType>
    AINLINE void recast_memory( size_t new_item_count, size_t newRequiredSize, const callbackType& cb )
    {
        // Try to fetch new memory.
        void *new_data_ptr = this->data.allocData.Allocate( this, newRequiredSize, alignof(structType) );

        if ( new_data_ptr == nullptr )
        {
            throw eir_exception();
        }

        // Construct the memory.
        size_t old_count = this->data.data_count;
        structType *old_data = this->data.data_entries;

        size_t moved_idx = 0;

        try
        {
            while ( moved_idx < new_item_count )
            {
                // This is not supposed to be constant because we can move-away from the original.
                structType *old_entry = nullptr;

                if ( moved_idx < old_count )
                {
                    old_entry = ( old_data + moved_idx );
                }

                cb( (structType*)new_data_ptr + moved_idx, old_entry, moved_idx );

                moved_idx++;
            }
        }
        catch( ... )
        {
            // Move-back the already moved items.
            while ( moved_idx > 0 )
            {
                moved_idx--;

                structType& new_item = *( (structType*)new_data_ptr + moved_idx );

                // If the new item did belong to an old item, then restore the item.
                if ( moved_idx < old_count )
                {
                    *( old_data + moved_idx ) = std::move( new_item );
                }

                // Must destroy all new items.
                new_item.~structType();
            }

            // We do not add the item to our array, so clean-up.
            this->data.allocData.Free( this, new_data_ptr );

            throw;
        }

        // Delete the old data.
        if ( old_data )
        {
            release_data( this, old_data, old_count );
        }

        this->data.data_entries = (structType*)new_data_ptr;
        this->data.data_count = new_item_count;
    }

public:
    // Simple modification methods.
    inline void AddToBack( structType&& item )
    {
        size_t oldCount = ( this->data.data_count );
        size_t newCount = ( oldCount + 1 );
        size_t newRequiredSize = newCount * sizeof(structType);

        if ( structType *use_data = this->data.data_entries )
        {
            bool gotToResize = this->data.allocData.Resize( this, use_data, newRequiredSize );

            if ( gotToResize )
            {
                // We just have to add at back.
                try
                {
                    new ( use_data + oldCount ) structType( std::move( item ) );
                }
                catch( ... )
                {
                    // Have to recover in case of failure.
                    this->data.allocData.Resize( this, use_data, oldCount * sizeof(structType) );

                    throw;
                }

                // Success.
                this->data.data_count = newCount;

                return;
            }
        }

        recast_memory( newCount, newRequiredSize,
            [&]( void *memPtr, structType *old_item, size_t idx )
        {
            if ( idx == oldCount )
            {
                new (memPtr) structType( std::move( item ) );
            }
            else
            {
                new (memPtr) structType( std::move( *old_item ) );
            }
        });
    }

    inline void AddToBack( const structType& item )
    {
        size_t oldCount = ( this->data.data_count );
        size_t newCount = ( oldCount + 1 );
        size_t newRequiredSize = newCount * sizeof(structType);

        if ( structType *use_data = this->data.data_entries )
        {
            bool gotToResize = this->data.allocData.Resize( this, use_data, newRequiredSize );

            if ( gotToResize )
            {
                // We just have to add at back.
                try
                {
                    new ( use_data + oldCount ) structType( item );
                }
                catch( ... )
                {
                    // Have to recover in case of failure.
                    this->data.allocData.Resize( this, use_data, oldCount * sizeof(structType) );

                    throw;
                }

                // Success.
                this->data.data_count = newCount;

                return;
            }
        }

        recast_memory( newCount, newRequiredSize,
            [&]( void *memPtr, structType *old_item, size_t idx )
        {
            if ( idx == oldCount )
            {
                new (memPtr) structType( item );
            }
            else
            {
                new (memPtr) structType( std::move( *old_item ) );
            }
        });
    }

private:
    template <typename callbackType>
    AINLINE void process_insert( size_t insertPos, size_t insertCount, const callbackType& cb )
    {
        // It is totally valid to do nothing.
        if ( insertCount == 0 )
            return;

        // Grow the background storage so that the insertion can succeed.
        // Initialize any new items if necessary.
        // If we could not grow then just allocate a new background storage.
        size_t oldCount = this->data.data_count;
        structType *oldData = this->data.data_entries;

        size_t secure_prior_count = std::min( insertPos, oldCount );

        size_t reqCount = ( std::max( insertPos, oldCount ) + insertCount );
        size_t reqSize = ( reqCount * sizeof(structType) );

        structType *use_data = nullptr;

        constexpr bool can_safely_move_data = (
            std::is_nothrow_move_constructible <structType>::value &&
            std::is_nothrow_move_assignable <structType>::value
        );

        bool hasTakenOldBuffer = false;

        // First try resizing the old buffer.
        if constexpr ( can_safely_move_data )
        {
            if ( oldData )
            {
                bool couldGrow = this->data.allocData.Resize( this, oldData, reqSize );

                if ( couldGrow )
                {
                    hasTakenOldBuffer = true;

                    use_data = oldData;
                    goto hasAcquiredDataPointer;
                }
            }
        }

        // If we did not get a good data pointer, then we try allocating a new buffer.
        {
            structType *new_data = (structType*)this->data.allocData.Allocate( this, reqSize, alignof(structType) );

            if ( new_data )
            {
                // Have to move over the old data to our new storage.
                // If the data is nothrow movable then we can just move stuff over.
                // Otherwise we must copy the data and destroy the copies on failure.
                if constexpr ( can_safely_move_data )
                {
                    for ( size_t n = 0; n < secure_prior_count; n++ )
                    {
                        structType *move_from = ( oldData + n );

                        new ( new_data + n ) structType( std::move( *move_from ) );
                    }
                }
                else
                {
                    // Do a safe copy.
                    size_t copy_idx = 0;

                    try
                    {
                        while ( copy_idx < secure_prior_count )
                        {
                            const structType *copy_from = ( oldData + copy_idx );

                            new ( new_data + copy_idx ) structType( *copy_from );

                            copy_idx++;
                        }
                    }
                    catch( ... )
                    {
                        // Destroy the copies because we failed.
                        while ( copy_idx > 0 )
                        {
                            copy_idx--;

                            new_data[ copy_idx ].~structType();
                        }

                        throw;
                    }
                }

                hasTakenOldBuffer = false;

                use_data = new_data;
                goto hasAcquiredDataPointer;
            }
        }

        throw eir_exception();

    hasAcquiredDataPointer:;
        try
        {
            // Make sure we fill any empty slots we created with null items.
            if ( insertPos > oldCount )
            {
                size_t spawn_idx = oldCount;

                try
                {
                    while ( spawn_idx < insertPos )
                    {
                        new ( use_data + spawn_idx ) structType();

                        spawn_idx++;
                    }
                }
                catch( ... )
                {
                    // Remove the newly spawned items because we failed.
                    while ( spawn_idx > 0 )
                    {
                        spawn_idx--;

                        use_data[ spawn_idx ].~structType();
                    }

                    throw;
                }
            }

            try
            {
                // See if we have to move/copy any items to the back because the insertion
                // would push them there.
                if ( insertPos < oldCount )
                {
                    size_t afterInsertOff = ( insertPos + insertCount );
                    size_t conflictCount = ( oldCount - insertPos );
                    //size_t sourceStart = ( insertPos );
                    size_t sourceEnd = ( insertPos + conflictCount );
                    size_t establishStart = ( afterInsertOff );
                    size_t establishEnd = ( afterInsertOff + conflictCount );

                    if constexpr ( can_safely_move_data )
                    {
                        size_t src_move_idx = sourceEnd;
                        size_t dst_move_idx = establishEnd;

                        while ( dst_move_idx > establishStart )
                        {
                            dst_move_idx--;
                            src_move_idx--;

                            structType *move_from = ( oldData + src_move_idx );

                            if ( hasTakenOldBuffer == false || dst_move_idx >= oldCount )
                            {
                                new ( use_data + dst_move_idx ) structType( std::move( *move_from ) );
                            }
                            else
                            {
                                *( use_data + dst_move_idx ) = std::move( *move_from );
                            }
                        }
                    }
                    else
                    {
                        // TODO: we might improve this stuff to be move cache-consistent
                        // by always operating in index-downward order on memory/index here.
                        // For that we must switch empty-init with this (order wise).

                        // Copy the intersecting items over.
                        size_t src_copy_idx = sourceEnd;
                        size_t dst_copy_idx = establishEnd;

                        try
                        {
                            while ( dst_copy_idx > establishStart )
                            {
                                dst_copy_idx--;
                                src_copy_idx--;

                                const structType *copy_from = ( oldData + src_copy_idx );

                                try
                                {
                                    new (use_data + dst_copy_idx) structType( *copy_from );
                                }
                                catch( ... )
                                {
                                    // Restore to the last-known-good item.
                                    dst_copy_idx++;
                                    throw;
                                }
                            }
                        }
                        catch( ... )
                        {
                            // Just have to destroy the copies.
                            while ( dst_copy_idx < establishEnd )
                            {
                                use_data[ dst_copy_idx ].~structType();

                                dst_copy_idx++;
                            }

                            throw;
                        }
                    }
                }

                try
                {
                    // So we created any empties, we copied any conflicts.
                    // What is left is: insert the actual items.
                    // Here we want to allow a template to take over.
                    size_t already_constr_count = 0;

                    if ( hasTakenOldBuffer && insertPos < oldCount )
                    {
                        already_constr_count = ( oldCount - insertPos );
                    }

                    cb( use_data + insertPos, already_constr_count, hasTakenOldBuffer );

                    // Success! Update the array meta-data.
                    this->data.data_entries = use_data;
                    this->data.data_count = reqCount;

                    // Delete any old pointers with their data.
                    if ( hasTakenOldBuffer == false && oldData != nullptr )
                    {
                        for ( size_t n = 0; n < oldCount; n++ )
                        {
                            oldData[ n ].~structType();
                        }

                        this->data.allocData.Free( this, oldData );
                    }
                }
                catch( ... )
                {
                    if ( insertPos < oldCount )
                    {
                        // Restore back the items if we moved, or destroy the clones if
                        // we copied.
                        size_t afterInsertOff = ( insertPos + insertCount );
                        size_t conflictCount = ( std::min( oldCount, afterInsertOff ) - insertPos );
                        size_t sourceEnd = ( insertPos + conflictCount );
                        size_t establishStart = ( afterInsertOff );
                        size_t establishEnd = ( afterInsertOff + conflictCount );

                        if constexpr ( can_safely_move_data )
                        {
                            size_t sourceStart = ( insertPos );

                            // For each slot that we restore, if it was a slot not previously occupied by data,
                            // then we also destroy it.
                            size_t src_move_idx = sourceStart;
                            size_t dst_move_idx = establishStart;

                            while ( src_move_idx < sourceEnd )
                            {
                                // Move back.
                                structType *move_from = ( use_data + dst_move_idx );

                                oldData[ src_move_idx ] = std::move( *move_from );

                                // Safe to delete because end node?
                                if ( hasTakenOldBuffer == false || dst_move_idx >= oldCount )
                                {
                                    move_from->~structType();
                                }

                                src_move_idx++;
                                dst_move_idx++;
                            }
                        }
                        else
                        {
                            // Just kill the copies.
                            size_t dst_move_idx = establishStart;

                            while ( dst_move_idx < establishEnd )
                            {
                                use_data[ dst_move_idx ].~structType();

                                dst_move_idx++;
                            }
                        }
                    }

                    throw;
                }
            }
            catch( ... )
            {
                if ( insertPos > oldCount )
                {
                    // Destroy all the newly spawned empty copies.
                    for ( size_t spawn_idx = oldCount; spawn_idx < insertPos; spawn_idx++ )
                    {
                        use_data[ spawn_idx ].~structType();
                    }
                }

                throw;
            }
        }
        catch( ... )
        {
            if ( hasTakenOldBuffer )
            {
                // Shrink back the memory.
                // This operation cannot fail.
                bool couldShrink = this->data.allocData.Resize( this, oldData, oldCount * sizeof(structType) );

                assert( couldShrink == true );
            }
            else
            {
                // Clean up the transfer operation we did.
                if constexpr ( can_safely_move_data )
                {
                    for ( size_t move_idx = 0; move_idx < insertPos; move_idx++ )
                    {
                        structType *move_from = ( use_data + move_idx );

                        oldData[ move_idx ] = std::move( *move_from );
                    }
                }

                // Delete the new items.
                for ( size_t n = 0; n < secure_prior_count; n++ )
                {
                    use_data[ n ].~structType();
                }

                // Remove the memory.
                this->data.allocData.Free( this, use_data );
            }

            throw;
        }
    }

public:
    // Insert a number of items into this vector, possibly with move-semantics.
    inline void InsertMove( size_t insertPos, structType *insertItems, size_t insertCount )
    {
        process_insert( insertPos, insertCount,
            [&]( structType *insertToPtr, size_t alreadyConstrCount, bool hasOldBuffer )
        {
            size_t insertIdx = 0;

            try
            {
                while ( insertIdx < insertCount )
                {
                    structType *move_from = ( insertItems + insertIdx );
                    structType *move_to = ( insertToPtr + insertIdx );

                    if ( insertIdx < alreadyConstrCount )
                    {
                        if constexpr ( std::is_move_assignable <structType>::value )
                        {
                            structType tmp( std::move( *move_to ) );
                            *move_to = std::move( *move_from );
                            *move_from = std::move( tmp );
                        }
                        else
                        {
                            *move_to = *move_from;
                        }
                    }
                    else
                    {
                        new (move_to) structType( std::move( *move_from ) );
                    }

                    insertIdx++;
                }
            }
            catch( ... )
            {
                while ( insertIdx > 0 )
                {
                    insertIdx--;

                    structType *move_to = ( insertItems + insertIdx );
                    structType *move_from = ( insertToPtr + insertIdx );

                    // If we have an old buffer, it also implies that it is safe to move-assign (nothrow).
                    if ( std::is_move_assignable <structType>::value && hasOldBuffer == true )
                    {
                        if ( insertIdx < alreadyConstrCount )
                        {
                            structType tmp( std::move( *move_to ) );
                            *move_to = std::move( *move_from );
                            *move_from = std::move( tmp );
                        }
                        else
                        {
                            *move_to = std::move( *move_from );

                            // Clean-up.
                            move_from->~structType();
                        }
                    }
                    else
                    {
                        // Just destroy the new item.
                        move_from->~structType();
                    }
                }

                throw;
            }
        });
    }

    // Copy items into the vector.
    inline void Insert( size_t insertPos, const structType *insertItems, size_t insertCount )
    {
        process_insert( insertPos, insertCount,
            [&]( structType *insertToPtr, size_t alreadyConstrCount, bool hasOldBuffer )
        {
            size_t insertIdx = 0;

            try
            {
                while ( insertIdx < insertCount )
                {
                    const structType *move_from = ( insertItems + insertIdx );
                    structType *move_to = ( insertToPtr + insertIdx );

                    if ( insertIdx < alreadyConstrCount )
                    {
                        *move_to = *move_from;
                    }
                    else
                    {
                        new (move_to) structType( *move_from );
                    }

                    insertIdx++;
                }
            }
            catch( ... )
            {
                while ( insertIdx > 0 )
                {
                    insertIdx--;

                    structType *move_from = ( insertToPtr + insertIdx );

                    // Just destroy the new item.
                    move_from->~structType();
                }

                throw;
            }
        });
    }

    // Simple method for single insertion.
    inline void InsertMove( size_t insertPos, structType&& value )
    {
        this->InsertMove( insertPos, &value, 1 );
    }

    inline void Insert( size_t insertPos, const structType& value )
    {
        this->Insert( insertPos, &value, 1 );
    }

    // Writes the contents of one array into this array at a certain offset.
    AINLINE void WriteVectorIntoCount( size_t writeOff, Vector&& srcData, size_t srcWriteCount, size_t srcStartOff = 0 )
    {
#ifdef _DEBUG
        assert( srcWriteCount + srcStartOff <= srcData.data.data_count );
#endif //_DEBUG

        if ( srcWriteCount == 0 )
            return;

        size_t oldCurSize = this->data.data_count;

        if ( writeOff == 0 && srcStartOff == 0 && oldCurSize <= srcWriteCount )
        {
            release_data( this, this->data.data_entries, oldCurSize );

            size_t actualSourceCount = srcData.data.data_count;

            this->data.data_entries = srcData.data.data_entries;
            this->data.data_count = actualSourceCount;

            srcData.data.data_entries = nullptr;
            srcData.data.data_count = 0;

            // Do we have to trim?
            if ( actualSourceCount > srcWriteCount )
            {
                this->Resize( srcWriteCount );
            }
        }
        else
        {
            // Make sure there is enough space to write our entries.
            size_t writeEndOff = ( writeOff + srcWriteCount );

            if ( oldCurSize < writeEndOff )
            {
                // TODO: optimize? would be possible by not constructing some entries...
                //  but maybe way too complicated.
                this->Resize( writeEndOff );
            }

            const structType *srcDataPtr = srcData.data.data_entries;
            structType *dstDataPtr = this->data.data_entries;

            for ( size_t n = 0; n < srcWriteCount; n++ )
            {
                dstDataPtr[ writeOff + n ] = std::move( srcDataPtr[ srcStartOff + n ] );
            }
        }
    }

private:
    AINLINE void shrink_backed_memory( structType *use_data, size_t newCount )
    {
        if ( newCount == 0 )
        {
            this->data.allocData.Free( this, use_data );

            // Gotta do this.
            this->data.data_entries = nullptr;
        }
        else
        {
            bool resizeSuccess = this->data.allocData.Resize( this, use_data, sizeof(structType) * newCount );

            // Since the CRT allocator is valid too and it does not support resizing, we actually ignore a failed resize.
            // It does not matter actually, we would just waste some space.
            //assert( resizeSuccess == true );
            (void)resizeSuccess;
        }

        this->data.data_count = newCount;
    }

public:
    // Removes multiple items from the vector.
    inline void RemoveMultipleByIndex( size_t removeIdx, size_t removeCnt )
    {
        if ( removeCnt == 0 )
            return;

        size_t oldCount = this->data.data_count;

        if ( removeIdx >= oldCount )
            return;

        size_t realRemoveCnt = std::min( removeCnt, oldCount - removeIdx );

        if ( realRemoveCnt == 0 )
            return;

        size_t newCount = ( oldCount - realRemoveCnt );

        // We can always shrink memory, so go for it.
        structType *use_data = this->data.data_entries;

        // We move down all items to squash the items that were removed.
        for ( size_t n = removeIdx; n < newCount; n++ )
        {
            *( use_data + n ) = std::move( *( use_data + n + realRemoveCnt ) );
        }

        // No point in keeping the last entries anymore, so destroy them.
        for ( size_t n = 0; n < realRemoveCnt; n++ )
        {
            use_data[ newCount + n ].~structType();
        }

        // Squish stuff.
        this->shrink_backed_memory( use_data, newCount );
    }

    inline void RemoveByIndex( size_t removeIdx )
    {
        RemoveMultipleByIndex( removeIdx, 1 );
    }

    inline void RemoveFromBack( void )
    {
        size_t oldCount = this->data.data_count;

        if ( oldCount == 0 )
            return;

        size_t newCount = ( oldCount - 1 );

        // We can always shrink memory, so go for it.
        structType *use_data = this->data.data_entries;

        use_data[ newCount ].~structType();

        this->shrink_backed_memory( use_data, newCount );
    }

    // Easy helper to remove all items of a certain value.
    inline void RemoveByValue( const structType& theValue )
    {
        size_t curCount = this->data.data_count;
        size_t cur_idx = 0;

        while ( cur_idx < curCount )
        {
            bool removeCurrent = false;
            {
                const structType& curItem = this->data.data_entries[ cur_idx ];

                if ( curItem == theValue )
                {
                    removeCurrent = true;
                }
            }

            if ( removeCurrent )
            {
                this->RemoveByIndex( cur_idx );

                curCount--;
            }
            else
            {
                cur_idx++;
            }
        }
    }

    // Returns a reference to a slot on this vector. If indices up to and including this
    // slot do not exist, then they are standard-constructed.
    // Useful for map-operating-mode.
    inline structType& ObtainItem( size_t idx )
    {
        if ( idx >= this->data.data_count )
        {
            this->Resize( idx + 1 );
        }

        return this->data.data_entries[ idx ];
    }

    // Puts an item into the vector on a certain index. If there is not enough space on
    // this vector, then it is resized to fit automatically.
    // The new slots that are allocated will all be standard-constructed.
    // Useful for map-operating-mode.
    inline void SetItem( size_t idx, structType&& item )
    {
        this->ObtainItem( idx ) = std::move( item );
    }

    inline void SetItem( size_t idx, const structType& item )
    {
        this->ObtainItem( idx ) = item;
    }

    // Removes all items from this array by release the back-end memory.
    inline void Clear( void )
    {
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = nullptr;
        this->data.data_count = 0;
    }

    inline size_t GetCount( void ) const
    {
        return this->data.data_count;
    }

    // Sets the array size, creating default items on new slots.
    inline void Resize( size_t newCount )
    {
        // Handle the case when memory is just being removed.
        if ( newCount == 0 )
        {
            this->Clear();

            return;
        }

        size_t oldCount = this->data.data_count;

        if ( oldCount == newCount )
            return;

        size_t newRequiredSize = newCount * sizeof(structType);

        if ( structType *useData = this->data.data_entries )
        {
            if ( oldCount > newCount )
            {
                // We assume that we can always shrink.
                // First delete the stuff.
                for ( size_t iter = newCount; iter < oldCount; iter++ )
                {
                    ( useData + iter )->~structType();
                }

                shrink_backed_memory( useData, newCount );
                return;
            }
            else // ( oldCount < newCount )
            {
                bool gotToResize = this->data.allocData.Resize( this, useData, newRequiredSize );

                if ( gotToResize )
                {
                    size_t create_idx = oldCount;

                    try
                    {
                        // Just fill up any new slots with defaults.
                        while ( create_idx < newCount )
                        {
                            new (useData + create_idx) structType();

                            create_idx++;
                        }
                    }
                    catch( ... )
                    {
                        // Remove any possibly newly added structs.
                        while ( create_idx > oldCount )
                        {
                            create_idx--;

                            useData[ create_idx ].~structType();
                        }

                        // Have to recover in case of failure.
                        this->data.allocData.Resize( this, useData, oldCount * sizeof(structType) );

                        throw;
                    }

                    // Success.
                    this->data.data_count = newCount;

                    return;
                }
            }
        }

        // We have to cast new items.
        recast_memory( newCount, newRequiredSize,
            [&]( void *memPtr, structType *old_item, size_t idx )
        {
            if ( idx < oldCount )
            {
                new (memPtr) structType( std::move( *old_item ) );
            }
            else
            {
                new (memPtr) structType();
            }
        });
    }

    // Returns true if an item that is equal to findItem already exists inside this vector.
    inline bool Find( const structType& findItem ) const
    {
        structType *data = this->data.data_entries;
        size_t numData = this->data.data_count;

        for ( size_t n = 0; n < numData; n++ )
        {
            if ( data[n] == findItem )
            {
                return true;
            }
        }

        return false;
    }

    // Walks all items of this object.
    template <typename callbackType>
    AINLINE void Walk( const callbackType& cb )
    {
        structType *data = this->data.data_entries;
        size_t num_data = this->data.data_count;

        for ( size_t n = 0; n < num_data; n++ )
        {
            cb( n, data[ n ] );
        }
    }

    // Walks all items of this object, constant.
    template <typename callbackType>
    AINLINE void Walk( const callbackType& cb ) const
    {
        const structType *data = this->data.data_entries;
        size_t num_data = this->data.data_count;

        for ( size_t n = 0; n < num_data; n++ )
        {
            cb( n, data[ n ] );
        }
    }

    // To support the C++11 range-based for loop.
    AINLINE structType* begin( void )                   { return ( this->data.data_entries ); }
    AINLINE structType* end( void )                     { return ( this->data.data_entries + this->data.data_count ); }

    AINLINE const structType* begin( void ) const       { return ( this->data.data_entries ); }
    AINLINE const structType* end( void ) const         { return ( this->data.data_entries + this->data.data_count ); }

    // Indexing operators.
    inline structType& operator [] ( size_t idx )
    {
        if ( idx >= this->data.data_count )
        {
            throw eir_exception();
        }

        return this->data.data_entries[ idx ];
    }

    inline const structType& operator [] ( size_t idx ) const
    {
        if ( idx >= this->data.data_count )
        {
            throw eir_exception();
        }

        return this->data.data_entries[ idx ];
    }

    inline structType* GetData( void )
    {
        return this->data.data_entries;
    }

    inline const structType* GetData( void ) const
    {
        return this->data.data_entries;
    }

    inline structType& GetBack( void )
    {
        size_t curCount = this->data.data_count;

        if ( curCount == 0 )
        {
            throw eir_exception();
        }

        return this->data.data_entries[ curCount - 1 ];
    }

    inline structType& GetBack( void ) const
    {
        size_t curCount = this->data.data_count;

        if ( curCount == 0 )
        {
            throw eir_exception();
        }

        return this->data.data_entries[ curCount - 1 ];
    }

    // Implement some comparison operators.
    template <typename otherAllocatorType>
    inline bool operator == ( const Vector <structType, otherAllocatorType>& right ) const
    {
        size_t leftCount = this->data.data_count;
        size_t rightCount = right.data.data_count;

        if ( leftCount != rightCount )
            return false;

        const structType *leftData = this->data.data_entries;
        const structType *rightData = right.data.data_entries;

        for ( size_t n = 0; n < leftCount; n++ )
        {
            const structType& leftDataItem = leftData[ n ];
            const structType& rightDataItem = rightData[ n ];

            // Actually use the item comparator.
            if ( leftDataItem != rightDataItem )
            {
                return false;
            }
        }

        return true;
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const Vector <structType, otherAllocatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    // Need this thing as long as there is no static_if.
    struct fields
    {
        structType *data_entries;
        size_t data_count;
    };

    size_opt <hasObjectAllocator, allocatorType, fields> data;
};

}

#endif //_EIR_VECTOR_HEADER_
