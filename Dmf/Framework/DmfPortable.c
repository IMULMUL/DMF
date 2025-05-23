/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPortable.c

Abstract:

    DMF Implementation:
    
    This Module contains support for the Portable (between Kernel and User-mode) APIs.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfPortable.tmh"
#endif

#define MAJOR_VERSION_WINDOWS_10  10
#define MINOR_VERSION_WINDOWS_10  0

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Portable_EventCreate(
    _Out_ DMF_PORTABLE_EVENT* EventPointer,
    _In_ EVENT_TYPE EventType,
    _In_ BOOLEAN InitialState
    )
/*++

Routine Description:

    Portable Function for the KMDF only KeInitializeEvent function.

Arguments:

    EventPointer - Pointer to Event Object Storage.
    EventType - Notification or Synchronization.
    InitialState - Initial State of the Event.

Return Value:

    NTSTATUS.

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DmfAssert(EventPointer != NULL);
    DmfAssert((EventType == SynchronizationEvent) ||
              (EventType == NotificationEvent));

    ntStatus = STATUS_SUCCESS;

#if defined(DMF_USER_MODE)
    BOOL manualReset;

    if (EventType == SynchronizationEvent)
    {
        manualReset = FALSE;
    }
    else if (EventType == NotificationEvent)
    {
        manualReset = TRUE;
    }
    else
    {
        DmfAssert(FALSE);
        manualReset = FALSE;
    }

    EventPointer->Handle = CreateEvent(NULL,
                                       manualReset,
                                       InitialState,
                                       NULL);

    if (EventPointer->Handle == NULL)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CreateEvent fails: %!STATUS!", ntStatus);
    }
#else
    KeInitializeEvent(&EventPointer->Handle,
                      EventType,
                      InitialState);
#endif // defined(DMF_USER_MODE)

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Portable_EventSet(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    )
/*++

Routine Description:

    Portable Function for the KMDF only KeSetEvent function.

Arguments:

    EventPointer - Pointer to Event Object Storage.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    DmfAssert(EventPointer != NULL);

#if defined(DMF_USER_MODE)
    SetEvent(EventPointer->Handle);
#else
    KeSetEvent(&EventPointer->Handle,
               0,
               FALSE);
#endif // defined(DMF_USER_MODE)

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Portable_EventReset(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    )
/*++

Routine Description:

    Portable Function for the KMDF only KeResetEvent function.

Arguments:

    EventPointer - Pointer to Event Object Storage.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    DmfAssert(EventPointer != NULL);

#if defined(DMF_USER_MODE)
    ResetEvent(EventPointer->Handle);
#else
    KeResetEvent(&EventPointer->Handle);
#endif // defined(DMF_USER_MODE)

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Portable_EventWaitForSingleObject(
    _In_ DMF_PORTABLE_EVENT* EventPointer,
    _In_opt_ ULONG* TimeoutMs,
    _In_ BOOLEAN Alertable
    )
/*++

Routine Description:

    Portable Function for the KMDF only KeWaitForSingleObject function.

Arguments:

    EventPointer - Pointer to Event Object Storage.
    TimeoutMs - Address of timeout value in milliseconds. NULL if caller waits infinitely.
    Alertable - Indicates if wait is alertable.

Return Value:

    NTSTATUS using Kernel-mode status values.

--*/
{
    NTSTATUS returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(EventPointer != NULL);

#if defined(DMF_USER_MODE)
    DWORD dwordReturnValue;
    ULONG timeoutMs;

    if (TimeoutMs != NULL)
    {
        timeoutMs = *TimeoutMs;
    }
    else
    {
        timeoutMs = INFINITE;
    }

    dwordReturnValue = WaitForSingleObjectEx(EventPointer->Handle,
                                             timeoutMs,
                                             Alertable);
    // Translate the Win32 values to Kernel-mode values.
    // NOTE: Actual numbers are the same, but this is done for clarity.
    //
    if (dwordReturnValue == WAIT_OBJECT_0)
    {
        returnValue = STATUS_SUCCESS;
    }
    else if (dwordReturnValue == WAIT_TIMEOUT)
    {
        returnValue = STATUS_TIMEOUT;
    }
    else if (dwordReturnValue == WAIT_ABANDONED)
    {
        returnValue = STATUS_ABANDONED;
    }
    else if (dwordReturnValue == WAIT_IO_COMPLETION)
    {
        returnValue = STATUS_ALERTED;
    }
    else
    {
        returnValue = STATUS_UNSUCCESSFUL;
    }
#else
    LARGE_INTEGER timeout100ns;
    LARGE_INTEGER* timeout100nsPointer;
    
    if (TimeoutMs != NULL)
    {
        // Caller waits for a maximum time in milliseconds.
        //
        timeout100ns.QuadPart = WDF_REL_TIMEOUT_IN_MS(*TimeoutMs);
        timeout100nsPointer = &timeout100ns;
    }
    else
    {
        // Passing NULL indicates "wait until event is set".
        //
        timeout100nsPointer = NULL;
    }

    returnValue = KeWaitForSingleObject(&EventPointer->Handle,
                                        Executive,
                                        KernelMode,
                                        Alertable,
                                        timeout100nsPointer);
#endif // defined(DMF_USER_MODE)

    FuncExit(DMF_TRACE, "returnValue=0x%X", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Portable_EventWaitForMultiple(
    _In_ ULONG EventCount,
    _In_reads_(EventCount) DMF_PORTABLE_EVENT** EventPointer,
    _In_ BOOLEAN WaitForAll,
    _In_opt_ ULONG* TimeoutMs,
    _In_ BOOLEAN Alertable
    )
/*++

Routine Description:

    Portable Function for the KMDF only KeWaitForSingleObject function.

Arguments:

    EventCount - Count of Event Objects.
    EventPointer - Pointer to Event Object Storage.
    WaitForAll - Indicating to 'wait for all' or 'wait for any'
    TimeoutMs - Address of timeout value in milliseconds. NULL if caller waits infinitely.
    Alertable - Indicates if wait is alertable.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(EventCount != 0);
    DmfAssert(EventPointer != NULL);
    DmfAssert(EventCount <= MAXIMUM_WAIT_OBJECTS);

    if (EventCount > MAXIMUM_WAIT_OBJECTS)
    {
        returnValue = STATUS_INVALID_PARAMETER;
        DmfAssert(FALSE);
        goto Exit;
    }

#if defined(DMF_USER_MODE)
    HANDLE waitHandles[MAXIMUM_WAIT_OBJECTS];

    for (ULONG eventIndex = 0; eventIndex < EventCount; ++eventIndex)
    {
        DmfAssert(EventPointer[eventIndex] != NULL);
        DmfAssert(EventPointer[eventIndex]->Handle != INVALID_HANDLE_VALUE);
        waitHandles[eventIndex] = EventPointer[eventIndex]->Handle;
    }

    DWORD dwordReturnValue;
    ULONG timeoutMs;

    if (TimeoutMs != NULL)
    {
        timeoutMs = *TimeoutMs;
    }
    else
    {
        timeoutMs = INFINITE;
    }

    dwordReturnValue = WaitForMultipleObjectsEx(EventCount,
                                                waitHandles,
                                                WaitForAll,
                                                timeoutMs,
                                                Alertable);
    if (dwordReturnValue == WAIT_TIMEOUT)
    {
        returnValue = STATUS_TIMEOUT;
    }
    else if (dwordReturnValue == WAIT_ABANDONED_0)
    {
        returnValue = STATUS_ABANDONED;
    }
    else if (dwordReturnValue == WAIT_IO_COMPLETION)
    {
        returnValue = STATUS_USER_APC;
    }
    else if (dwordReturnValue == WAIT_FAILED)
    {
        returnValue = STATUS_UNSUCCESSFUL;
    }
    else if (WaitForAll)
    {
        // NOTE: dwordReturnValue >= WAIT_OBJECT_0 is always TRUE.
        //
        if (dwordReturnValue < (WAIT_OBJECT_0 + EventCount))
        {
            returnValue = STATUS_SUCCESS;
        }
        else if ((dwordReturnValue >= WAIT_ABANDONED_0) &&
                 (dwordReturnValue < (WAIT_ABANDONED_0 + EventCount)))
        {
            returnValue = STATUS_ABANDONED;
        }
        else
        {
            returnValue = STATUS_UNSUCCESSFUL;
        }
    }
    else if (!WaitForAll)
    {
        // NOTE: dwordReturnValue >= WAIT_OBJECT_0 is always TRUE.
        //
        if (dwordReturnValue < (WAIT_OBJECT_0 + EventCount))
        {
            returnValue = STATUS_WAIT_0 + (dwordReturnValue - WAIT_OBJECT_0);
        }
        else if ((dwordReturnValue >= WAIT_ABANDONED_0) &&
                 (dwordReturnValue < (WAIT_ABANDONED_0 + EventCount)))
        {
            // NOTE: Only single status is available.
            //
            returnValue = STATUS_ABANDONED;
        }
        else
        {
            returnValue = STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        returnValue = STATUS_UNSUCCESSFUL;
    }
#else
    VOID* waitObjects[MAXIMUM_WAIT_OBJECTS];
    WAIT_TYPE waitType;

    for (ULONG eventIndex = 0; eventIndex < EventCount; ++eventIndex)
    {
        DmfAssert(EventPointer[eventIndex] != NULL);
        waitObjects[eventIndex] = &EventPointer[eventIndex]->Handle;
    }

    // We don't support other wait types like WaitNotification and WaitDequeue.
    //
    if (WaitForAll)
    {
        waitType = WaitAll;
    }
    else
    {
        waitType = WaitAny;
    }

    LARGE_INTEGER timeout100ns;
    LARGE_INTEGER* timeout100nsPointer;
    
    if (TimeoutMs != NULL)
    {
        // Caller waits for a maximum time in milliseconds.
        //
        timeout100ns.QuadPart = WDF_REL_TIMEOUT_IN_MS(*TimeoutMs);
        timeout100nsPointer = &timeout100ns;
    }
    else
    {
        // Passing NULL indicates "wait until event is set".
        //
        timeout100nsPointer = NULL;
    }

    returnValue = KeWaitForMultipleObjects(EventCount,
                                           waitObjects,
                                           waitType,
                                           Executive,
                                           KernelMode,
                                           Alertable,
                                           timeout100nsPointer,
                                           NULL);
#endif // defined(DMF_USER_MODE)

Exit:

    FuncExit(DMF_TRACE, "returnValue=0x%X", returnValue);

    return returnValue;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Portable_EventClose(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    )
/*++

Routine Description:

    Portable Function for the UMDF only CloseHandle function.

Arguments:

    EventPointer - Pointer to Event Object Storage.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    DmfAssert(EventPointer != NULL);

#if defined(DMF_USER_MODE)
    CloseHandle(EventPointer->Handle);
#else
    UNREFERENCED_PARAMETER(EventPointer);
#endif // defined(DMF_USER_MODE)

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Portable_LookasideListCreate(
    _In_ PWDF_OBJECT_ATTRIBUTES LookasideAttributes,
    _In_ size_t BufferSize,
    _In_ POOL_TYPE PoolType,
    _In_ PWDF_OBJECT_ATTRIBUTES MemoryAttributes,
    _In_ ULONG PoolTag,
    _Out_ DMF_PORTABLE_LOOKASIDELIST* LookasidePointer
    )
/*++

Routine Description:

    Portable Function for the KMDF only WdfLookasideListCreate function.

Arguments:

    LookasideAttributes - Pointer to Lookaside Object Attributes.
    BufferSize - Size of memory objects contained by the lookaside list.
    PoolType - Memory area to carve out the memory objects from.
    MemoryAttributes - Pointer to Memory Object attributes.
    PoolTag - Identifier for the Memory object.
    LookasidePointer - Storage area for the Lookaside list object.

Return Value:

    NTSTATUS from the KMDF/UMDF function.

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DmfAssert(LookasidePointer != NULL);

#if defined(DMF_USER_MODE)
    UNREFERENCED_PARAMETER(LookasideAttributes);

    ntStatus = STATUS_SUCCESS;

    if (MemoryAttributes)
    {
        RtlCopyMemory(&LookasidePointer->MemoryAttributes,
                      MemoryAttributes,
                      sizeof(WDF_OBJECT_ATTRIBUTES));
    }
    else
    {
        RtlZeroMemory(&LookasidePointer->MemoryAttributes,
                      sizeof(WDF_OBJECT_ATTRIBUTES));
    }
    LookasidePointer->BufferSize = BufferSize;
    LookasidePointer->PoolType = PoolType;
    LookasidePointer->PoolTag = PoolTag;
#else
    DmfAssert(BufferSize != 0);
    // 'Error annotation: __formal(1,BufferSize) cannot be zero.'
    //
    #pragma warning(suppress:28160)
    ntStatus= WdfLookasideListCreate(LookasideAttributes,
                                     BufferSize,
                                     PoolType,
                                     MemoryAttributes,
                                     PoolTag,
                                     &LookasidePointer->WdflookasideList);
#endif // defined(DMF_USER_MODE)

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#if defined(DMF_USER_MODE)
_IRQL_requires_max_(PASSIVE_LEVEL)
#else
_IRQL_requires_max_(DISPATCH_LEVEL)
#endif // defined(DMF_USER_MODE)
_Must_inspect_result_
NTSTATUS
DMF_Portable_LookasideListCreateMemory(
    _In_ DMF_PORTABLE_LOOKASIDELIST* LookasidePointer,
    _Out_ WDFMEMORY* Memory
    )
/*++

Routine Description:

    Portable Function for the KMDF only WDFMemoryCreateFromLookaside function.

Arguments:

    LookasidePointer - Storage area for the Lookaside list object.
    Memory - Pointer to a WDF memory Object.

Return Value:

    NTSTATUS from the KMDF/UMDF function.

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DmfAssert(LookasidePointer != NULL);

#if defined(DMF_USER_MODE)
    ntStatus = WdfMemoryCreate(&LookasidePointer->MemoryAttributes,
                               LookasidePointer->PoolType,
                               LookasidePointer->PoolTag,
                               LookasidePointer->BufferSize,
                               Memory,
                               NULL);
#else
    ntStatus = WdfMemoryCreateFromLookaside(LookasidePointer->WdflookasideList,
                                            Memory);
#endif // defined(DMF_USER_MODE)

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

VOID
DMF_Portable_Rundown_Initialize(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Initialize a given DMF_PORTABLE_RUNDOWN_REF structure.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    None

--*/
{
#if defined(DMF_KERNEL_MODE)
    ExInitializeRundownProtection(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
#endif // defined(DMF_KERNEL_MODE)
}

VOID
DMF_Portable_Rundown_Reinitialize(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Reinitialize a given DMF_PORTABLE_RUNDOWN_REF structure.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    None

--*/
{
#if defined(DMF_KERNEL_MODE)
    ExReInitializeRundownProtection(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
#endif // defined(DMF_KERNEL_MODE)
}

_Must_inspect_result_
BOOLEAN
DMF_Portable_Rundown_Acquire(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Increment the reference count in a given DMF_PORTABLE_RUNODOWN_REF structure
    if rundown has not yet started.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    TRUE if rundown down has not started and the increment occurred.

--*/
{
    BOOLEAN returnValue;

#if defined(DMF_KERNEL_MODE)
    returnValue = ExAcquireRundownProtection(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
    returnValue = FALSE;
#endif // defined(DMF_KERNEL_MODE)
    
    return returnValue;
}

VOID
DMF_Portable_Rundown_Release(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Decrement the reference count in a given DMF_PORTABLE_RUNODOWN_REF structure.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    None

--*/
{
#if defined(DMF_KERNEL_MODE)
    ExReleaseRundownProtection(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
#endif // defined(DMF_KERNEL_MODE)
}

VOID
DMF_Portable_Rundown_WaitForRundownProtectionRelease(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Wait for a given DMF_PORTABLE_RUNDOWN_REF structure's reference count to go to
    zero while preventing any more increments from happening.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    None

--*/
{
#if defined(DMF_KERNEL_MODE)
    ExWaitForRundownProtectionRelease(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
#endif // defined(DMF_KERNEL_MODE)
}

VOID
DMF_Portable_Rundown_Completed(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    )
/*++

Routine Description:

    Wait for a given DMF_PORTABLE_RUNDOWN_REF structure's reference count to go to
    zero while preventing any more increments from happening.

Arguments:

    RundownRef - The given DMF_PORTABLE_RUNDOWN_REF structure.

Return Value:

    None

--*/
{
#if defined(DMF_KERNEL_MODE)
    ExRundownCompleted(&RundownRef->RundownRef);
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    UNREFERENCED_PARAMETER(RundownRef);
    DmfAssert(FALSE);
#endif // defined(DMF_KERNEL_MODE)
}

_Must_inspect_result_
BOOLEAN
DMF_Portable_VersionCheck(
    _In_ ULONG MinimumOsVersion,
    _Out_ BOOLEAN *IsOsEqualOrGreater
    )
/*++

Routine Description:

    Read the Windows 10 OS version to check for feature availability.

Arguments:

    MinimumOsVersion - The minimum OS version to check for.
    IsOsEqualOrGreater - Flag to indicate if the running OS is greater
                         than or equal to the minimum passed in.

Return Value:

    TRUE if able to read the OS version.

--*/
{
    BOOLEAN returnStatus;
    static ULONG s_OsBuildNumber = 0;

    returnStatus = FALSE;
    *IsOsEqualOrGreater = FALSE;

    if (s_OsBuildNumber == 0)
    {
#if defined(DMF_KERNEL_MODE)
        NTSTATUS ntStatus;
        RTL_OSVERSIONINFOW versionInfo;

        versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
        ntStatus = RtlGetVersion(&versionInfo);
        if (NT_SUCCESS(ntStatus))
        {
            if (versionInfo.dwMajorVersion >= MAJOR_VERSION_WINDOWS_10 &&
                versionInfo.dwMinorVersion == MINOR_VERSION_WINDOWS_10)
            {
                s_OsBuildNumber = versionInfo.dwBuildNumber;
            }
        }
#else
        OSVERSIONINFOEXW versionInfo = { 0 };
        LONG status = 0;

        typedef LONG(WINAPI* PFN_RTL_GET_VERSION)(OSVERSIONINFOEXW*);
        PFN_RTL_GET_VERSION functionRtlGetVersion = NULL;
        
        versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);

        HMODULE ntDll = GetModuleHandleW(L"ntdll.dll");
        if (ntDll != NULL)
        {
            functionRtlGetVersion = (PFN_RTL_GET_VERSION)GetProcAddress(ntDll,
                                                                        "RtlGetVersion");
            if (functionRtlGetVersion)
            {
                status = functionRtlGetVersion(&versionInfo);
                if (status == S_OK)
                {
                    // Expecting major version 10 and minor version 0.
                    // Otherwise, unknown or unsupported os version.
                    //
                    if (versionInfo.dwMajorVersion >= MAJOR_VERSION_WINDOWS_10 &&
                        versionInfo.dwMinorVersion == MINOR_VERSION_WINDOWS_10)
                    {
                        // Everything worked, so store the os build number.
                        //
                        s_OsBuildNumber = versionInfo.dwBuildNumber;
                    }
                }
            }
        }
#endif // defined(DMF_KERNEL_MODE)
    }

    // 0 is an unsupported OS build version. It is overloaded here to
    // indicate an error in retrieving the OS version.
    //
    if (s_OsBuildNumber != 0)
    {
        returnStatus = TRUE;
        if (s_OsBuildNumber >= MinimumOsVersion)
        {
            *IsOsEqualOrGreater = TRUE;
        }
    }

    return returnStatus;
}

// eof: DmfPortable.c
//
