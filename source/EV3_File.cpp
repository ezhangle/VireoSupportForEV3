/*
 * Copyright (c) 2013-2014 National Instruments Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/stat.h>

#include "TypeAndDataManager.h"
#include "StringUtilities.h"

extern "C" {
#include "c_memory.h"
DSPSTAT cMemoryFreeHandle(PRGID PrgId, HANDLER Handle);
}

using namespace Vireo;

VIREO_FUNCTION_SIGNATURE3(FileOpenRead, StringRef, Int16, Int32)
{
    TempStackCStringFromString fileName(_Param(0));
    Int16 handle;
    Int32 size;

    DSPSTAT DspStat = BUSYBREAK;
    PRGID PrgId = CurrentProgramId();
    char FilenameBuf[vmFILENAMESIZE];
    if (ConstructFilename(PrgId, fileName.BeginCStr(), FilenameBuf, "") == OK)
    {
        DspStat = cMemoryOpenFile(PrgId, OPEN_FOR_READ, FilenameBuf, &handle, &size);
    }

    if (_ParamPointer(1))
        _Param(1) = handle;
    if (_ParamPointer(2))
        _Param(2) = size;

    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(FileOpenWrite, StringRef, Int16)
{
    TempStackCStringFromString fileName(_Param(0));
    Int16 handle;

    DSPSTAT DspStat = BUSYBREAK;
    PRGID PrgId = CurrentProgramId();
    char FilenameBuf[vmFILENAMESIZE];
    Int32 ISize;
    if (ConstructFilename(PrgId, fileName.BeginCStr(), FilenameBuf, "") == OK)
    {
        DspStat = cMemoryOpenFile(PrgId, OPEN_FOR_WRITE, FilenameBuf, &handle, &ISize);
    }

    if (_ParamPointer(1))
        _Param(1) = handle;

    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(FileOpenAppend, StringRef, Int16)
{
    TempStackCStringFromString fileName(_Param(0));
    Int16 handle;

    DSPSTAT DspStat = BUSYBREAK;
    PRGID PrgId = CurrentProgramId();
    char FilenameBuf[vmFILENAMESIZE];
    Int32 ISize;
    if (ConstructFilename(PrgId, fileName.BeginCStr(), FilenameBuf, "") == OK)
    {
        DspStat = cMemoryOpenFile(PrgId, OPEN_FOR_APPEND, FilenameBuf, &handle, &ISize);
    }

    if (_ParamPointer(1))
        _Param(1) = handle;

    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE1(FileClose, Int16)
{
    Int16 handle = _Param(0);

    DSPSTAT DspStat = cMemoryCloseFile(CurrentProgramId(), handle);
    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE3(FileRead, Int16, Int32, TypedArrayCoreRef)
{
    Int16 handle = _Param(0);
    Int32 length = _Param(1);
    TypedArrayCoreRef data;
    if (_ParamPointer(2))
        data = _Param(2);
    else
        return _NextInstruction();

    PRGID PrgId = CurrentProgramId();
    Int32 ISize;
    DESCR *pTmp;
    HANDLER arrayHandle;
    DSPSTAT DspStat;

    // Create a byte array to hold the data
    if (length < MIN_ARRAY_ELEMENTS)
        length = MIN_ARRAY_ELEMENTS;
    ISize = length * sizeof(DATA8) + sizeof(DESCR);
    if (cMemoryAlloc(PrgId, POOL_TYPE_MEMORY, (GBINDEX) ISize, (void**) &pTmp, &arrayHandle) == OK)
    {
        pTmp->Type        = DATA_8;
        pTmp->ElementSize = (DATA8)sizeof(DATA8);
        pTmp->Elements    = length;

        // Read the file into the array
        DspStat = cMemoryReadFile(PrgId, handle, length, DEL_NONE, pTmp->pArray);

        if (DspStat != FAILBREAK)
        {
            // If the array is not big enough, resize it
            if (data->Length() < length)
                data->Resize1D(length);
            // Copy into the output data array
            for (Int32 i = 0; i < length; i++)
                *data->BeginAt(i) = pTmp->pArray[i];

            cMemoryFreeHandle(PrgId, arrayHandle);
        }
    }
    else
        DspStat = FAILBREAK;

    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(FileWrite, Int16, TypedArrayCoreRef)
{
    Int16 handle = _Param(0);
    TypedArrayCoreRef data = _Param(1);
    Int32 length = data->Length();

    PRGID PrgId = CurrentProgramId();
    Int32 ISize;
    DESCR *pTmp;
    HANDLER arrayHandle;
    DSPSTAT DspStat;

    // Create a byte array to hold the data
    if (length < MIN_ARRAY_ELEMENTS)
        length = MIN_ARRAY_ELEMENTS;
    ISize = length * sizeof(DATA8) + sizeof(DESCR);
    if (cMemoryAlloc(PrgId, POOL_TYPE_MEMORY, (GBINDEX) ISize, (void**) &pTmp, &arrayHandle) == OK)
    {
        pTmp->Type        = DATA_8;
        pTmp->ElementSize = (DATA8)sizeof(DATA8);
        pTmp->Elements    = length;

        // Copy from the data array passed as an argument
        for (Int32 i = 0; i < length; i++)
            pTmp->pArray[i] = *data->BeginAt(i);

        // Write the array to the file
        DspStat = cMemoryWriteFile(PrgId, handle, length, DEL_NONE, pTmp->pArray);

        cMemoryFreeHandle(PrgId, arrayHandle);
    }
    else
        DspStat = FAILBREAK;

    SetDispatchStatus(DspStat);

    return DspStat == BUSYBREAK ? _this : _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE1(FileRemove, StringRef)
{
    TempStackCStringFromString fileName(_Param(0));

    PRGID PrgId = CurrentProgramId();
    char FilenameBuf[vmFILENAMESIZE];

    if (ConstructFilename(PrgId, fileName.BeginCStr(), FilenameBuf,"") == OK)
    {
        cMemoryDeleteSubFolders(FilenameBuf);
        SetUiUpdate();
    }

    return _NextInstruction();
}

// Checks whether a given file is currently open.
// If the file is not open, handle is -1 and write is 0.
// Otherwise, handle is the file's handle number and write is 0 if the file is
// open for read or 1 if the file is open for write/append.
VIREO_FUNCTION_SIGNATURE3(FileResolveHandle, StringRef, Int16, Int8)
{
    TempStackCStringFromString fileName(_Param(0));
    Int16 handle;
    Int8  write;

    PRGID PrgId = CurrentProgramId();
    char FilenameBuf[vmFILENAMESIZE];
    DSPSTAT DspStat;

    if (ConstructFilename(PrgId, fileName.BeginCStr(), FilenameBuf, "") == OK)
        DspStat = cMemoryGetFileHandle(PrgId, FilenameBuf, &handle, &write);
    else
        handle = write = -1;

    if (_ParamPointer(1))
        _Param(1) = handle;
    if (_ParamPointer(2))
        _Param(2) = write;

    return _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(FileNameExist, StringRef, Int8)
{
    TempStackCStringFromString fileName(_Param(0));
    Int8 *flag = _ParamPointer(1);

    if (flag)
    {
        PRGID PrgId = CurrentProgramId();
        char Filename[MAX_FILENAME_SIZE];
        struct stat FileStatus;

        cMemoryFilename(PrgId, fileName.BeginCStr(), "", MAX_FILENAME_SIZE, Filename);
        *flag = (stat(Filename, &FileStatus) == 0);
    }

    return _NextInstruction();
}

#include "TypeDefiner.h"
DEFINE_VIREO_BEGIN(EV3_IO)
    DEFINE_VIREO_FUNCTION(FileOpenRead, "p(i(.String),o(.Int16),o(.Int32))");
    DEFINE_VIREO_FUNCTION(FileOpenWrite, "p(i(.String),o(.Int16))");
    DEFINE_VIREO_FUNCTION(FileOpenAppend, "p(i(.String),o(.Int16))");
    DEFINE_VIREO_FUNCTION(FileClose, "p(i(.Int16))");
    DEFINE_VIREO_FUNCTION(FileRead, "p(i(.Int16),i(.Int32),o(.Array))");
    DEFINE_VIREO_FUNCTION(FileWrite, "p(i(.Int16),i(.Array))");
    DEFINE_VIREO_FUNCTION(FileRemove, "p(i(.String))");
    DEFINE_VIREO_FUNCTION(FileResolveHandle, "p(i(.String),i(.Int16),i(.Int8))");
    DEFINE_VIREO_FUNCTION(FileNameExist, "p(i(.String),o(.Int8))");
DEFINE_VIREO_END()

