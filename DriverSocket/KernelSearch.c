#include "KernelSearch.h"

PVOID g_KernelBase = NULL;
ULONG g_KernelSize = 0;
PSYSTEM_SERVICE_DESCRIPTOR_TABLE g_SSDT = NULL;

/// <summary>
/// Get ntoskrnl base address
/// </summary>
/// <param name="pSize">Size of module</param>
/// <returns>Found address, NULL if not found</returns>
PVOID GetKernelBase(OUT PULONG pSize)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bytes = 0;
	PRTL_PROCESS_MODULES pMods = NULL;
	PVOID checkPtr = NULL;
	UNICODE_STRING routineName;

	// Already found
	if (g_KernelBase != NULL)
	{
		if (pSize)
			*pSize = g_KernelSize;
		return g_KernelBase;
	}

	RtlInitUnicodeString(&routineName, L"NtOpenFile");

	checkPtr = MmGetSystemRoutineAddress(&routineName);
	if (checkPtr == NULL)
		return NULL;

	// Protect from UserMode AV
	status = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);
	if (bytes == 0)
	{
		DbgPrint("error: %s: Invalid SystemModuleInformation size\n", __FUNCTION__);
		return NULL;
	}

	pMods = (PRTL_PROCESS_MODULES)ExAllocatePool(NonPagedPool, bytes);
	RtlZeroMemory(pMods, bytes);

	status = ZwQuerySystemInformation(SystemModuleInformation, pMods, bytes, &bytes);

	if (NT_SUCCESS(status))
	{
		PRTL_PROCESS_MODULE_INFORMATION pMod = pMods->Modules;

		for (ULONG i = 0; i < pMods->NumberOfModules; i++)
		{
			// System routine is inside module
			if (checkPtr >= pMod[i].ImageBase &&
				checkPtr < (PVOID)((PUCHAR)pMod[i].ImageBase + pMod[i].ImageSize))
			{
				g_KernelBase = pMod[i].ImageBase;
				g_KernelSize = pMod[i].ImageSize;
				if (pSize)
					*pSize = g_KernelSize;
				break;
			}
		}
	}

	if (pMods)
		ExFreePool(pMods);

	return g_KernelBase;
}

// 获取模块基址
PVOID GetModuleBase(CHAR *ModuleName)
{
	//参数效验
	if (ModuleName == NULL)return NULL;


	//定义变量
	ULONG i = 0;
	RTL_PROCESS_MODULES *ProcessModules = NULL;
	PVOID ImageBase = NULL;
	ULONG ReturnLength = 0;
	RTL_PROCESS_MODULE_INFORMATION *ModuleInformation = NULL;

	ZwQuerySystemInformation(SystemModuleInformation, &ProcessModules, 4, &ReturnLength);
	_asm int 3;
	if (ReturnLength)
	{
		ProcessModules = (PRTL_PROCESS_MODULES)ExAllocatePool((POOL_TYPE)SystemModuleInformation, 2 * ReturnLength);
		if (ProcessModules)
		{
			if (NT_SUCCESS(ZwQuerySystemInformation(SystemModuleInformation, ProcessModules, 2 * ReturnLength, NULL)))
			{
				ModuleInformation = (RTL_PROCESS_MODULE_INFORMATION *)(ProcessModules->Modules);
				for (i = 0; i < ProcessModules->NumberOfModules; i++)
				{
					//KdPrint(("%s\n", (CHAR*)&ModuleInformation[i].FullPathName[ModuleInformation[i].OffsetToFileName]));
					if (!_stricmp(ModuleName, (CHAR*)&ModuleInformation[i].FullPathName[ModuleInformation[i].OffsetToFileName]))
					{
						ImageBase = ModuleInformation[i].ImageBase;
						break;
					}
					KdPrint(("%s\t %0x\t %0x\t %s  \n",
						&ModuleInformation[i].FullPathName[ModuleInformation[i].OffsetToFileName],
						ModuleInformation[i].ImageBase,
						ModuleInformation[i].ImageSize,
						ModuleInformation[i].FullPathName));
				}
			}
			ExFreePool(ProcessModules);
		}
	}

	return ImageBase;
}