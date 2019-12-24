#ifndef _ASMJITSHARED_HEADER_
#define _ASMJITSHARED_HEADER_

// Common asmjit tools that I do not want to rewrite all the time.
namespace asmjitshared
{

bool EmbedASMJITCodeIntoModule(
    PEFile& exeImage, bool requiresRelocations, const asmjit::CodeHolder& asmCodeHolder, const asmjit::Label& entryPointLabel,
    PEFile::PESectionDataReference& entryPointRefOut
);

};

#endif //_ASMJITSHARED_HEADER_