#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

// The unique code to talk to your driver
#define IOCTL_COMMAND_HANDLER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef enum _COMMAND_TYPE {
    COMMAND_NONE = 0,
    COMMAND_READ_MEMORY,
    COMMAND_WRITE_MEMORY,
    COMMAND_GET_BASE_ADDRESS
} COMMAND_TYPE;

// This is your unified request struct
typedef struct _KERNEL_COMMAND_REQUEST {
    unsigned long ProcessId;
    unsigned __int64 Address;
    void* Buffer;
    unsigned __int64 Size;
    COMMAND_TYPE Command; // Tells the driver WHAT to do
} KERNEL_COMMAND_REQUEST, * PKERNEL_COMMAND_REQUEST;