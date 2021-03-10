#pragma once

struct _crtMemState;

#define MEMALLOC_VERSION 1

typedef size_t(*MemAllocFailHandler_t) (size_t);

class MemAlloc
{
public:
    virtual ~MemAlloc();

    virtual void* alloc(size_t nSize) = 0;
    virtual void* realloc(void* pMem, size_t nSize) = 0;
    virtual void free(void* pMem) = 0;
    virtual void* expandNoLongerSupported(void* pMem, size_t nSize) = 0;

    virtual size_t getSize(void* pMem) = 0;

    virtual void PushAllocDbgInfo(const char* pFileName, int nLine) = 0;
    virtual void PopAllocDbgInfo() = 0;

    virtual long crtSetBreakAlloc(long lNewBreakAlloc) = 0;
    virtual int crtSetReportMode(int nReportType, int nReportMode) = 0;
    virtual int crtIsValidHeapPointer(const void* pMem) = 0;
    virtual int crtIsValidPointer(const void* pMem, unsigned int size, int access) = 0;
    virtual int crtCheckMemory(void) = 0;
    virtual int crtSetDbgFlag(int nNewFlag) = 0;
    virtual void crtMemCheckpoint(_crtMemState* pState) = 0;

    virtual void dumpStats() = 0;
    virtual void dumpStatsFileBase(char const* pchFileBase) = 0;

    virtual void* crtSetReportFile(int nRptType, void* hFile) = 0;
    virtual void* crtSetReportHook(void* pfnNewHook) = 0;
    virtual int crtDbgReport(int nRptType, const char* szFile,
        int nLine, const char* szModule, const char* pMsg) = 0;

    virtual int heapChk() = 0;

    virtual bool isDebugHeap() = 0;

    virtual void getActualDbgInfo(const char*& pFileName, int& nLine) = 0;
    virtual void registerAllocation(const char* pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime) = 0;
    virtual void registerDeallocation(const char* pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime) = 0;

    virtual int getVersion() = 0;

    virtual void CompactHeap() = 0;

    virtual MemAllocFailHandler_t SetAllocFailHandler(MemAllocFailHandler_t pfnMemAllocFailHandler) = 0;

    virtual void dumpBlockStats(void*) = 0;

#if defined( _MEMTEST )
    virtual void SetStatsExtraInfo(const char* pMapName, const char* pComment) = 0;
#endif

    virtual size_t MemoryAllocFailed() = 0;

    virtual int getDebugInfoSize() = 0;
    virtual void saveDebugInfo(void* pvDebugInfo) = 0;
    virtual void restoreDebugInfo(const void* pvDebugInfo) = 0;
    virtual void initDebugInfo(void* pvDebugInfo, const char* pchRootFileName, int nLine) = 0;

    virtual void globalMemoryStatus(size_t* pUsedMemory, size_t* pFreeMemory) = 0;
};