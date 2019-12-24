/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MacroUtils.h
*  PURPOSE:     Common macros in the SDK
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _COMMON_MACRO_UTILITIES_
#define _COMMON_MACRO_UTILITIES_

// Basic always inline definition.
#ifndef AINLINE
#ifdef _MSC_VER
#define AINLINE __forceinline
#elif __linux__
#define AINLINE inline
#else
#define AINLINE inline
#endif
#endif

#ifndef _MSC_VER
#define abstract
#endif

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif

#endif //_COMMON_MACRO_UTILITIES_