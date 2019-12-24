/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MultiString.h
*  PURPOSE:     Character string that can deal with multiple encodings.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// We had this filePath definition in the global headers of FileSystem for pretty long.
// Across many filesystem implementations there are a multitude of encodings internally used,
// so it makes much sense to involve an encoding-transparent type for it.
// But since that type has become so complicated and powerful, it is a really good idea
// to put it here. Besides, not just FileSystem could require such a type!

#ifndef _EIR_MULTISTRING_HEADER_
#define _EIR_MULTISTRING_HEADER_

#include <cwchar>
#include <type_traits>
#include <assert.h>

#include "MemoryRaw.h"
#include "UniChar.h"

#include "eirutils.h"
#include "ABIHelpers.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"
#include "rwlist.hpp"

#include "String.h"

namespace eir
{

// String container that is encoding-transparent.
// Supports both unicode and ANSI, can run case-sensitive or opposite.
#define MSTR_TEMPLARGS \
    template <typename allocatorType>
#define MSTR_TEMPLUSE \
    <allocatorType>

MSTR_TEMPLARGS
struct MultiString
{
private:
    // For comparison method.
    template <typename _allocatorType> // had to change this because of stupid GCC.
    friend struct MultiString;

    // We need some virtual string types for ABI compatibility.
    typedef abiVirtualString <char8_t> abiUTF8String;

    struct stringProvider abstract
    {
        virtual         ~stringProvider( void )         {}

        // Friendly reminder: only clone to MultiStrings that have a compatible allocatorType.
        virtual stringProvider* Clone( MultiString *refPtr ) const = 0;
        virtual stringProvider* InheritConstruct( MultiString *refPtr ) const = 0;

        virtual void            UpdateRefPtr( MultiString *newRefPtr ) noexcept = 0;

        virtual void            Clear( void ) noexcept = 0;
        virtual void            SetSize( size_t strSize ) = 0;

        virtual size_t          GetLength( void ) const = 0;
        virtual size_t          GetDecodedLength( void ) const = 0;

        virtual void            InsertChars( size_t offset, const char *src, size_t srcLen ) = 0;
        virtual void            InsertChars( size_t offset, const wchar_t *src, size_t srcLen ) = 0;
        virtual void            InsertChars( size_t offset, const char16_t *src, size_t srcLen ) = 0;
        virtual void            InsertChars( size_t offset, const char32_t *src, size_t srcLen ) = 0;
        virtual void			InsertChars( size_t offset, const char8_t *src, size_t srcLen ) = 0;
        virtual void            Insert( size_t offset, const stringProvider *right, size_t srcLen ) = 0;

        virtual void            AppendChars( const char *right, size_t rightLen ) = 0;
        virtual void            AppendChars( const wchar_t *right, size_t rightLen ) = 0;
        virtual void            AppendChars( const char16_t *right, size_t rightLen ) = 0;
        virtual void            AppendChars( const char32_t *right, size_t rightLen ) = 0;
        virtual void			AppendChars( const char8_t *right, size_t rightLen ) = 0;
        virtual void            Append( const stringProvider *right ) = 0;

        virtual eCompResult     CompareToChars( const char *right, size_t rightLen, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareToChars( const wchar_t *right, size_t rightLen, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareToChars( const char16_t *right, size_t rightLen, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareToChars( const char32_t *right, size_t rightLen, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareToChars( const char8_t *right, size_t rightLen, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareTo( const stringProvider *right, bool caseSensitive ) const = 0;

        virtual char			GetCharacterANSI( size_t pos ) const = 0;
        virtual wchar_t			GetCharacterUnicode( size_t pos ) const = 0;
        virtual char16_t        GetCharacterUTF16( size_t pos ) const = 0;
        virtual char32_t        GetCharacter( size_t pos ) const = 0;
        virtual char8_t			GetCharacterUTF8( size_t pos ) const = 0;

        virtual eCompResult     CompareCharacterAt( char refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareCharacterAt( wchar_t refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareCharacterAt( char16_t refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareCharacterAt( char32_t refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual eCompResult     CompareCharacterAt( char8_t refChar, size_t pos, bool caseSensitive ) const = 0;

        virtual abiString       ToANSI( void ) const = 0;
        virtual abiWideString   ToUnicode( void ) const = 0;
        virtual abiUTF8String	ToUTF8( void ) const = 0;

        virtual const char*     GetConstANSIString( void ) const noexcept		{ return nullptr; }
        virtual const wchar_t*  GetConstUnicodeString( void ) const noexcept	{ return nullptr; }
        virtual const char16_t* GetConstUTF16String( void ) const noexcept      { return nullptr; }
        virtual const char8_t*	GetConstUTF8String( void ) const noexcept		{ return nullptr; }
        virtual const char32_t* GetConstUTF32String( void ) const noexcept      { return nullptr; }

        virtual bool            IsUnicode( void ) const noexcept = 0;
        virtual bool            IsSameCharType( const stringProvider *other ) const noexcept = 0;
    };

    // Helper for static calls.
    template <typename callbackType, typename... Args>
    static AINLINE decltype(auto) StringProviderCharPipe( const stringProvider *right, Args... theArgs )
    {
        if ( const char *str = right->GetConstANSIString() )
        {
            return callbackType::ProcChars( str, std::forward <Args> ( theArgs )... );
        }
        else if ( const wchar_t *str = right->GetConstUnicodeString() )
        {
            return callbackType::ProcChars( str, std::forward <Args> ( theArgs )... );
        }
        else if ( const char16_t *str = right->GetConstUTF16String() )
        {
            return callbackType::ProcChars( str, std::forward <Args> ( theArgs )... );
        }
        else if ( const char8_t *str = right->GetConstUTF8String() )
        {
            return callbackType::ProcChars( str, std::forward <Args> ( theArgs )... );
        }
        else if ( const char32_t *str = right->GetConstUTF32String() )
        {
            return callbackType::ProcChars( str, std::forward <Args> ( theArgs )... );
        }
        else
        {
            throw codepoint_exception( "invalid character type in codepoint operation" );
        }
    }

private:
    template <typename callbackType>
    struct staticToRuntimeDispatch
    {
        template <typename charType>
        static AINLINE decltype(auto) ProcChars( const charType *str, const callbackType& cb )
        {
            return cb( str );
        }
    };

public:
    template <typename callbackType>
    static AINLINE decltype(auto) DynStringProviderCharPipe( const stringProvider *prov, const callbackType& cb )
    {
        return StringProviderCharPipe <staticToRuntimeDispatch <callbackType>> ( prov, cb );
    }

    template <typename charType>
    inline static const charType* GetConstCharStringProvider( const stringProvider *prov )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return prov->GetConstANSIString();
        }
        if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return prov->GetConstUnicodeString();
        }
        if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return prov->GetConstUTF16String();
        }
        if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return prov->GetConstUTF8String();
        }
        if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return prov->GetConstUTF32String();
        }

        return nullptr;
    }

    // Helper to check which character type a stringProvider does store.
    template <typename charType>
    static AINLINE bool is_string_prov_char( const stringProvider *prov )
    {
        return ( GetConstCharStringProvider <charType> ( prov ) != nullptr );
    }

    // We need to host the allocator directly inside the MultiString.
    INSTANCE_SUBSTRUCTCHECK( is_object );

    static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

    // Allocator redirect from any sub-string directly to allocator of MultiString.
    struct directMSAllocator
    {
        inline directMSAllocator( MultiString *refPtr )
        {
            this->refPtr = refPtr;
        }

        inline void* Allocate( void *_, size_t memSize, size_t alignment )
        {
            MultiString *refPtr = this->refPtr;
            return refPtr->data.allocData.Allocate( refPtr, memSize, alignment );
        }

        inline bool Resize( void *_, void *memPtr, size_t reqSize )
        {
            MultiString *refPtr = this->refPtr;
            return refPtr->data.allocData.Resize( refPtr, memPtr, reqSize );
        }

        inline void Free( void *_, void *memPtr )
        {
            MultiString *refPtr = this->refPtr;
            refPtr->data.allocData.Free( refPtr, memPtr );
        }

        MultiString *refPtr;

        struct is_object {};
    };

#define CSP_TEMPLARGS \
    template <typename charType>
#define CSP_TEMPLUSE \
    <charType>

    CSP_TEMPLARGS
    struct charStringProvider : public stringProvider
    {
        typedef character_env <charType> char_env;
        typedef charenv_charprov_tocplen <charType> char_prov_t;
        typedef character_env_iterator <charType, char_prov_t> char_iterator;

        // Redirect allocator calls from the character provider.
        DEFINE_HEAP_REDIR_ALLOC( cspRedirAlloc );

        MultiString *refPtr;
        eir::String <charType, cspRedirAlloc> strData;

        inline charStringProvider( MultiString *refPtr ) : refPtr( refPtr )
        {
            return;
        }
        inline charStringProvider( const charStringProvider& right ) = default;
        inline charStringProvider( const charStringProvider& right, MultiString *refPtr ) : refPtr( refPtr ), strData( right.strData )
        {
            return;
        }

        void UpdateRefPtr( MultiString *newRefPtr ) noexcept override final
        {
            this->refPtr = newRefPtr;
        }

        void Clear( void ) noexcept override final              { strData.Clear(); }
        void SetSize( size_t strSize ) override final           { strData.Resize( strSize ); }
        size_t GetLength( void ) const override final           { return strData.GetLength(); }
        size_t GetDecodedLength( void ) const override final
        {
            size_t len = 0;

            const charType *str = strData.GetConstString();

            char_prov_t char_prov( str, strData.GetLength() );

            for ( char_iterator iter( str, std::move( char_prov ) ); !iter.IsEnd(); iter.Increment() )
            {
                len++;
            }

            return len;
        }

    private:
        AINLINE static size_t get_encoding_strpos( const charType *data, size_t dataLen, size_t charPos )
        {
            // For ANSI this function is supposed to optimize down to a return of charPos.
            // Needs verification.

            const charType *str = data;

            char_prov_t char_prov( str, dataLen );

            char_iterator iter( str, std::move( char_prov ) );

            size_t cur_i = 0;

            for ( ; !iter.IsEnd(), cur_i < charPos; iter.Increment(), cur_i++ );

            return ( iter.GetPointer() - data );
        }

        void InsertCharsNative( size_t offset, const charType *src, size_t srcLen )
        {
            size_t real_pos = get_encoding_strpos( strData.GetConstString(), strData.GetLength(), offset );

            strData.Insert( real_pos, src, srcLen );
        }

        template <typename transCharType>
        AINLINE void InsertCharsTrans( size_t offset, const transCharType *src, size_t srcLen )
        {
            eir::String <charType, directMSAllocator> nativeOut = CharacterUtil::ConvertStringsLength <transCharType, charType, directMSAllocator> ( src, srcLen, this->refPtr );

            InsertCharsNative( offset, nativeOut.GetConstString(), nativeOut.GetLength() );
        }

    public:
        void InsertChars( size_t offset, const char *src, size_t srcLen ) override final
        {
            if constexpr ( std::is_same <charType, char>::value == false )
            {
                InsertCharsTrans( offset, src, srcLen );
            }
            else
            {
                InsertCharsNative( offset, src, srcLen );
            }
        }
        void InsertChars( size_t offset, const wchar_t *src, size_t srcLen ) override final
        {
            if constexpr ( std::is_same <charType, wchar_t>::value == false )
            {
                InsertCharsTrans( offset, src, srcLen );
            }
            else
            {
                InsertCharsNative( offset, src, srcLen );
            }
        }
        void InsertChars( size_t offset, const char16_t *src, size_t srcLen ) override final
        {
            if constexpr ( std::is_same <charType, char16_t>::value == false )
            {
                InsertCharsTrans( offset, src, srcLen );
            }
            else
            {
                InsertCharsNative( offset, src, srcLen );
            }
        }
        void InsertChars( size_t offset, const char32_t *src, size_t srcLen ) override final
        {
            if constexpr ( std::is_same <charType, char32_t>::value == false )
            {
                InsertCharsTrans( offset, src, srcLen );
            }
            else
            {
                InsertCharsNative( offset, src, srcLen );
            }
        }
        void InsertChars( size_t offset, const char8_t *src, size_t srcLen ) override final
        {
            if constexpr ( std::is_same <charType, char8_t>::value == false )
            {
                InsertCharsTrans( offset, src, srcLen );
            }
            else
            {
                InsertCharsNative( offset, src, srcLen );
            }
        }

    private:
        void AppendCharsNative( const charType *right, size_t rightLen )
        {
            strData.Append( right, rightLen );
        }

        template <typename transCharType>
        AINLINE void AppendCharsTrans( const transCharType *right, size_t rightLen )
        {
            eir::String <charType, directMSAllocator> nativeOut = CharacterUtil::ConvertStringsLength <transCharType, charType, directMSAllocator> ( right, rightLen, this->refPtr );

            AppendChars( nativeOut.GetConstString(), nativeOut.GetLength() );
        }

    public:
        void AppendChars( const char *right, size_t rightLen ) override final
        {
            if constexpr ( std::is_same <charType, char>::value == false )
            {
                AppendCharsTrans( right, rightLen );
            }
            else
            {
                AppendCharsNative( right, rightLen );
            }
        }
        void AppendChars( const wchar_t *right, size_t rightLen ) override final
        {
            if constexpr ( std::is_same <charType, wchar_t>::value == false )
            {
                AppendCharsTrans( right, rightLen );
            }
            else
            {
                AppendCharsNative( right, rightLen );
            }
        }
        void AppendChars( const char16_t *right, size_t rightLen ) override final
        {
            if constexpr ( std::is_same <charType, char16_t>::value == false )
            {
                AppendCharsTrans( right, rightLen );
            }
            else
            {
                AppendCharsNative( right, rightLen );
            }
        }
        void AppendChars( const char32_t *right, size_t rightLen ) override final
        {
            if constexpr ( std::is_same <charType, char32_t>::value == false )
            {
                AppendCharsTrans( right, rightLen );
            }
            else
            {
                AppendCharsNative( right, rightLen );
            }
        }
        void AppendChars( const char8_t *right, size_t rightLen ) override final
        {
            if constexpr ( std::is_same <charType, char8_t>::value == false )
            {
                AppendCharsTrans( right, rightLen );
            }
            else
            {
                AppendCharsNative( right, rightLen );
            }
        }

    private:
        template <typename transCharType>
        AINLINE eCompResult CompareToCharsNative( const transCharType *transStr, size_t strLen, bool caseSensitive ) const
        {
            return FixedStringCompare( this->strData.GetConstString(), this->strData.GetLength(), transStr, strLen, caseSensitive );
        }

    public:
        eCompResult CompareToChars( const char *right, size_t rightLen, bool caseSensitive ) const override final
        {
            return CompareToCharsNative( right, rightLen, caseSensitive );
        }
        eCompResult CompareToChars( const wchar_t *right, size_t rightLen, bool caseSensitive ) const override final
        {
            return CompareToCharsNative( right, rightLen, caseSensitive );
        }
        eCompResult CompareToChars( const char16_t *right, size_t rightLen, bool caseSensitive ) const override final
        {
            return CompareToCharsNative( right, rightLen, caseSensitive );
        }
        eCompResult CompareToChars( const char32_t *right, size_t rightLen, bool caseSensitive ) const override final
        {
            return CompareToCharsNative( right, rightLen, caseSensitive );
        }
        eCompResult CompareToChars( const char8_t *right, size_t rightLen, bool caseSensitive ) const override final
        {
            return CompareToCharsNative( right, rightLen, caseSensitive );
        }

        eCompResult CompareTo( const stringProvider *right, bool caseSensitive ) const override final
        {
            eCompResult result = eCompResult::LEFT_LESS;

            if ( const char *ansiStr = right->GetConstANSIString() )
            {
                result = CompareToChars( ansiStr, right->GetLength(), caseSensitive );
            }
            else if ( const wchar_t *wideStr = right->GetConstUnicodeString() )
            {
                result = CompareToChars( wideStr, right->GetLength(), caseSensitive );
            }
            else if ( const char16_t *wideStr = right->GetConstUTF16String() )
            {
                result = CompareToChars( wideStr, right->GetLength(), caseSensitive );
            }
            else if ( const char8_t *utf8Str = right->GetConstUTF8String() )
            {
                result = CompareToChars( utf8Str, right->GetLength(), caseSensitive );
            }
            else if ( const char32_t *utf32Str = right->GetConstUTF32String() )
            {
                result = CompareToChars( utf32Str, right->GetLength(), caseSensitive );
            }
            else if ( this->strData.IsEmpty() )
            {
                result = eCompResult::EQUAL;
            }

            return result;
        }

    private:
        template <typename transCharType>
        AINLINE transCharType GetCharacterFromNative( size_t strPos ) const
        {
            transCharType resVal = (transCharType)0xCC;

            const charType *ourStr = this->strData.GetConstString();
            size_t cplen = this->strData.GetLength();

            size_t realPos = get_encoding_strpos( ourStr, cplen, strPos );

            if ( realPos < cplen )
            {
                size_t leftLen = ( cplen - realPos );

                const charType *reqPos = ( ourStr + realPos );

                char_prov_t char_prov( reqPos, leftLen );

                typename char_env::ucp_t ourNativeChar = char_iterator( ourStr + realPos, std::move( char_prov ) ).Resolve();

                // Transform our character into a character of the destination environment.
                typedef character_env <transCharType> trans_env;

                typename trans_env::ucp_t trans_ucp;

                if ( AcquireDirectUCP <char_env, trans_env> ( ourNativeChar, trans_ucp ) )
                {
                    typename trans_env::enc_result encRes;
                    trans_env::EncodeUCP( trans_ucp, encRes );

                    if ( encRes.numData > 0 )
                    {
                        // We simply return the first character.
                        // Actually should consider returning all of the characters.
                        resVal = encRes.data[0];
                    }
                }
            }

            return resVal;
        }

    public:
        char GetCharacterANSI( size_t strPos ) const override final
        {
            // For ANSI, this function should optimize down to a return of value.

            return GetCharacterFromNative <char> ( strPos );
        }
        wchar_t GetCharacterUnicode( size_t strPos ) const override final
        {
            return GetCharacterFromNative <wchar_t> ( strPos );
        }
        char16_t GetCharacterUTF16( size_t strPos ) const override final
        {
            return GetCharacterFromNative <wchar_t> ( strPos );
        }
        char32_t GetCharacter( size_t strPos ) const override final
        {
            return GetCharacterFromNative <char32_t> ( strPos );
        }
        char8_t GetCharacterUTF8( size_t strPos ) const override final
        {
            return GetCharacterFromNative <char8_t> ( strPos );
        }

    private:
        template <typename transCharType>
        AINLINE eCompResult CompareCharacterAtWithNative( transCharType refChar, size_t strPos, bool caseSensitive ) const
        {
            const charType *ourStr = this->strData.GetConstString();
            size_t cplen = this->strData.GetLength();

            size_t realPos = get_encoding_strpos( ourStr, cplen, strPos );

            if ( realPos < cplen )
            {
                size_t leftLen = ( cplen - realPos );

                const charType *reqPos = ( ourStr + realPos );

                char_prov_t char_prov( reqPos, leftLen );

                typename char_env::ucp_t ourVal = char_iterator( ourStr + realPos, std::move( char_prov ) ).Resolve();

                return CompareCharacter( ourVal, refChar, caseSensitive );
            }

            return eCompResult::LEFT_LESS;
        }

    public:
        eCompResult CompareCharacterAt( char refChar, size_t strPos, bool caseSensitive ) const override final
        {
            // Should optimize down very well in case of ANSI-ANSI comparison.

            return CompareCharacterAtWithNative( refChar, strPos, caseSensitive );
        }
        eCompResult CompareCharacterAt( wchar_t refChar, size_t strPos, bool caseSensitive ) const override final
        {
            return CompareCharacterAtWithNative( refChar, strPos, caseSensitive );
        }
        eCompResult CompareCharacterAt( char16_t refChar, size_t strPos, bool caseSensitive ) const override final
        {
            return CompareCharacterAtWithNative( refChar, strPos, caseSensitive );
        }
        eCompResult CompareCharacterAt( char32_t refChar, size_t strPos, bool caseSensitive ) const override final
        {
            return CompareCharacterAtWithNative( refChar, strPos, caseSensitive );
        }
        eCompResult CompareCharacterAt( char8_t refChar, size_t strPos, bool caseSensitive ) const override final
        {
            // Subtle differences to ANSI comparison.

            return CompareCharacterAtWithNative( refChar, strPos, caseSensitive );
        }

    private:
        struct InsertCharsFunctor
        {
            template <typename subCharType>
            static AINLINE void ProcChars( const subCharType *str, stringProvider *thisProv, size_t offset, size_t srcLen )
            {
                thisProv->InsertChars( offset, str, srcLen );
            }
        };

    public:
        void Insert( size_t offset, const stringProvider *right, size_t srcLen ) override final
        {
            StringProviderCharPipe <InsertCharsFunctor> ( right, this, offset, srcLen );
        }

    private:
        struct AppendCharsFunctor
        {
            template <typename subCharType>
            static AINLINE void ProcChars( const subCharType *str, stringProvider *thisProv, size_t strLen )
            {
                thisProv->AppendChars( str, strLen );
            }
        };

    public:
        void Append( const stringProvider *right ) override final
        {
            StringProviderCharPipe <AppendCharsFunctor> ( right, this, right->GetLength() );
        }

        bool IsUnicode( void ) const noexcept override final
        {
            return ( std::is_same <charType, char>::value == false );
        }

        bool IsSameCharType( const stringProvider *other ) const noexcept override final
        {
            return ( is_string_prov_char <charType> ( other ) );
        }

        abiString ToANSI( void ) const override final
        {
            if constexpr ( std::is_same <charType, char>::value == false )
            {
                auto convStr = CharacterUtil::ConvertStringsLength <charType, char, directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );

                return abiString::Make <directMSAllocator> ( convStr.GetConstString(), convStr.GetLength(), this->refPtr );
            }
            else
            {
                return abiString::Make <directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );
            }
        }

        abiWideString ToUnicode( void ) const override final
        {
            if constexpr ( std::is_same <charType, wchar_t>::value == false )
            {
                auto convStr = CharacterUtil::ConvertStringsLength <charType, wchar_t, directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );

                return abiWideString::Make <directMSAllocator> ( convStr.GetConstString(), convStr.GetLength(), this->refPtr );
            }
            else
            {
                return abiWideString::Make <directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );
            }
        }

        abiUTF8String ToUTF8( void ) const override final
        {
            if constexpr ( std::is_same <charType, char8_t>::value == false )
            {
                auto convStr = CharacterUtil::ConvertStringsLength <charType, char8_t, directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );

                return abiUTF8String::Make <directMSAllocator> ( convStr.GetConstString(), convStr.GetLength(), this->refPtr );
            }
            else
            {
                return abiUTF8String::Make <directMSAllocator> ( this->strData.GetConstString(), this->strData.GetLength(), this->refPtr );
            }
        }
    };

    struct ansiStringProvider final : public charStringProvider <char>
    {
        using charStringProvider <char>::charStringProvider;

        stringProvider* Clone( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <ansiStringProvider> ( refPtr->data.allocData, refPtr, *this, refPtr );
        }

        stringProvider* InheritConstruct( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <ansiStringProvider> ( refPtr->data.allocData, refPtr, refPtr );
        }

        // Dangerous function!
        const char* GetConstANSIString( void ) const noexcept override final
        {
            return this->strData.GetConstString();
        }
    };

    struct wideStringProvider final : public charStringProvider <wchar_t>
    {
        using charStringProvider <wchar_t>::charStringProvider;

        stringProvider* Clone( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <wideStringProvider> ( refPtr->data.allocData, refPtr, *this, refPtr );
        }

        stringProvider* InheritConstruct( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <wideStringProvider> ( refPtr->data.allocData, refPtr, refPtr );
        }

        // Dangerous function!
        const wchar_t* GetConstUnicodeString( void ) const noexcept final
        {
            return this->strData.GetConstString();
        }
    };

    struct char16StringProvider final : public charStringProvider <char16_t>
    {
        using charStringProvider <char16_t>::charStringProvider;

        stringProvider* Clone( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <char16StringProvider> ( refPtr->data.allocData, refPtr, *this, refPtr );
        }

        stringProvider* InheritConstruct( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <char16StringProvider> ( refPtr->data.allocData, refPtr, refPtr );
        }

        // Dangerous function!
        const char16_t* GetConstUTF16String( void ) const noexcept final
        {
            return this->strData.GetConstString();
        }
    };

    struct utf8StringProvider final : public charStringProvider <char8_t>
    {
        using charStringProvider <char8_t>::charStringProvider;

        stringProvider* Clone( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <utf8StringProvider> ( refPtr->data.allocData, refPtr, *this, refPtr );
        }

        stringProvider* InheritConstruct( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <utf8StringProvider> ( refPtr->data.allocData, refPtr, refPtr );
        }

        // Dangerous function!
        const char8_t* GetConstUTF8String( void ) const noexcept final
        {
            return this->strData.GetConstString();
        }
    };

    struct utf32StringProvider final : public charStringProvider <char32_t>
    {
        using charStringProvider <char32_t>::charStringProvider;

        stringProvider* Clone( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <utf32StringProvider> ( refPtr->data.allocData, refPtr, *this, refPtr );
        }

        stringProvider* InheritConstruct( MultiString *refPtr ) const override final
        {
            return eir::dyn_new_struct <utf32StringProvider> ( refPtr->data.allocData, refPtr, refPtr );
        }

        // Dangerous function!
        const char32_t* GetConstUTF32String( void ) const noexcept final
        {
            return this->strData.GetConstString();
        }
    };

    template <typename charType>
    inline charStringProvider <charType>* CreateStringProvider( void )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return eir::dyn_new_struct <ansiStringProvider> ( this->data.allocData, this, this );
        }
        if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return eir::dyn_new_struct <wideStringProvider> ( this->data.allocData, this, this );
        }
        if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return eir::dyn_new_struct <char16StringProvider> ( this->data.allocData, this, this );
        }
        if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return eir::dyn_new_struct <utf8StringProvider> ( this->data.allocData, this, this );
        }
        if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return eir::dyn_new_struct <utf32StringProvider> ( this->data.allocData, this, this );
        }

        // Cannot construct for this character type.
        throw eir_exception();
    }

    inline void DestroyStringProvider( stringProvider *prov )
    {
        eir::dyn_del_struct <stringProvider> ( this->data.allocData, this, prov );
    }

    template <typename charType>
    inline static void AppendStringProvider( stringProvider *prov, const charType *str, size_t len )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            prov->AppendChars( str, len );
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            prov->AppendChars( str, len );
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            prov->AppendChars( str, len );
        }
        else
        {
            // Only available to MSVC because it is a smarter compiler.
#ifdef _MSC_VER
            static_assert( false, "invalid character type for string provider append" );
#endif //_MSC_VER
        }
    }

    // Compatibility upgrade function for input characters.
    template <typename otherCharType>
    AINLINE void compat_upgrade( void )
    {
        stringProvider *curProv = this->data.strData;

        // If we have no string provider, we just create one that is the same as the required character type.
        if ( !curProv )
        {
            this->data.strData = CreateStringProvider <otherCharType> ();

            return;
        }

        // Otherwise we check if the target character type (otherCharType) can fit into the current provider.
        // If not then we upgrade to the target.
        if ( curProv->IsUnicode() )
        {
            // Anything can fit.
            return;
        }

        // If we just want to input ANSI things, then we are fine.
        if constexpr ( std::is_same <otherCharType, char>::value )
        {
            return;
        }

        // Upgrade to the target.
        //TODO: maybe limit us to certain types, such as char or wchar_t.
        charStringProvider <otherCharType> *newProv = CreateStringProvider <otherCharType> ();

        try
        {
            // Take over the old characters.
            newProv->Append( curProv );

            // Delete old character stuff.
            DestroyStringProvider( curProv );

            this->data.strData = newProv;
        }
        catch( ... )
        {
            DestroyStringProvider( newProv );
            throw;
        }
    }

    // Upgrading compatibility based on a source string provider.
    AINLINE void compat_strprov_upgrade( const stringProvider *other )
    {
        stringProvider *curProv = this->data.strData;

        // If we had no provider, then we just create one that matches the target.
        if ( !curProv )
        {
            this->data.strData = other->InheritConstruct( this );

            return;
        }

        // If our current string provider is already unicode then we are ok.
        if ( curProv->IsUnicode() )
        {
            return;
        }

        // Otherwise if the other is not unicode too, then we are ok too.
        if ( other->IsUnicode() == false )
        {
            return;
        }

        stringProvider *newProv = other->InheritConstruct( this );

        try
        {
            // Take over the old characters.
            newProv->Append( curProv );

            // Delete old character stuff.
            DestroyStringProvider( curProv );

            this->data.strData = newProv;
        }
        catch( ... )
        {
            DestroyStringProvider( newProv );
            throw;
        }
    }

public:
    inline MultiString( void ) noexcept
    {
        this->data.strData = nullptr;
    }

    template <typename charType>
    inline MultiString( const charType *right, size_t cplen )
    {
        this->data.strData = CreateStringProvider <charType> ();

        this->data.strData->AppendChars( right, cplen );
    }

    template <typename charType>
    inline MultiString( const charType *right )
    {
        this->data.strData = CreateStringProvider <charType> ();

        this->data.strData->AppendChars( right, cplen_tozero( right ) );
    }

    template <typename charType, typename strAllocatorType>
    inline explicit MultiString( const eir::String <charType, strAllocatorType>& right )
    {
        this->data.strData = CreateStringProvider <charType> ();

        this->data.strData->AppendChars( right.GetConstString(), right.GetLength() );
    }

    // Special constructor for creating a string with allocator arguments.
    template <typename... Args>
    inline MultiString( constr_with_alloc _, Args&&... allocArgs )
        : data( size_opt_constr_with_default::DEFAULT, std::forward <Args> ( allocArgs )... )
    {
        this->data.strData = nullptr;
    }

    // Special constructor for taking-over the allocation data.
    // We do not copy the string data though, so do not get confused!
    inline MultiString( constr_with_alloc_copy _, const MultiString& right ) : data( right.data )
    {
        // But we do not start with string data.
        this->data.strData = nullptr;
    }

    template <typename charType>
    AINLINE static MultiString Make( const charType *str, size_t len )
    {
        MultiString path;

        path.compat_upgrade <charType> ();

        AppendStringProvider <charType> ( path.data.strData, str, len );

        return path;
    }

    // NOTE: this function does importantly require that both MultiString have the same allocatorType.
    inline MultiString( const MultiString& right ) : data( right.data )
    {
        this->data.strData = nullptr;

        if ( right.data.strData )
        {
            this->data.strData = right.data.strData->Clone( this );
        }
    }

    template <typename subAllocatorType>
    inline MultiString( const MultiString <subAllocatorType>& right )
    {
        this->data.strData = nullptr;

        this->operator += ( right );
    }

    // WARNING: only move if you know that allocators are the same.
    inline MultiString( MultiString&& right ) noexcept : data( right.data )
    {
        this->data.strData = right.data.strData;

        right.data.strData = nullptr;
    }

    inline ~MultiString( void )
    {
        if ( stringProvider *provider = this->data.strData )
        {
            DestroyStringProvider( provider );
        }
    }

    inline size_t size( void ) const
    {
        size_t outSize = 0;

        if ( stringProvider *provider = this->data.strData )
        {
            outSize = provider->GetLength();
        }

        return outSize;
    }

    inline size_t charlen( void ) const
    {
        size_t outLen = 0;

        if ( stringProvider *provider = this->data.strData )
        {
            outLen = provider->GetDecodedLength();
        }

        return outLen;
    }

    inline void resize( size_t strSize )
    {
        if ( this->data.strData == nullptr )
        {
            this->data.strData = CreateStringProvider <char> ();
        }

        this->data.strData->SetSize( strSize );
    }

    inline bool empty( void ) const
    {
        bool isEmpty = true;

        if ( stringProvider *provider = this->data.strData )
        {
            isEmpty = ( provider->GetLength() == 0 );
        }

        return isEmpty;
    }

    inline void clear( void ) noexcept
    {
        if ( stringProvider *provider = this->data.strData )
        {
            provider->Clear();
        }
    }

    inline char at( size_t strPos ) const
    {
        if ( strPos >= size() || !this->data.strData )
        {
            throw std::out_of_range( "strPos too big!" );
        }

        return this->data.strData->GetCharacterANSI( strPos );
    }

    inline wchar_t at_w( size_t strPos ) const
    {
        if ( strPos >= size() || !this->data.strData )
        {
            throw std::out_of_range( "strPos too big!" );
        }

        return this->data.strData->GetCharacterUnicode( strPos );
    }

    inline char32_t charat_u32( size_t charPos ) const
    {
        if ( charPos >= charlen() || !this->data.strData )
        {
            throw std::out_of_range( "charPos too big!" );
        }

        return this->data.strData->GetCharacter( charPos );
    }

    template <typename charType>
    inline bool compareCharAt( charType refChar, size_t strPos, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( stringProvider *provider = this->data.strData )
        {
            isEqual = ( provider->CompareCharacterAt( refChar, strPos, caseSensitive ) == eCompResult::EQUAL );
        }

        return isEqual;
    }

    // Compares two strings and returns -1 if this string is smaller, 0 if both are same,
    // 1 if right is smaller.
    template <typename otherAllocatorType>
    inline eCompResult compare( const MultiString <otherAllocatorType>& right, bool caseSensitive = true ) const
    {
        const stringProvider *leftStrProv = this->data.strData;
        const typename MultiString <otherAllocatorType>::stringProvider *rightStrProv = right.data.strData;

        if ( !leftStrProv && !rightStrProv )
            return eCompResult::EQUAL;

        if ( !leftStrProv )
            return eCompResult::LEFT_LESS;

        if ( !rightStrProv )
            return eCompResult::LEFT_GREATER;

        eCompResult result = eCompResult::LEFT_GREATER;

        size_t rightLen = rightStrProv->GetLength();

        MultiString <otherAllocatorType>::DynStringProviderCharPipe( rightStrProv,
            [&]( const auto *rightStr )
        {
            result = leftStrProv->CompareToChars( rightStr, rightLen, caseSensitive );
        });

        return result;
    }

    inline bool operator < ( const MultiString& right ) const
    {
        return ( this->compare( right ) == eCompResult::LEFT_LESS );
    }

    template <typename otherAllocatorType>
    inline bool operator < ( const MultiString <otherAllocatorType>& right ) const
    {
        return ( this->compare( right ) == eCompResult::LEFT_LESS );
    }

    // Inserts a string of specified length into a character offset.
    template <typename otherCharType>
    inline void insert( size_t offset, const otherCharType *src, size_t srcCPCount )
    {
        this->compat_upgrade <otherCharType> ();

        this->data.strData->InsertChars( offset, src, srcCPCount );
    }

    // Inserts a string into a character offset.
    template <typename otherCharType>
    inline void insert( size_t offset, const otherCharType *src )
    {
        this->insert( offset, src, cplen_tozero( src ) );
    }

    // Inserts a string of specified length into a character offset.
    inline void insert( size_t offset, const MultiString& src, size_t srcCPLen )
    {
        const stringProvider *rightProv = src.data.strData;

        if ( !rightProv )
            return;

        this->compat_strprov_upgrade( rightProv );

        this->data.strData->Insert( offset, rightProv, srcCPLen );
    }

    // Inserts a string into a character offset.
    inline void insert( size_t offset, const MultiString& src )
    {
        const stringProvider *rightProv = src.data.strData;

        if ( !rightProv )
            return;

        this->compat_strprov_upgrade( rightProv );

        this->data.strData->Insert( offset, rightProv, rightProv->GetLength() );
    }

    // Appends a string onto this string.
    template <typename charType>
    inline void append( const charType *appendStr, size_t appendCPLen )
    {
        this->compat_upgrade <charType> ();

        this->data.strData->AppendChars( appendStr, appendCPLen );
    }

    template <typename charType>
    inline void append( const charType *appendStr )
    {
        this->append( appendStr, cplen_tozero( appendStr ) );
    }

    // Returns a sub-string based on character offsets.
    inline MultiString substr( size_t pos, size_t len = std::numeric_limits <size_t>::max() ) const
    {
        MultiString subresult( eir::constr_with_alloc_copy::DEFAULT, *this );

        if ( stringProvider *prov = this->data.strData )
        {
            size_t cur_pos = pos;
            size_t end_pos = pos + len;

            size_t num_iter = 0;

            size_t char_count = prov->GetDecodedLength();

            while ( true )
            {
                size_t cur_iter = num_iter++;

                if ( cur_iter >= len )
                    break;

                size_t n = cur_pos++;

                if ( n >= end_pos )
                    break;

                if ( n >= char_count )
                    break;

                subresult += prov->GetCharacter( n );
            }
        }

        return subresult;
    }

    template <typename otherCharType>
    inline MultiString& operator = ( const otherCharType *right )
    {
        if ( this->data.strData )
        {
            DestroyStringProvider( this->data.strData );

            this->data.strData = nullptr;
        }

        this->operator += ( right );

        return *this;
    }

    // NOTE: this copy operator uses the fact that both MultiString have the same allocatorType.
    inline MultiString& operator = ( const MultiString& right )
    {
        if ( this->data.strData )
        {
            DestroyStringProvider( this->data.strData );

            this->data.strData = nullptr;
        }

        if ( right.data.strData )
        {
            this->data.strData = right.data.strData->Clone( this );
        }

        return *this;
    }

    inline MultiString& operator = ( MultiString&& right ) noexcept
    {
        if ( this->data.strData )
        {
            DestroyStringProvider( this->data.strData );

            this->data.strData = nullptr;
        }

        this->data.strData = right.data.strData;

        right.data.strData = nullptr;

        return *this;
    }

    template <typename charType>
    inline bool equals( const charType *right, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( this->data.strData )
        {
            isEqual = ( this->data.strData->CompareToChars( right, cplen_tozero( right ), caseSensitive ) == eCompResult::EQUAL );
        }
        else if ( *right == (charType)0 )
        {
            isEqual = true;
        }

        return isEqual;
    }

    inline bool equals( const MultiString& right, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( this->data.strData && right.data.strData )
        {
            isEqual = ( this->data.strData->CompareTo( right.data.strData, caseSensitive ) == eCompResult::EQUAL );
        }
        else if ( this->data.strData == nullptr && right.data.strData == nullptr )
        {
            isEqual = true;
        }

        return isEqual;
    }

    inline bool operator == ( const MultiString& right ) const
    {
        return equals( right, true );
    }

    template <typename otherAllocatorType>
    inline bool operator == ( const MultiString <otherAllocatorType>& right ) const
    {
        bool isEqual = false;

        right.char_dispatch(
            [&]( const auto *str )
        {
            isEqual = this->equals( str, true );
        });

        return isEqual;
    }

    inline bool operator != ( const MultiString& right ) const
    {
        return !( this->operator == ( right ) );
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const MultiString <otherAllocatorType>& right ) const
    {
        return !( this->operator == ( right ) );
    }

    template <typename otherCharType>
    inline bool operator == ( const otherCharType *right ) const
    {
        return equals( right, true );
    }

    template <typename otherCharType>
    inline bool operator != ( const otherCharType *right ) const
    {
        return !( this->operator == ( right ) );
    }

    inline MultiString operator + ( const MultiString& right ) const
    {
        MultiString newPath( *this );

        if ( stringProvider *rightProv = right.data.strData )
        {
            newPath.compat_strprov_upgrade( rightProv );

            newPath.data.strData->Append( rightProv );
        }

        return newPath;
    }

    // Those friend functions are really cool. Those actually are hidden static functions.
    template <typename charType>
    AINLINE friend MultiString operator + ( const MultiString& left, const charType *right )
    {
        MultiString newPath( left );
        newPath.compat_upgrade <charType> ();
        newPath.data.strData->AppendChars( right, cplen_tozero( right ) );
        return newPath;
    }
    template <typename charType>
    AINLINE friend MultiString operator + ( const charType *left, const MultiString& right )
    {
        MultiString outStr( constr_with_alloc::DEFAULT, right.data.allocData );
        outStr.compat_upgrade <charType> ();
        outStr.data.strData->AppendChars( left, cplen_tozero( left ) );
        outStr += right;
        return outStr;
    }
    template <typename charType>
    AINLINE friend MultiString operator + ( MultiString&& left, const charType *right )
    {
        left.compat_upgrade <charType> ();
        left.data.strData->AppendChars( right, cplen_tozero( right ) );
        return std::move( left );
    }
    template <typename charType>
    AINLINE friend MultiString operator + ( const charType *left, MultiString&& right )
    {
        right.compat_upgrade <charType> ();
        right.insert( 0, left );
        return std::move( right );
    }

    template <typename otherAllocatorType>
    inline MultiString operator + ( const MultiString <otherAllocatorType>& right ) const
    {
        MultiString newPath( *this );

        right.char_dispatch(
            [&]( const auto *str )
        {
            newPath.append( str, right.size() );
        });

        return newPath;
    }

    inline MultiString& operator += ( const MultiString& right )
    {
        if ( stringProvider *rightProv = right.data.strData )
        {
            this->compat_strprov_upgrade( rightProv );

            this->data.strData->Append( rightProv );
        }

        return *this;
    }

    template <typename otherAllocatorType>
    inline MultiString& operator += ( const MultiString <otherAllocatorType>& right )
    {
        size_t rightLen = right.size();

        right.char_dispatch(
            [&]( const auto *str )
        {
            this->append( str, rightLen );
        });

        return *this;
    }

    template <typename otherCharType, typename otherAllocatorType>
    inline MultiString& operator += ( const eir::String <otherCharType, otherAllocatorType>& right )
    {
        this->compat_upgrade <otherCharType> ();

        this->data.strData->AppendChars( right.GetConstString(), right.GetLength() );

        return *this;
    }

    template <typename otherCharType>
    inline MultiString& operator += ( const otherCharType *right )
    {
        this->compat_upgrade <otherCharType> ();

        this->data.strData->AppendChars( right, cplen_tozero( right ) );

        return *this;
    }

    // We have to spell these things out because otherwise we could get ugly error messages.
    inline MultiString& operator += ( const char& right )
    {
        this->compat_upgrade <char> ();

        this->data.strData->AppendChars( &right, 1 );

        return *this;
    }

    inline MultiString& operator += ( const wchar_t& right )
    {
        this->compat_upgrade <wchar_t> ();

        this->data.strData->AppendChars( &right, 1 );

        return *this;
    }

    inline MultiString& operator += ( const char16_t& right )
    {
        this->compat_upgrade <char16_t> ();

        this->data.strData->AppendChars( &right, 1 );

        return *this;
    }

    inline MultiString& operator += ( const char32_t& right )
    {
        this->compat_upgrade <char32_t> ();

        this->data.strData->AppendChars( &right, 1 );

        return *this;
    }

    inline MultiString& operator += ( const char8_t& right )
    {
        this->compat_upgrade <char8_t> ();

        this->data.strData->AppendChars( &right, 1 );

        return *this;
    }

    inline const char* c_str( void ) const noexcept
    {
        const char *outString = GetEmptyString <char> ();

        if ( stringProvider *provider = this->data.strData )
        {
            outString = provider->GetConstANSIString();
        }

        return outString;
    }

    inline const wchar_t* w_str( void ) const noexcept
    {
        const wchar_t *outString = GetEmptyString <wchar_t> ();

        if ( stringProvider *provider = this->data.strData )
        {
            outString = provider->GetConstUnicodeString();
        }

        return outString;
    }

    inline const char16_t* u16_str( void ) const noexcept
    {
        const char16_t *outString = GetEmptyString <char16_t> ();

        if ( stringProvider *provider = this->data.strData )
        {
            outString = provider->GetConstUTF16String();
        }

        return outString;
    }

    inline const char8_t* u8_str( void ) const noexcept
    {
        const char8_t *outString = GetEmptyString <char8_t> ();

        if ( stringProvider *provider = this->data.strData )
        {
            outString = provider->GetConstUTF8String();
        }

        return outString;
    }

    inline const char32_t* u32_str( void ) const noexcept
    {
        const char32_t *outString = GetEmptyString <char32_t> ();

        if ( stringProvider *provider = this->data.strData )
        {
            outString = provider->GetConstUTF32String();
        }

        return outString;
    }

    // More advanced functions.
    template <typename subAllocatorType, typename... Args>
    inline eir::String <char, subAllocatorType> convert_ansi( Args&&... allocArgs ) const
    {
        eir::String <char, subAllocatorType> ansiOut( nullptr, 0, std::forward <Args> ( allocArgs )... );

        if ( stringProvider *provider = this->data.strData )
        {
            auto ansiConv = provider->ToANSI();

            ansiOut.Append( ansiConv.c_str(), ansiConv.size() );
        }

        return ansiOut;
    }

    template <typename subAllocatorType, typename... Args>
    inline eir::String <wchar_t, subAllocatorType> convert_unicode( Args&&... allocArgs ) const
    {
        eir::String <wchar_t, subAllocatorType> unicodeOut( nullptr, 0, std::forward <Args> ( allocArgs )... );

        if ( stringProvider *provider = this->data.strData )
        {
            auto wideConv = provider->ToUnicode();

            unicodeOut.Append( wideConv.c_str(), wideConv.size() );
        }

        return unicodeOut;
    }

    // Changes the format of the string backbuffer to the supplied character type.
    // Throws "eir_exception" if the character type is not supported.
    template <typename charType>
    inline void transform_to( void )
    {
        stringProvider *origProv = this->data.strData;

        if ( !origProv )
        {
            // We actually do request to be that character type, so deliver.
            this->data.strData = CreateStringProvider <charType> ();

            return;
        }

        // Check if origProv already is that transformation char type.
        if ( GetConstCharStringProvider <charType> ( origProv ) != nullptr )
        {
            return;
        }

        // Need to turn things into our type.
        stringProvider *newProv = CreateStringProvider <charType> ();

        try
        {
            newProv->Append( origProv );
        }
        catch( ... )
        {
            DestroyStringProvider( newProv );
            throw;
        }

        DestroyStringProvider( origProv );

        this->data.strData = newProv;
    }

    // Returns the string backbuffer in the provided format.
    template <typename charType>
    inline const charType* to_char( void ) const
    {
        const stringProvider *strProv = this->data.strData;

        if ( !strProv )
        {
            return nullptr;
        }

        return GetConstCharStringProvider <charType> ( strProv );
    }

    template <typename callbackType>
    AINLINE decltype(auto) char_dispatch( const callbackType& cb ) const
    {
        stringProvider *prov = this->data.strData;

        if ( !prov )
        {
            return cb( GetEmptyString <char> () );
        }
        
        return DynStringProviderCharPipe( prov, cb );
    }

    // Functions to determine the underlying string type.
    template <typename charType>
    inline bool is_underlying_char( void ) const
    {
        stringProvider *prov = this->data.strData;

        if ( prov == nullptr )
            return false;

        return is_string_prov_char <charType> ( prov );
    }

    inline stringProvider* string_prov( void ) const
    {
        return this->data.strData;
    }

    inline const allocatorType& GetAllocData( void ) const
    {
        return this->data.allocData;
    }

private:
    struct fields
    {
        stringProvider *strData;
    };

    size_opt <hasObjectAllocator, allocatorType, fields> data;
};

// Flesh out the allocator redirect from CSP to MultiString.
MSTR_TEMPLARGS
CSP_TEMPLARGS
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN MultiString MSTR_TEMPLUSE::template charStringProvider CSP_TEMPLUSE::cspRedirAlloc::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
{
    charStringProvider *hostStruct = LIST_GETITEM( charStringProvider, refMem, strData );
    MultiString *refStruct = hostStruct->refPtr;
    return refStruct->data.allocData.Allocate( refStruct, memSize, alignment );
}
MSTR_TEMPLARGS
CSP_TEMPLARGS
IMPL_HEAP_REDIR_METH_RESIZE_RETURN MultiString MSTR_TEMPLUSE::template charStringProvider CSP_TEMPLUSE::cspRedirAlloc::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS
{
    charStringProvider *hostStruct = LIST_GETITEM( charStringProvider, refMem, strData );
    MultiString *refStruct = hostStruct->refPtr;
    return refStruct->data.allocData.Resize( refStruct, objMem, reqNewSize );
}
MSTR_TEMPLARGS
CSP_TEMPLARGS
IMPL_HEAP_REDIR_METH_FREE_RETURN MultiString MSTR_TEMPLUSE::template charStringProvider CSP_TEMPLUSE::cspRedirAlloc::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
{
    charStringProvider *hostStruct = LIST_GETITEM( charStringProvider, refMem, strData );
    MultiString *refStruct = hostStruct->refPtr;
    refStruct->data.allocData.Free( refStruct, memPtr );
}

}

#endif //_EIR_MULTISTRING_HEADER_
