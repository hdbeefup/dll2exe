// MIPS 32bit implementation of function registry entries.

#ifndef _PELOADER_FUNCTIONS_REGISTRY_MIPS_
#define _PELOADER_FUNCTIONS_REGISTRY_MIPS_

#include "peloader.h"

namespace PEFileDetails
{

struct PERuntimeFunctionMIPS
{
    PEFile::PESectionDataReference begAddr;
    PEFile::PESectionDataReference endAddr;
    PEFile::PESectionDataReference exceptHandlerAddr;
    PEFile::PESectionDataReference userDataAddr;
    PEFile::PESectionDataReference endOfPrologAddr;
};

// TODO: add the function registry and stuff.

}

#endif //_PELOADER_FUNCTIONS_REGISTRY_MIPS_