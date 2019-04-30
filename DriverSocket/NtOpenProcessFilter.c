#include <ntifs.h>
#include <ntddk.h>

// 内核之SSDT - HOOK
//系统服务表
typedef struct _KSYSTEM_SERVICE_TABLE
{
	PULONG ServiceTableBase;       //函数地址表的首地址
	PULONG ServiceCounterTableBase;//函数表中每个函数被调用的次数
	ULONG  NumberOfService;        //服务函数的个数
	ULONG ParamTableBase;          //参数个数表首地址
}KSYSTEM_SERVICE_TABLE;

//服务描述符
typedef struct _KSERVICE_TABLE_DESCRIPTOR
{
	KSYSTEM_SERVICE_TABLE ntoskrnl;//ntoskrnl.exe的服务函数,SSDT
	KSYSTEM_SERVICE_TABLE win32k;  //win32k.sys的服务函数,ShadowSSDT
	KSYSTEM_SERVICE_TABLE notUsed1;//暂时没用1
	KSYSTEM_SERVICE_TABLE notUsed2;//暂时没用2
}KSERVICE_TABLE_DESCRIPTOR;

//定义HOOK的函数的类型
typedef NTSTATUS(NTAPI*FuZwOpenProcess)(
	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
	);

//自写的函数声明
NTSTATUS NTAPI MyZwOpenProcess(
	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
);

//记录系统的该函数
FuZwOpenProcess g_OldZwOpenProcess;
//服务描述符表指针
KSERVICE_TABLE_DESCRIPTOR* g_pServiceTable = NULL;
//要保护进程的ID
ULONG g_Pid = 9527;

//安装钩子
void InstallHook();
//卸载钩子
void UninstallHook();
//关闭页写入保护
void WPOFF();
//开启页写入保护
void WPON();

//卸载驱动
void OutLoad(DRIVER_OBJECT* obj)
{
	obj;
	//卸载钩子
	UninstallHook();
}

//安装钩子
void InstallHook()
{
	//1.获取KTHREAD
	PETHREAD pNowThread = PsGetCurrentThread();
	//2.获取ServiceTable表
	g_pServiceTable = (KSERVICE_TABLE_DESCRIPTOR*)(*(ULONG*)((ULONG)pNowThread + 0xbc));
	//3.保存旧的函数
	g_OldZwOpenProcess = (FuZwOpenProcess)
	g_pServiceTable->ntoskrnl.ServiceTableBase[0xbe];
	//4.关闭页只读保护
	WPOFF();
	//5.写入自己的函数到SSDT表内
	g_pServiceTable->ntoskrnl.ServiceTableBase[0xbe] = (ULONG)MyZwOpenProcess;
	//6.开启页只读保护
	WPON();
}

//卸载钩子
void UninstallHook()
{
	//1.关闭页只读保护
	WPOFF();
	//2.写入原来的函数到SSDT表内
	g_pServiceTable->ntoskrnl.ServiceTableBase[0xbe]
		= (ULONG)g_OldZwOpenProcess;
	//3.开启页只读保护
	WPON();
}


//关闭页只读保护
void _declspec(naked) WPOFF()
{
	__asm
	{
		push eax;
		mov eax, cr0;
		and eax, ~0x10000;
		mov cr0, eax;
		pop eax;
		ret;
	}
}

//开启页只读保护
void _declspec(naked) WPON()
{
	__asm
	{
		push eax;
		mov eax, cr0;
		or eax, 0x10000;
		mov cr0, eax;
		pop eax;
		ret;
	}
}

//自写的函数
NTSTATUS NTAPI MyZwOpenProcess(
	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
)
{
	#define PROCESS_TERMINATE 0x1
	//当此进程为要保护的进程时
	if (ClientId->UniqueProcess == (HANDLE)g_Pid && DesiredAccess == PROCESS_TERMINATE)
	{
		//设为拒绝访问
		DesiredAccess = 0;
	}
	//调用原函数
	return g_OldZwOpenProcess(
		ProcessHandle,
		DesiredAccess,
		ObjectAttributes,
		ClientId);
}