/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.vmem.h
*  PURPOSE:     OS implementation of virtual memory mapping
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_OS_VIRTUAL_MEMORY_FUNCS_
#define _NATIVE_OS_VIRTUAL_MEMORY_FUNCS_

// For FATAL_ASSERT.
#include "eirutils.h"

// For size_t.
#include <stddef.h>

#if defined(__linux__)
#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif //CROSS PLATFORM CODE

// Functions for reserving, freeing, committing and decomitting virtual memory regions.
// By implementing this interface for a certain platform you enable many features of
// the Eir development environment basing on very to-the-metal memory semantics!
struct NativeVirtualMemoryAccessor
{
private:
#ifdef _WIN32
    SYSTEM_INFO _systemInfo;
#elif defined(__linux__)
    // Some system metrics.
    size_t _sys_page_size;
#endif //_WIN32

public:
    inline NativeVirtualMemoryAccessor( void )
    {
#ifdef _WIN32
        GetSystemInfo( &_systemInfo );
#elif defined(__linux__)
        long linux_page_size = sysconf( _SC_PAGESIZE );

        FATAL_ASSERT( linux_page_size > 0 );

        this->_sys_page_size = (size_t)linux_page_size;
#endif //_WIN32
    }

    // As long as we do not store complicated things we are fine with default methods.
    inline NativeVirtualMemoryAccessor( NativeVirtualMemoryAccessor&& right ) = default;
    inline NativeVirtualMemoryAccessor( const NativeVirtualMemoryAccessor& right ) = default;

    inline NativeVirtualMemoryAccessor& operator = ( NativeVirtualMemoryAccessor&& right ) = default;
    inline NativeVirtualMemoryAccessor& operator = ( const NativeVirtualMemoryAccessor& right ) = default;

    // Platform dependent query functions.
    inline size_t GetPlatformPageSize( void ) const
    {
        // The page size is the amount of bytes the smallest requestable memory unit on hardware consists of.

        size_t page_size = 0;

#ifdef _WIN32
        page_size = (size_t)this->_systemInfo.dwPageSize;
#elif defined(__linux__)
        page_size = this->_sys_page_size;
#else
#error No page size query function for this architecture.
#endif

        return page_size;
    }

    inline size_t GetPlatformAllocationGranularity( void ) const
    {
        // Allocation granularity defines the size of memory requestable by-minimum from the OS.
        // On Windows you usually cannot request memory by page-size but have to create arenas
        // that consist of multiple pages, managing memory inside manually.

        //TODO: for systems other than Windows it could make sense to specifically introduce
        // a recommended allocation granularity.

        size_t arena_size = 0;

#ifdef _WIN32
        arena_size = (size_t)this->_systemInfo.dwAllocationGranularity;
#elif defined(__linux__)
        // I am not aware of any limitation in the Linux kernel of this nature.
        arena_size = this->_sys_page_size;
#else
#error No memory arena query function for this architecture.
#endif

        return arena_size;
    }

    // Cross-platform memory request API.
    // Allocates memory randomly or at the exactly specified position using the provided size.
    // The allocated memory does not have to be accessible after allocation; it could have to
    // be committed first.
    // MUST BE AN atomic operation.
    static inline void* RequestVirtualMemory( void *memPtr, size_t memSize )
    {
        void *actualMemPtr = nullptr;

#ifdef _WIN32
        actualMemPtr = VirtualAlloc( memPtr, (SIZE_T)memSize, MEM_RESERVE, PAGE_READWRITE );
#elif defined(__linux__)
        int mmap_flags = MAP_UNINITIALIZED|MAP_PRIVATE|MAP_ANONYMOUS;

        bool do_fixed = false;

        if ( memPtr != nullptr )
        {
            // We must not use MAP_FIXED because it is broken shit.
            // Instead we must do verifications.
            do_fixed = true;

            // Under Linux 4.17 there is this new flag called MAP_FIXED_NOREPLACE.
            // I am happy that the Linux developers actually improve by comparison :-)
            mmap_flags |= MAP_FIXED_NOREPLACE;
        }

        actualMemPtr = mmap( memPtr, memSize, PROT_NONE, mmap_flags, -1, 0 );

        if ( actualMemPtr == MAP_FAILED )
        {
            return nullptr;
        }

        if ( do_fixed && actualMemPtr != memPtr )
        {
            munmap( actualMemPtr, memSize );
            return nullptr;
        }
#endif //_WIN32

        return actualMemPtr;
    }

    // Release memory that was previously allocated using RequestVirtualMemory.
    static inline bool ReleaseVirtualMemory( void *memPtr, size_t memSize )
    {
        bool success = false;

#ifdef _WIN32
        // The documentation says that if we MEM_RELEASE things, the size
        // parameter must be 0.
        success = ( VirtualFree( memPtr, 0, MEM_RELEASE ) != FALSE );
#elif defined(__linux__)
        success = ( munmap( memPtr, memSize ) == 0 );
#endif //_WIN32

        FATAL_ASSERT( success == true );

        return success;
    }

    // Committing and decommitting memory.
    // This function makes the reserved memory actually usable in the program.
    static inline bool CommitVirtualMemory( void *mem_ptr, size_t mem_size )
    {
        bool success = false;

#ifdef _WIN32
        LPVOID commitHandle = VirtualAlloc( mem_ptr, mem_size, MEM_COMMIT, PAGE_READWRITE );

        success = ( commitHandle == mem_ptr );
#elif defined(__linux__)
        // We protect the page into accessibility.
        int error_out = mprotect( mem_ptr, mem_size, PROT_READ|PROT_WRITE );

        success = ( error_out == 0 );
#endif //_WIN32

        return success;
    }

    // Decommits the memory, making it unusable by the program.
    static inline bool DecommitVirtualMemory( void *mem_ptr, size_t mem_size )
    {
        bool success = false;

#ifdef _WIN32
        BOOL decommitSuccess = VirtualFree( mem_ptr, mem_size, MEM_DECOMMIT );

        success = ( decommitSuccess == TRUE );
#elif defined(__linux__)
        int error_out = mprotect( mem_ptr, mem_size, PROT_NONE );

        success = ( error_out == 0 );
#endif //_WIN32

        return success;
    }
};

#endif //_NATIVE_OS_VIRTUAL_MEMORY_FUNCS_
