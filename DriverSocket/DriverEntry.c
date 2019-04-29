#include <ntddk.h>
#include "KernelSearch.h"

#define SOCKET_IOCTL CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define DEVICE_NAME L"\\Device\\SocketIoDevice"
#define SYM_LINK_NAME L"\\??\\SocketSYM"

PDEVICE_OBJECT pDevice;
UNICODE_STRING DeviceName;
UNICODE_STRING SymLinkName;

NTSTATUS IoctlDevice(PDEVICE_OBJECT pDeviceObject, PIRP pIrp) {
	NTSTATUS status;
	ULONG_PTR informaiton = 0;
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG IoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	PVOID InputData = pIrp->AssociatedIrp.SystemBuffer;
	PVOID OutputData = pIrp->AssociatedIrp.SystemBuffer;
	ULONG InputDataLength = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputDataLength = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
	switch (IoControlCode)
	{
	case SOCKET_IOCTL:
		InputData = pIrp->AssociatedIrp.SystemBuffer;
		DbgPrint("%s\r\n", InputData);
		GetModuleBase(InputData);
		status = STATUS_SUCCESS;
		break;
	default:
		status = STATUS_UNSUCCESSFUL;
	}
	pIrp->IoStatus.Status = status;//Ring3 GetLastError();
	pIrp->IoStatus.Information = 0;//返回ReturnLength sizeof(cls) , 如果是string类型可以strlen
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);// I/O请求处理完毕
	return STATUS_SUCCESS;
}

// 设备共用派遣函数
NTSTATUS DeviceApi(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;//Ring3 GetLastError();
	pIrp->IoStatus.Information = 0;//返回ReturnLength
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);// I/O请求处理完毕
	return STATUS_SUCCESS;
}

NTSTATUS DriverUnLoad(PDRIVER_OBJECT pDriverObject)
{
	IoDeleteSymbolicLink(&SymLinkName);
	while (pDevice != NULL)
	{
		pDevice = pDevice->NextDevice;
		IoDeleteDevice(pDevice);
	}
	return STATUS_SUCCESS;
}

NTSTATUS InitSocket(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING usRegistPath) {
	NTSTATUS status = STATUS_SUCCESS;
	RtlInitUnicodeString(&DeviceName, DEVICE_NAME);
	RtlInitUnicodeString(&SymLinkName, SYM_LINK_NAME);
	// 通过循环将设备创建、读写、关闭等函数设置为通用的DeviceApi
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		pDriverObject->MajorFunction[i] = DeviceApi;
	}
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoctlDevice;
	//创建设备
	status = IoCreateDevice(pDriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevice);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Create Device Faild!\n");
		IoDeleteDevice(pDevice);
		status = STATUS_UNSUCCESSFUL;
	}
	//将设备连接名称与设备名称关联
	status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Create SymLink Faild!\n");
		IoDeleteDevice(pDevice);
		status = STATUS_UNSUCCESSFUL;
	}
	// 设置pDevice以缓冲区方式读取
	pDevice->Flags = DO_BUFFERED_IO;
	return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING usRegistPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	pDriverObject->DriverUnload = DriverUnLoad;

	status = InitSocket(pDriverObject, usRegistPath);
	
	DbgPrint("Initialize Success\n");
	return status;
}