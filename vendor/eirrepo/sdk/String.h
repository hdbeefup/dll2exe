/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/String.h
*  PURPOSE:     Optimized String implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// We can also use optimized memory semantics for Strings, aswell.
// After all appending to a String should not cast another malloc, just a resize of
// the underlying memory buffer. Why would STL rely on this weird double-the-backbuffer
// trick, anyway?

#ifndef _EIR_STRING_HEADER_
#define _EIR_STRING_HEADER_

#include "eirutils.h"
#include "MacroUtils.h"
#include "DataUtil.h"
#include "MetaHelpers.h"

#include <type_traits>

namespace eir
{

// Important helper for many string functions.
template <typename subCharType>
static inline const subCharType (&GetEmptyString( void ))[1]
{
    static const subCharType empty_string[] = { (subCharType)0 };
    return empty_string;
}

template <typename charType, typename allocatorType>
struct String
{
    // TODO: think about copying the allocator aswell, when a copy-assignment is being done.

    // Make sure that templates are friends of each-other.
    template <typename, typename> friend struct String;

    // When we talk about characters in this object we actually mean code-points that historically
    // were single characters but became redundant as single characters after the invention of
    // UTF-8 and other multi-codepoint encodings for internationalization.

    // Characters should be a trivial type, meaning that it does not throw exceptions and stuff when
    // copying or assigning. It also does use the default constructor.
    static_assert( std::is_trivial <charType>::value == true, "eir::String charType has to be of trivial type" );

private:
    INSTANCE_SUBSTRUCTCHECK( is_object );

    static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

    AINLINE void reset_to_empty( void )
    {
        this->data.char_data = (charType*)GetEmptyString <charType> ();
        this->data.num_chars = 0;
    }

public:
    inline String( void ) noexcept
    {
        this->reset_to_empty();
    }

private:
    AINLINE void initialize_with( const charType *initChars, size_t initCharCount )
    {
        if ( initCharCount == 0 )
        {
            this->reset_to_empty();
        }
        else
        {
            size_t copyCharSize = sizeof(charType) * ( initCharCount + 1 );

            charType *charBuf = (charType*)this->data.allocData.Allocate( this, copyCharSize, alignof(charType) );

            if ( charBuf == nullptr )
            {
                throw eir_exception();
            }

            //noexcept
            {
                // Copy the stuff.
                FSDataUtil::copy_impl( initChars, initChars + initCharCount, charBuf );

                // Put the zero terminator.
                *( charBuf + initCharCount ) = charType();

                // We take the data.
                this->data.char_data = charBuf;
                this->data.num_chars = initCharCount;
            }
        }
    }

    // Helper logic.
    static AINLINE void free_old_buffer( String *refMem, charType *oldChars, size_t oldCharCount, bool isNewBuf )
    {
        if ( isNewBuf && oldCharCount > 0 )
        {
            refMem->data.allocData.Free( refMem, oldChars );
        }
    }

public:
    template <typename... Args>
    inline String( const charType *initChars, size_t initCharCount, Args... allocArgs )
        : data( size_opt_constr_with_default::DEFAULT, std::forward <Args> ( allocArgs )... )
    {
        initialize_with( initChars, initCharCount );
    }

    template <typename... Args>
    inline String( constr_with_alloc, Args... allocArgs )
        : data( size_opt_constr_with_default::DEFAULT, std::forward <Args> ( allocArgs )... )
    {
        initialize_with( nullptr, 0 );
    }

    inline String( const charType *initChars )
    {
        size_t initCharCount = cplen_tozero( initChars );

        initialize_with( initChars, initCharCount );
    }

    inline String( const String& right ) : data( right.data )
    {
        // Simply create a copy.
        size_t copyCharCount = right.data.num_chars;
        const charType *src_data = right.data.char_data;

        initialize_with( src_data, copyCharCount );
    }

    template <typename otherAllocatorType>
    inline String( const String <charType, otherAllocatorType>& right )
    {
        // Simply create a copy.
        size_t copyCharCount = right.data.num_chars;
        const charType *src_data = right.data.char_data;

        initialize_with( src_data, copyCharCount );
    }

    inline String( String&& right ) noexcept : data( std::move( right.data ) )
    {
        this->data.char_data = right.data.char_data;
        this->data.num_chars = right.data.num_chars;

        right.reset_to_empty();
    }

private:
    AINLINE void release_data( void )
    {
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );
    }

public:
    inline ~String( void )
    {
        this->release_data();
    }

private:
    AINLINE static void expand_buffer( String *refMem, charType *oldCharBuf, size_t oldCharCount, size_t oldCharCopyCount, size_t newCharCount, charType*& useBufOut, bool& isBufNewOut )
    {
        size_t newRequiredCharsSize = sizeof(charType) * ( newCharCount + 1 );

        bool hasBuf = false;
        charType *useBuf = nullptr; // initializing this just to stop compiler warnings.
        bool isBufNew = false;

        if ( oldCharBuf && oldCharCount > 0 )
        {
            if ( newCharCount == 0 )
            {
                refMem->data.allocData.Free( refMem, oldCharBuf );

                hasBuf = true;
                useBuf = nullptr;
                isBufNew = true;
            }
            else
            {
                bool couldResize = refMem->data.allocData.Resize( refMem, oldCharBuf, newRequiredCharsSize );

                if ( couldResize )
                {
                    hasBuf = true;
                    useBuf = oldCharBuf;
                    isBufNew = false;
                }
            }
        }

        if ( hasBuf == false )
        {
            useBuf = (charType*)refMem->data.allocData.Allocate( refMem, newRequiredCharsSize, alignof(charType) );

            if ( useBuf == nullptr )
            {
                throw eir_exception();
            }

            // Guarranteed to throw no exception due to the trivial charType.
            size_t charCopyCount = std::min( oldCharCopyCount, newCharCount );

            if ( charCopyCount > 0 )
            {
                // Copy over the characters.
                FSDataUtil::copy_impl( oldCharBuf, oldCharBuf + charCopyCount, useBuf );
            }

            hasBuf = true;
            isBufNew = true;
        }

        // Return the data.
        isBufNewOut = isBufNew;
        useBufOut = useBuf;
    }

public:
    // *** Modification functions.

    inline void Assign( const charType *theChars, size_t copyCharCount )
    {
        if ( copyCharCount == 0 )
        {
            this->release_data();

            this->reset_to_empty();
        }
        else
        {
            // We need a big enough buffer for the new string.
            // If we have a buffer already then try scaling it.
            // Otherwise we allocate a new one.
            charType *useBuf = nullptr;
            bool isBufNew;

            charType *oldCharBuf = this->data.char_data;
            size_t oldCharCount = this->data.num_chars;

            expand_buffer( this, oldCharBuf, oldCharCount, 0, copyCharCount, useBuf, isBufNew );

            // Granted because charType is trivial.
            //noexcept
            {
                // Create a copy of the input strings.
                // We add 1 to include the null-terminator.
                FSDataUtil::copy_impl( theChars, theChars + copyCharCount + 1, useBuf );

                // Take over the buff.
                free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

                this->data.char_data = useBuf;
                this->data.num_chars = copyCharCount;
            }
        }
    }

    inline String& operator = ( const String& right )
    {
        this->Assign( right.data.char_data, right.data.num_chars );

        return *this;
    }

    template <typename otherAllocatorType>
    inline String& operator = ( const String <charType, otherAllocatorType>& right )
    {
        this->Assign( right.data.char_data, right.data.num_chars );

        return *this;
    }

    inline String& operator = ( const charType *rightChars )
    {
        this->Assign( rightChars, cplen_tozero( rightChars ) );

        return *this;
    }

    // WARNING: only move if allocator stays the same!
    inline String& operator = ( String&& right ) noexcept
    {
        // Delete previous string.
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );

        // Move over allocator if needed.
        this->data = std::move( right.data );

        this->data.char_data = right.data.char_data;
        this->data.num_chars = right.data.num_chars;

        right.reset_to_empty();

        return *this;
    }

    // Append characters to the end of this string.
    inline void Append( const charType *charsToAppend, size_t charsToAppendCount )
    {
        // There is nothing to do.
        if ( charsToAppendCount == 0 )
            return;

        size_t num_chars = this->data.num_chars;

        // Calculate how long the new string has to be.
        size_t newCharCount = ( num_chars + charsToAppendCount );

        // Allocate the new buffer.
        charType *oldCharBuf = this->data.char_data;

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, num_chars, num_chars, newCharCount, useBuf, isBufNew );

        // Guarranteed due to trivial charType.
        //noexcept
        {
            // Now copy in the appended bytes.
            FSDataUtil::copy_impl( charsToAppend, charsToAppend + charsToAppendCount, useBuf + num_chars );

            // We must re-put the null-terminator.
            *( useBuf + newCharCount ) = (charType)0;

            // Take over the new buffer.
            free_old_buffer( this, oldCharBuf, num_chars, isBufNew );

            this->data.char_data = useBuf;
            this->data.num_chars = newCharCount;
        }
    }

    inline void Insert( size_t insertPos, const charType *charsToInsert, size_t charsToInsertCount )
    {
        // Nothing to do? Then just quit.
        if ( charsToInsertCount == 0 )
            return;

        // Expand the memory as required.
        size_t oldCharCount = this->data.num_chars;
        charType *oldCharBuf = this->data.char_data;

        // If the insertion position is after the string size, then we clamp the insertion
        // position to the size.
        if ( insertPos > oldCharCount )
        {
            insertPos = oldCharCount;
        }

        size_t newCharCount = ( oldCharCount + charsToInsertCount );

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, oldCharCount, insertPos, newCharCount, useBuf, isBufNew );

        //noexcept
        {
            // We first put any after-bytes to the end of the string.
            size_t afterBytesCount = ( oldCharCount - insertPos );

            if ( afterBytesCount > 0 )
            {
                const charType *move_start = ( oldCharBuf + insertPos );

                FSDataUtil::copy_backward_impl( move_start, move_start + afterBytesCount, useBuf + newCharCount );
            }

            // Now copy in the insertion chars.
            FSDataUtil::copy_impl( charsToInsert, charsToInsert + charsToInsertCount, useBuf + insertPos );

            // Put the zero terminator at the end.
            *( useBuf + newCharCount ) = (charType)0;

            // Take over the new stuff.
            free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

            this->data.char_data = useBuf;
            this->data.num_chars = newCharCount;
        }
    }

    // Turns the string around, so that it is written in reverse order.
    inline void reverse( void )
    {
        // We revert the beginning even-string.
        // Then if there was an uneven-item at the end, we insert it to the front.
        size_t cp_count = this->data.num_chars;

        if ( cp_count == 0 )
            return;

        bool is_uneven = ( ( cp_count & 0x01 ) == 1 );

        charType *data = this->data.char_data;

        // Revert the even part.
        {
            size_t even = ( is_uneven ? cp_count - 1 : cp_count );
            size_t half_of_even = ( cp_count / 2 );

            for ( size_t n = 0; n < half_of_even; n++ )
            {
                size_t left_idx = n;
                size_t right_idx = ( even - n - 1 );

                charType left_swap = std::move( data[ left_idx ] );
                charType right_swap = std::move( data[ right_idx ] );

                data[ left_idx ] = std::move( right_swap );
                data[ right_idx ] = std::move( left_swap );
            }
        }

        // If we are uneven, then we have to move up all the items by one now.
        if ( is_uneven )
        {
            size_t idx = ( cp_count - 1 );

            charType uneven_remember = data[ idx ];

            while ( idx > 0 )
            {
                size_t idx_to = idx;

                idx--;

                data[ idx_to ] = std::move( data[ idx ] );
            }

            data[ 0 ] = uneven_remember;
        }
    }

    // Empties out the string by freeing the associated buffer.
    inline void Clear( void )
    {
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );

        this->reset_to_empty();
    }

    // Sets the amount of code points that this string is consisting of.
    // If the string grows then it is padded with zeroes.
    inline void Resize( size_t numCodePoints )
    {
        size_t oldCharCount = this->data.num_chars;

        // Nothing to do?
        if ( oldCharCount == numCodePoints )
            return;

        charType *oldCharBuf = this->data.char_data;

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, oldCharCount, numCodePoints, numCodePoints, useBuf, isBufNew );

        // Fill up the zeroes.
        for ( size_t idx = oldCharCount; idx < numCodePoints; idx++ )
        {
            useBuf[ idx ] = (charType)0;
        }

        // Zero-terminate.
        *( useBuf + numCodePoints ) = (charType)0;

        // Destroy any previous buffer.
        free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

        // Remember the new thing.
        this->data.char_data = useBuf;
        this->data.num_chars = numCodePoints;
    }

    // Returns true if the codepoints of compareWith match this string.
    // This check if of course case-sensitive.
    // Use other algorithms if you need an case-insensitive comparison because it is complicated (UniChar.h).
    inline bool equals( const charType *compareWith, size_t compareWithCount ) const
    {
        size_t num_chars = this->data.num_chars;

        if ( num_chars != compareWithCount )
            return false;

        // Do we find any codepoint that does not match?
        const charType *leftChars = this->data.char_data;

        for ( size_t n = 0; n < num_chars; n++ )
        {
            charType leftChar = *( leftChars + n );
            charType rightChar = *( compareWith + n );

            if ( leftChar != rightChar )
            {
                return false;
            }
        }

        return true;
    }

    inline bool equals( const charType *compareWith ) const
    {
        // TODO: maybe optimize this so that we loop only once.
        return equals( compareWith, cplen_tozero( compareWith ) );
    }

    inline bool IsEmpty( void ) const
    {
        return ( this->data.num_chars == 0 );
    }

    inline size_t GetLength( void ) const
    {
        return this->data.num_chars;
    }

    inline const charType* GetConstString( void ) const
    {
        return this->data.char_data;
    }

    // Helpful operator overrides.

    inline String& operator += ( charType oneChar )
    {
        this->Append( &oneChar, 1 );

        return *this;
    }

    inline String& operator += ( const charType *someChars )
    {
        size_t appendCount = cplen_tozero( someChars );

        this->Append( someChars, appendCount );

        return *this;
    }

    template <typename otherAllocatorType>
    inline String& operator += ( const String <charType, otherAllocatorType>& right )
    {
        this->Append( right.data.char_data, right.data.num_chars );

        return *this;
    }

    template <typename otherAllocatorType>
    inline String operator + ( const String <charType, otherAllocatorType>& right )
    {
        String newString( *this );

        newString += right;

        return newString;
    }

    inline bool operator == ( const charType *someChars ) const
    {
        return this->equals( someChars, cplen_tozero( someChars ) );
    }

    template <typename otherAllocatorType>
    inline bool operator == ( const String <charType, otherAllocatorType>& right ) const
    {
        return this->equals( right.data.char_data, right.data.num_chars );
    }

    inline bool operator != ( const charType *someChars ) const
    {
        return !( operator == ( someChars ) );
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const String <charType, otherAllocatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

    // Reason why we do not provide less-than or greater-than operators is because it would only make
    // sense with case-insensitive functionality added.

    // Needed for operator overloads.
    inline const allocatorType& GetAllocData( void ) const
    {
        return this->data.allocData;
    }

private:
    // The actual members of the String object.
    // Only time will tell if they'll include static_if.
    // Maybe we will have a nice time with C++ concepts?
    struct fields
    {
        charType *char_data;
        size_t num_chars;
    };

    size_opt <hasObjectAllocator, allocatorType, fields> data;
};

// Optimizations for the things.
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( const charType *left, String <charType, allocatorType>&& right )
{
    right.Insert( 0, left, cplen_tozero( left ) );
    return std::move( right );
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( String <charType, allocatorType>&& left, const charType *right )
{
    left += right;
    return std::move( left );
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( const charType *left, const String <charType, allocatorType>& right )
{
    String <charType, allocatorType> outStr( eir::constr_with_alloc::DEFAULT, right.GetAllocData() );
    outStr += left;
    outStr += right;
    return outStr;
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( const String <charType, allocatorType>& left, const charType *right )
{
    String <charType, allocatorType> outStr( left );
    outStr += right;
    return outStr;
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( charType left, String <charType, allocatorType>&& right )
{
    right.Insert( 0, &left, 1 );
    return std::move( right );
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( String <charType, allocatorType>&& left, charType right )
{
    left += right;
    return std::move( left );
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( charType left, const String <charType, allocatorType>& right )
{
    String <charType, allocatorType> outStr( eir::constr_with_alloc::DEFAULT, right.GetAllocData() );
    outStr += left;
    outStr += right;
    return outStr;
}
template <typename charType, typename allocatorType>
AINLINE String <charType, allocatorType> operator + ( const String <charType, allocatorType>& left, charType right )
{
    String <charType, allocatorType> outStr( left );
    outStr += right;
    return outStr;
}

}

#endif //_EIR_STRING_HEADER_
