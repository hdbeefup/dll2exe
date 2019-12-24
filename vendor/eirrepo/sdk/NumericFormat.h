/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/NumericFormat.h
*  PURPOSE:     Numeric formatting with Eir SDK character strings
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Usually the standard library is the provider for converting numbers into
// string representations. But since the algorithms behind that magic are
// rather simple, we want to provide them ourselves.

#ifndef _EIR_SDK_NUMERIC_FORMATTING_
#define _EIR_SDK_NUMERIC_FORMATTING_

#include "MacroUtils.h"
#include "String.h"
#include "UniChar.h"

#include <type_traits>

namespace eir
{

template <typename charType>
AINLINE charType GetCharacterForNumeric( unsigned int numeric )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        switch( numeric )
        {
        case 0: return '0';
        case 1: return '1';
        case 2: return '2';
        case 3: return '3';
        case 4: return '4';
        case 5: return '5';
        case 6: return '6';
        case 7: return '7';
        case 8: return '8';
        case 9: return '9';
        case 10: return 'a';
        case 11: return 'b';
        case 12: return 'c';
        case 13: return 'd';
        case 14: return 'e';
        case 15: return 'f';
        }

        return '?';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        switch( numeric )
        {
        case 0: return L'0';
        case 1: return L'1';
        case 2: return L'2';
        case 3: return L'3';
        case 4: return L'4';
        case 5: return L'5';
        case 6: return L'6';
        case 7: return L'7';
        case 8: return L'8';
        case 9: return L'9';
        case 10: return L'a';
        case 11: return L'b';
        case 12: return L'c';
        case 13: return L'd';
        case 14: return L'e';
        case 15: return L'f';
        }

        return L'?';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        switch( numeric )
        {
        case 0: return U'0';
        case 1: return U'1';
        case 2: return U'2';
        case 3: return U'3';
        case 4: return U'4';
        case 5: return U'5';
        case 6: return U'6';
        case 7: return U'7';
        case 8: return U'8';
        case 9: return U'9';
        case 10: return U'a';
        case 11: return U'b';
        case 12: return U'c';
        case 13: return U'd';
        case 14: return U'e';
        case 15: return U'f';
        }

        return U'?';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        switch( numeric )
        {
        case 0: return u'0';
        case 1: return u'1';
        case 2: return u'2';
        case 3: return u'3';
        case 4: return u'4';
        case 5: return u'5';
        case 6: return u'6';
        case 7: return u'7';
        case 8: return u'8';
        case 9: return u'9';
        case 10: return u'a';
        case 11: return u'b';
        case 12: return u'c';
        case 13: return u'd';
        case 14: return u'e';
        case 15: return u'f';
        }

        return u'?';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        switch( numeric )
        {
        case 0: return (char8_t)'0';
        case 1: return (char8_t)'1';
        case 2: return (char8_t)'2';
        case 3: return (char8_t)'3';
        case 4: return (char8_t)'4';
        case 5: return (char8_t)'5';
        case 6: return (char8_t)'6';
        case 7: return (char8_t)'7';
        case 8: return (char8_t)'8';
        case 9: return (char8_t)'9';
        case 10: return (char8_t)'a';
        case 11: return (char8_t)'b';
        case 12: return (char8_t)'c';
        case 13: return (char8_t)'d';
        case 14: return (char8_t)'e';
        case 15: return (char8_t)'f';
        }

        return (char8_t)'?';
    }

    throw eir_exception();
}

template <typename ucpType>
AINLINE bool GetNumericForCharacter( ucpType cp, unsigned int& valueOut )
{
    if constexpr ( std::is_same <ucpType, char>::value )
    {
        switch( cp )
        {
        case '0': valueOut = 0; return true;
        case '1': valueOut = 1; return true;
        case '2': valueOut = 2; return true;
        case '3': valueOut = 3; return true;
        case '4': valueOut = 4; return true;
        case '5': valueOut = 5; return true;
        case '6': valueOut = 6; return true;
        case '7': valueOut = 7; return true;
        case '8': valueOut = 8; return true;
        case '9': valueOut = 9; return true;
        case 'a': case 'A': valueOut = 10; return true;
        case 'b': case 'B': valueOut = 11; return true;
        case 'c': case 'C': valueOut = 12; return true;
        case 'd': case 'D': valueOut = 13; return true;
        case 'e': case 'E': valueOut = 14; return true;
        case 'f': case 'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, wchar_t>::value )
    {
        switch( cp )
        {
        case L'0': valueOut = 0; return true;
        case L'1': valueOut = 1; return true;
        case L'2': valueOut = 2; return true;
        case L'3': valueOut = 3; return true;
        case L'4': valueOut = 4; return true;
        case L'5': valueOut = 5; return true;
        case L'6': valueOut = 6; return true;
        case L'7': valueOut = 7; return true;
        case L'8': valueOut = 8; return true;
        case L'9': valueOut = 9; return true;
        case L'a': case L'A': valueOut = 10; return true;
        case L'b': case L'B': valueOut = 11; return true;
        case L'c': case L'C': valueOut = 12; return true;
        case L'd': case L'D': valueOut = 13; return true;
        case L'e': case L'E': valueOut = 14; return true;
        case L'f': case L'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char16_t>::value )
    {
        switch( cp )
        {
        case u'0': valueOut = 0; return true;
        case u'1': valueOut = 1; return true;
        case u'2': valueOut = 2; return true;
        case u'3': valueOut = 3; return true;
        case u'4': valueOut = 4; return true;
        case u'5': valueOut = 5; return true;
        case u'6': valueOut = 6; return true;
        case u'7': valueOut = 7; return true;
        case u'8': valueOut = 8; return true;
        case u'9': valueOut = 9; return true;
        case u'a': case u'A': valueOut = 10; return true;
        case u'b': case u'B': valueOut = 11; return true;
        case u'c': case u'C': valueOut = 12; return true;
        case u'd': case u'D': valueOut = 13; return true;
        case u'e': case u'E': valueOut = 14; return true;
        case u'f': case u'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char32_t>::value )
    {
        switch( cp )
        {
        case U'0': valueOut = 0; return true;
        case U'1': valueOut = 1; return true;
        case U'2': valueOut = 2; return true;
        case U'3': valueOut = 3; return true;
        case U'4': valueOut = 4; return true;
        case U'5': valueOut = 5; return true;
        case U'6': valueOut = 6; return true;
        case U'7': valueOut = 7; return true;
        case U'8': valueOut = 8; return true;
        case U'9': valueOut = 9; return true;
        case U'a': case U'A': valueOut = 10; return true;
        case U'b': case U'B': valueOut = 11; return true;
        case U'c': case U'C': valueOut = 12; return true;
        case U'd': case U'D': valueOut = 13; return true;
        case U'e': case U'E': valueOut = 14; return true;
        case U'f': case U'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char8_t>::value )
    {
        switch( cp )
        {
        case (char8_t)'0': valueOut = 0; return true;
        case (char8_t)'1': valueOut = 1; return true;
        case (char8_t)'2': valueOut = 2; return true;
        case (char8_t)'3': valueOut = 3; return true;
        case (char8_t)'4': valueOut = 4; return true;
        case (char8_t)'5': valueOut = 5; return true;
        case (char8_t)'6': valueOut = 6; return true;
        case (char8_t)'7': valueOut = 7; return true;
        case (char8_t)'8': valueOut = 8; return true;
        case (char8_t)'9': valueOut = 9; return true;
        case (char8_t)'a': case (char8_t)'A': valueOut = 10; return true;
        case (char8_t)'b': case (char8_t)'B': valueOut = 11; return true;
        case (char8_t)'c': case (char8_t)'C': valueOut = 12; return true;
        case (char8_t)'d': case (char8_t)'D': valueOut = 13; return true;
        case (char8_t)'e': case (char8_t)'E': valueOut = 14; return true;
        case (char8_t)'f': case (char8_t)'F': valueOut = 15; return true;
        }

        return false;
    }

    return false;
}

template <typename charType>
AINLINE charType GetMinusSignCodepoint( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '-';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'-';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'-';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'-';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return (char8_t)'-';
    }

    throw eir_exception();
}

template <typename charType, typename allocatorType, typename numberType, typename... Args>
inline eir::String <charType, allocatorType> to_string( const numberType& value, unsigned int base = 10, Args&&... theArgs )
{
    eir::String <charType, allocatorType> strOut( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( theArgs )... );

    if ( base == 0 || base > 16 )
        return strOut;

    if constexpr ( std::is_integral <numberType>::value )
    {
        numberType bitStream = value;

        bool isSigned = false;

        typename std::make_unsigned <numberType>::type u_bitStream;

        if constexpr ( std::is_signed <numberType>::value )
        {
            isSigned = ( bitStream < 0 );

            if ( isSigned )
            {
                u_bitStream = (typename std::make_unsigned <numberType>::type)( -bitStream );
            }
            else
            {
                u_bitStream = (typename std::make_unsigned <numberType>::type)( bitStream );
            }
        }
        else
        {
            u_bitStream = bitStream;
        }

        if ( u_bitStream == 0 )
        {
            // Just print out the zero.
            strOut += GetCharacterForNumeric <charType> ( 0 );
        }
        else
        {
            // We build up the polynome in reverse order.
            // Then we use the eir::String reverse method to turn it around.
            while ( u_bitStream > 0 )
            {
                unsigned int polynome_coeff = ( u_bitStream % base );

                // Move down all polynome entries by one.
                u_bitStream /= base;

                // Add this polynome value.
                strOut += GetCharacterForNumeric <charType> ( polynome_coeff );
            }
        }

        // Add a sign if required.
        if ( isSigned )
        {
            strOut += GetMinusSignCodepoint <charType> ();
        }

        // Reverse the (polynome) string.
        // This only works because each number is only one code point.
        strOut.reverse();
    }
    else
    {
#ifdef _MSC_VER
        static_assert( false, "invalid type in to_string conversion" );
#endif //_MSC_VER_
    }

    return strOut;
}

template <typename numberType, typename charType>
inline numberType to_number_len( const charType *strnum, size_t strlen, unsigned int base = 10 )
{
    // TODO: add floating point support.

    if ( base == 0 || base > 16 )
        return 0;

    typedef typename character_env <charType>::ucp_t ucp_t;

    charenv_charprov_tocplen <charType> prov( strnum, strlen );
    character_env_iterator <charType, decltype(prov)> iter( strnum, std::move( prov ) );

    bool isSigned = false;

    (void)isSigned;

    if constexpr ( std::is_signed <numberType>::value )
    {
        ucp_t ucp = iter.Resolve();

        if ( ucp == GetMinusSignCodepoint <ucp_t> () )
        {
            isSigned = true;

            iter.Increment();
        }
    }

    numberType result = 0;

    while ( !iter.IsEnd() )
    {
        ucp_t ucp = iter.ResolveAndIncrement();

        unsigned int val;
        bool got_val = GetNumericForCharacter( ucp, val );

        if ( !got_val )
        {
            return 0;
        }

        if ( val >= base )
        {
            return 0;
        }

        result *= base;
        result += val;
    }

    if constexpr ( std::is_signed <numberType>::value )
    {
        return ( isSigned ? -result : result );
    }
    else
    {
        return result;
    }
}

template <typename numberType, typename charType>
inline numberType to_number( const charType *strnum, unsigned int base = 10 )
{
    return to_number_len <numberType> ( strnum, cplen_tozero( strnum ), base );
}

}

#endif //_EIR_SDK_NUMERIC_FORMATTING_
