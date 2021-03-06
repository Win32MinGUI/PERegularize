#include <Windows.h>

/*
* Regularize PE32/PE32+ files to ->[my own favor]<- by:
* 
*   Setting FileHeader.TimeDateStamp to 0
*   Setting OptionalHeader.MajorLinkerVersion and OptionalHeader.MinorLinkerVersion to 0
*   Setting OptionalHeader.MajorOperatingSystemVersion to 4
*   Setting OptionalHeader.MinorOperatingSystemVersion to 0
*   Setting OptionalHeader.MajorSubsystemVersion to 4
*   Setting OptionalHeader.MinorSubsystemVersion to 0
*   Converting SizeOfRawData and PointerToRawData values in SectionHeaders to fit into FileAlignmentMask
*   Capticalizing DLL names in the Import Table and sort them by ascending order
*/

/* just an alternative to using sizeof(IMAGE_IMPORT_DESCRIPTOR) */
#ifndef IMAGE_SIZEOF_IMPORT_DESCRIPTOR
#define IMAGE_SIZEOF_IMPORT_DESCRIPTOR 0x14
#endif

#ifndef INVALID_OFFSET
#define INVALID_OFFSET 0
#endif

/* string comparison */
int scm(char *, char *);
/* memory copy */
void mcp(void *, void *, unsigned int);

VOID CapitalizeEntryName(PSTR);
DWORD RVAToFileOffset(DWORD, IMAGE_SECTION_HEADER *, DWORD);
BOOL RegularizePEFile(BYTE*);
VOID RegularizeSectionHeadersAndImportEntries(BYTE*, DWORD, DWORD, IMAGE_SECTION_HEADER*, DWORD, DWORD);

#ifdef _DEBUG
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
#else
VOID Win32MinGUIEntryPoint()
#endif
{
	PWSTR* sArgv;
	INT nArgs;

	sArgv = CommandLineToArgvW(GetCommandLine(), &nArgs);
	if (sArgv)
	{
		HANDLE hFile = CreateFileW(sArgv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD nFileSize = GetFileSize(hFile, NULL);
			if (nFileSize != 0 && nFileSize != 0xFFFFFFFF)
			{
				HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
				
				if (hFileMapping)
				{
					BYTE* pFile = MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

					if (pFile)
					{
						BOOL bFileModified = RegularizePEFile(pFile);

						if (bFileModified)
							FlushViewOfFile(pFile, 0);
						UnmapViewOfFile(pFile);
					}
					CloseHandle(hFileMapping);
				}
			}
			CloseHandle(hFile);
		}

		LocalFree(sArgv);
	}

#ifdef _DEBUG
	return 0;
#else
	ExitProcess(0);
#endif
}
int scm(char *s1, char *s2)
{
	for (;; ++s1, ++s2)
		if (*s1 != *s2)
			return *s1 - *s2;
		else if (!*s1)
			return 0;
}
void mcp(void *dst, void *src, unsigned int sz)
{
	char *d = dst;
	char *s = src;
	unsigned int i;
	
	for (i = 0; i < sz; ++i)
		*d++ = *s++;
}
VOID CapitalizeEntryName(PSTR pS) // 将首字母大写，其余字母小写
{
	if (*pS >= 'a' && *pS <= 'z')
		*pS &= 0xDF;
	++pS;
	for (; *pS; ++pS)
		if (*pS >= 'A' && *pS <= 'Z')
			*pS |= 0x20;
}
DWORD RVAToFileOffset(DWORD RVA, IMAGE_SECTION_HEADER* pScnHd, DWORD NumberOfSections)
{
	if (RVA >0 && RVA < pScnHd[0].VirtualAddress)
		return RVA;
	else
	{
		DWORD i;
		for (i = 0; i < NumberOfSections; ++i)
			if (RVA >= pScnHd[i].VirtualAddress && RVA < pScnHd[i].VirtualAddress + pScnHd[i].Misc.VirtualSize)
				return RVA - (pScnHd[i].VirtualAddress - pScnHd[i].PointerToRawData);
		return INVALID_OFFSET;
	}
}

BOOL IsPE32(BYTE* pPE)
{
	return ((IMAGE_NT_HEADERS32*)(pPE + ((IMAGE_DOS_HEADER*)pPE)->e_lfanew))->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
		((IMAGE_NT_HEADERS32*)(pPE + ((IMAGE_DOS_HEADER*)pPE)->e_lfanew))->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
}
BOOL IsPE64(BYTE* pPE)
{
	return ((IMAGE_NT_HEADERS64*)(pPE + ((IMAGE_DOS_HEADER*)pPE)->e_lfanew))->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
		((IMAGE_NT_HEADERS64*)(pPE + ((IMAGE_DOS_HEADER*)pPE)->e_lfanew))->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
}

BOOL RegularizePEFile(BYTE* pPE)
{
	if (IsPE32(pPE))
	{
		IMAGE_DOS_HEADER* pDosHd;
		IMAGE_NT_HEADERS32* pNtHds;
		IMAGE_SECTION_HEADER* pScnHd;

		pDosHd = (IMAGE_DOS_HEADER*)pPE;
		pNtHds = (IMAGE_NT_HEADERS32*)(pPE + pDosHd->e_lfanew);
		pScnHd = (IMAGE_SECTION_HEADER*)((BYTE*)(pNtHds + 1) + pNtHds->FileHeader.SizeOfOptionalHeader - sizeof(IMAGE_OPTIONAL_HEADER32));

		pNtHds->FileHeader.TimeDateStamp = 0;
		pNtHds->OptionalHeader.MajorLinkerVersion = 0;
		pNtHds->OptionalHeader.MinorLinkerVersion = 0;
		pNtHds->OptionalHeader.MajorOperatingSystemVersion = 4;
		pNtHds->OptionalHeader.MinorOperatingSystemVersion = 0;
		pNtHds->OptionalHeader.MajorSubsystemVersion = 4;
		pNtHds->OptionalHeader.MinorSubsystemVersion = 0;

		RegularizeSectionHeadersAndImportEntries(pPE, pNtHds->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress,
			pNtHds->FileHeader.NumberOfSections, pScnHd, pNtHds->OptionalHeader.SectionAlignment, pNtHds->OptionalHeader.FileAlignment);

		return TRUE;
	}
	else if (IsPE64(pPE))
	{
		IMAGE_DOS_HEADER* pDosHd;
		IMAGE_NT_HEADERS64* pNtHds;
		IMAGE_SECTION_HEADER* pScnHd;

		pDosHd = (IMAGE_DOS_HEADER*)pPE;
		pNtHds = (IMAGE_NT_HEADERS64*)(pPE + pDosHd->e_lfanew);
		pScnHd = (IMAGE_SECTION_HEADER*)((BYTE*)(pNtHds + 1) + pNtHds->FileHeader.SizeOfOptionalHeader - sizeof(IMAGE_OPTIONAL_HEADER64));

		pNtHds->FileHeader.TimeDateStamp = 0;
		pNtHds->OptionalHeader.MajorLinkerVersion = 0;
		pNtHds->OptionalHeader.MinorLinkerVersion = 0;
		pNtHds->OptionalHeader.MajorOperatingSystemVersion = 4;
		pNtHds->OptionalHeader.MinorOperatingSystemVersion = 0;
		pNtHds->OptionalHeader.MajorSubsystemVersion = 4;
		pNtHds->OptionalHeader.MinorSubsystemVersion = 0;


		RegularizeSectionHeadersAndImportEntries(pPE, pNtHds->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress,
			pNtHds->FileHeader.NumberOfSections, pScnHd, pNtHds->OptionalHeader.SectionAlignment, pNtHds->OptionalHeader.FileAlignment);

		return TRUE;
	}

	return FALSE;
}
VOID RegularizeSectionHeadersAndImportEntries(BYTE* pPE, DWORD nImportDirectoryVA, DWORD nNumberOfSections, IMAGE_SECTION_HEADER* pScnHd, DWORD nSectionAlignment, DWORD nFileAlignment)
{
	DWORD i;
	DWORD nOffset;
	DWORD nFileAlignmentMask = nFileAlignment - 1;

	// update SizeOfRawData and PointerToRawData if they are not regulated
	for (i = 0; i < nNumberOfSections; ++i)
	{
		if (pScnHd[i].SizeOfRawData && (pScnHd[i].VirtualAddress % nSectionAlignment))
		{
			pScnHd[i].SizeOfRawData = (pScnHd[i].SizeOfRawData + nFileAlignmentMask) & (0xFFFFFFFF ^ nFileAlignmentMask);
			pScnHd[i].PointerToRawData = pScnHd[i].PointerToRawData & (0xFFFFFFFF ^ nFileAlignmentMask);
		}
	}

	if (nImportDirectoryVA)
	{
		nOffset = RVAToFileOffset(nImportDirectoryVA, pScnHd, nNumberOfSections);
		if (nOffset != INVALID_OFFSET)
		{
			IMAGE_IMPORT_DESCRIPTOR* pImpD;
			DWORD nImpD; // 总的 ImportDirEntry 数目「不包含末尾 0 Entry 」 = pNtHds->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / IMAGE_SIZEOF_IMPORT_DESCRIPTOR - 1;

			pImpD = (IMAGE_IMPORT_DESCRIPTOR*)(pPE + nOffset);

			for (i = 0; i < 256; ++i)
				if (pImpD[i].OriginalFirstThunk == 0 && pImpD[i].TimeDateStamp == 0 && pImpD[i].ForwarderChain == 0 && pImpD[i].Name == 0 && pImpD[i].FirstThunk == 0)
					break;
			nImpD = i;

			for (i = 0; i < nImpD; ++i)
			{
				nOffset = RVAToFileOffset(pImpD[i].Name, pScnHd, nNumberOfSections);
				if (nOffset != INVALID_OFFSET)
					CapitalizeEntryName(pPE + nOffset);
			}

			if (nImpD > 1)
			{
				/*
				将 IMAGE_IMPORT_DESCRIPTOR 里面各个 Entry 排升序

				[0] OFT/Characteristics  TimeDateStamp  ForwarderChain  User32.dll    FirstThunk		Idx[0]=1 OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk
				[1] OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk		Idx[1]=0 OFT/Characteristics  TimeDateStamp  ForwarderChain  User32.dll    FirstThunk
				[2] NULL                 NULL           NULL            NULL          NULL

				Idx[i] 是比 [i] Name 小的 Entry 总个数


				假设通过以下行为变更後
				KERNEL32.DLL -> Kernel32.dll
				每个 entry 的 DLL 名是唯一的
				即 Import Directory Table 中不会有两个 KERNEL32.DLL

				假设每个 Name RVA 都是有效的
				*/
				HANDLE hHeap;

				IMAGE_IMPORT_DESCRIPTOR* pImpDX; // copy of IMAGE_IMPORT_DESCRIPTOR * nImpD
				BYTE** pName;
				DWORD* Idx;
				DWORD k;

				hHeap = HeapCreate(0, 0, 0);
				pImpDX = HeapAlloc(hHeap, 0, nImpD * IMAGE_SIZEOF_IMPORT_DESCRIPTOR);
				pName = HeapAlloc(hHeap, 0, nImpD << 2);
				Idx = HeapAlloc(hHeap, 0, nImpD << 2);

				mcp(pImpDX, pImpD, nImpD * IMAGE_SIZEOF_IMPORT_DESCRIPTOR);

				for (i = 0; i < nImpD; ++i)
					pName[i] = pPE + RVAToFileOffset(pImpD[i].Name, pScnHd, nNumberOfSections);

				for (i = 0; i < nImpD; ++i)
				{
					Idx[i] = 0;
					for (k = 0; k < nImpD; ++k)
						if (i != k)
							if (scm(pName[k], pName[i]) < 0)
								++Idx[i];
				}
				/*
				Fix duplicate indices
				[0] OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk	Idx[0]=0	->	OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk
				[1] OFT/Characteristics  TimeDateStamp  ForwarderChain  User32.dll    FirstThunk	Idx[1]=2	->	OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk
				[2] OFT/Characteristics  TimeDateStamp  ForwarderChain  Kernel32.dll  FirstThunk	Idx[2]=0+1=1->	OFT/Characteristics  TimeDateStamp  ForwarderChain  User32.dll    FirstThunk
				[3] NULL                 NULL           NULL            NULL          NULL			i = 2, k = 0 ++Idx[2]
				*/
				for (i = 0; i < nImpD; ++i)
					for (k = 0; k < nImpD; ++k)
						if (i != k)
							if (scm(pName[k], pName[i]) == 0)
								if (i > k)
									++Idx[i];

				for (i = 0; i < nImpD; ++i)
					mcp(pImpD + Idx[i], pImpDX + i, IMAGE_SIZEOF_IMPORT_DESCRIPTOR);

				HeapDestroy(hHeap);
			}
		}
	}
}