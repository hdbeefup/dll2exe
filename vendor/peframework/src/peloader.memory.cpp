// Memory management header of peframework.
// We must not initialize any static memory here but we can redirect to third-party libraries.
#include "peloader.h"

#ifdef PEFRAMEWORK_NATIVE_EXECUTIVE
#include <NativeExecutive/CExecutiveManager.h>
#endif //PEFRAMEWORK_NATIVE_EXECUTIVE

void* PEGlobalStaticAllocator::Allocate( void *refPtr, size_t memSize, size_t alignment )
{
#ifdef PEFRAMEWORK_NATIVE_EXECUTIVE
    return NatExecGlobalStaticAlloc::Allocate( refPtr, memSize, alignment );
#else
    return CRTHeapAllocator::Allocate( refPtr, memSize, alignment );
#endif //PEFRAMEWORK_NATIVE_EXECUTIVE
}

bool PEGlobalStaticAllocator::Resize( void *refPtr, void *memPtr, size_t memSize )
{
#ifdef PEFRAMEWORK_NATIVE_EXECUTIVE
    return NatExecGlobalStaticAlloc::Resize( refPtr, memPtr, memSize );
#else
    return CRTHeapAllocator::Resize( refPtr, memPtr, memSize );
#endif //PEFRAMEWORK_NATIVE_EXECUTIVE
}

void PEGlobalStaticAllocator::Free( void *refPtr, void *memPtr )
{
#ifdef PEFRAMEWORK_NATIVE_EXECUTIVE
    NatExecGlobalStaticAlloc::Free( refPtr, memPtr );
#else
    CRTHeapAllocator::Free( refPtr, memPtr );
#endif //PEFRAMRWORK_NATIVE_EXECUTIVE
}