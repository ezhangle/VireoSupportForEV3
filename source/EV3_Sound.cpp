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

#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include "c_sound.h"
}

#include "TypeAndDataManager.h"
#include "StringUtilities.h"

using namespace Vireo;


VIREO_FUNCTION_SIGNATURE0(SoundStop)
{
    // Stop sound
    SoundInstance.cSoundState = SOUND_STOPPED;
    if (SoundInstance.hSoundFile >= 0)
    {
        close(SoundInstance.hSoundFile);
        SoundInstance.hSoundFile = -1;
    }

    // Assemble SoundData
    const UInt8 BytesToWrite = 1;
    DATA8 SoundData[BytesToWrite];
    SoundData[0] = BREAK;

    // Write to sound device file
    SoundInstance.SoundDriverDescriptor = open(SOUND_DEVICE_NAME, O_WRONLY);
    if (SoundInstance.SoundDriverDescriptor >= 0)
    {
        write(SoundInstance.SoundDriverDescriptor, SoundData, BytesToWrite);
        close(SoundInstance.SoundDriverDescriptor);
        SoundInstance.SoundDriverDescriptor = -1;
    }
    else
        SoundInstance.cSoundState = SOUND_STOPPED;

    return _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE3(SoundTone, UInt8, UInt16, UInt16)
{
    UInt8  Volume    = _Param(0);
    UInt16 Frequency = _Param(1);
    UInt16 Duration  = _Param(2);

    Volume = Volume > 0   ? Volume : 0;
    Volume = Volume < 100 ? Volume : 100;

    // Change state
    (*SoundInstance.pSound).Status = BUSY;
    SoundInstance.cSoundState = SOUND_TONE_PLAYING;
    SoundInstance.SoundOwner = CallingObjectId();

    // Assemble SoundData
    const UInt8 BytesToWrite = 6;
    DATA8 SoundData[BytesToWrite];
    SoundData[0] = TONE;
    SoundData[1] = (Volume + 7) / 8;
    SoundData[2] = (UBYTE) Frequency;
    SoundData[3] = (UBYTE) (Frequency >> 8);
    SoundData[4] = (UBYTE) Duration;
    SoundData[5] = (UBYTE) (Duration >> 8);

    // Write to sound device file
    SoundInstance.SoundDriverDescriptor = open(SOUND_DEVICE_NAME, O_WRONLY);
    if (SoundInstance.SoundDriverDescriptor >= 0)
    {
        write(SoundInstance.SoundDriverDescriptor, SoundData, BytesToWrite);
        close(SoundInstance.SoundDriverDescriptor);
        SoundInstance.SoundDriverDescriptor = -1;
    }
    else
        SoundInstance.cSoundState = SOUND_STOPPED;

    return _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(SoundPlay, UInt8, StringRef)
{
    UInt8     Volume   = _Param(0);
    TempStackCStringFromString fileName(_Param(1));

    Volume = Volume > 0   ? Volume : 0;
    Volume = Volume < 100 ? Volume : 100;

    SoundInstance.cSoundState = SOUND_STOPPED;
    if (SoundInstance.hSoundFile >= 0)
    {
        close(SoundInstance.hSoundFile);
        SoundInstance.hSoundFile = -1;
    }
    SoundInstance.SoundOwner = CallingObjectId();

    // Assemble SoundData
    const UInt8 BytesToWrite = 2;
    DATA8 SoundData[BytesToWrite];
    SoundData[0] = PLAY;
    SoundData[1] = (Volume + 7) / 8;

    // Handle fileName
    char pathName[MAX_FILENAME_SIZE];
    UBYTE Tmp1;
    UBYTE Tmp2;

    if (fileName.BeginCStr())
    {
        pathName[0] = 0;
        if (*((Utf8Char *) fileName.BeginCStr()) != '.' && *((Utf8Char *) fileName.BeginCStr()) != '/')
        {
            GetResourcePath(pathName, MAX_FILENAME_SIZE);
            sprintf(SoundInstance.PathBuffer, "%s%s.rsf", pathName, fileName.BeginCStr());
        }
        else
            sprintf(SoundInstance.PathBuffer, "%s.rsf", fileName.BeginCStr());

        SoundInstance.hSoundFile = open(SoundInstance.PathBuffer, O_RDONLY, 0666);
        if (SoundInstance.hSoundFile >= 0)
        {
            stat(SoundInstance.PathBuffer,&SoundInstance.FileStatus);
            SoundInstance.SoundFileLength = SoundInstance.FileStatus.st_size;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundFileFormat = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundDataLength = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundSampleRate = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundPlayMode = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            SoundInstance.cSoundState = SOUND_SETUP_FILE;

            if (SoundInstance.SoundFileFormat == FILEFORMAT_ADPCM_SOUND)
                cSoundInitAdPcm();

            SoundInstance.SoundDriverDescriptor = open(SOUND_DEVICE_NAME, O_WRONLY);
            if (SoundInstance.SoundDriverDescriptor >= 0)
            {
                write(SoundInstance.SoundDriverDescriptor, SoundData, BytesToWrite);
                close(SoundInstance.SoundDriverDescriptor);
                SoundInstance.SoundDriverDescriptor = -1;
                SoundInstance.cSoundState = SOUND_FILE_PLAYING;
                (*SoundInstance.pSound).Status = BUSY;
            }
        }
    }

    return _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE2(SoundPlayLoop, UInt8, StringRef)
{
    UInt8     Volume   = _Param(0);
    TempStackCStringFromString fileName(_Param(1));

    Volume = Volume > 0   ? Volume : 0;
    Volume = Volume < 100 ? Volume : 100;

    SoundInstance.cSoundState = SOUND_STOPPED;
    if (SoundInstance.hSoundFile >= 0)
    {
        close(SoundInstance.hSoundFile);
        SoundInstance.hSoundFile = -1;
    }
    SoundInstance.SoundOwner = CallingObjectId();

    // Assemble SoundData
    const UInt8 BytesToWrite = 2;
    DATA8 SoundData[BytesToWrite];
    SoundData[0] = PLAY;
    SoundData[1] = (Volume + 7) / 8;

    // Handle fileName
    char pathName[MAX_FILENAME_SIZE];
    UBYTE Tmp1;
    UBYTE Tmp2;

    if (fileName.BeginCStr())
    {
        pathName[0] = 0;
        if (*((Utf8Char *) fileName.BeginCStr()) != '.' && *((Utf8Char *) fileName.BeginCStr()) != '/')
        {
            GetResourcePath(pathName, MAX_FILENAME_SIZE);
            sprintf(SoundInstance.PathBuffer, "%s%s.rsf", pathName, fileName.BeginCStr());
        }
        else
            sprintf(SoundInstance.PathBuffer, "%s.rsf", fileName.BeginCStr());

        SoundInstance.hSoundFile = open(SoundInstance.PathBuffer, O_RDONLY, 0666);
        if (SoundInstance.hSoundFile >= 0)
        {
            stat(SoundInstance.PathBuffer,&SoundInstance.FileStatus);
            SoundInstance.SoundFileLength = SoundInstance.FileStatus.st_size;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundFileFormat = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundDataLength = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundSampleRate = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            read(SoundInstance.hSoundFile, &Tmp1, 1);
            read(SoundInstance.hSoundFile, &Tmp2, 1);
            SoundInstance.SoundPlayMode = (UWORD)Tmp1 << 8 | (UWORD)Tmp2;

            SoundInstance.cSoundState = SOUND_SETUP_FILE;

            if (SoundInstance.SoundFileFormat == FILEFORMAT_ADPCM_SOUND)
                cSoundInitAdPcm();

            SoundInstance.SoundDriverDescriptor = open(SOUND_DEVICE_NAME, O_WRONLY);
            if (SoundInstance.SoundDriverDescriptor >= 0)
            {
                write(SoundInstance.SoundDriverDescriptor, SoundData, BytesToWrite);
                close(SoundInstance.SoundDriverDescriptor);
                SoundInstance.SoundDriverDescriptor = -1;
                SoundInstance.cSoundState = SOUND_FILE_LOOPING;
                (*SoundInstance.pSound).Status = BUSY;
            }
        }
    }

    return _NextInstruction();
}

VIREO_FUNCTION_SIGNATURE1(SoundTest, UInt8)
{
    UInt8 *isBusy = _ParamPointer(0);

    if (isBusy)
        *isBusy = ((*SoundInstance.pSound).Status == BUSY);

    return _NextInstruction();
}

#include "TypeDefiner.h"
DEFINE_VIREO_BEGIN(EV3_IO)
    DEFINE_VIREO_FUNCTION(SoundStop, "p()");
    DEFINE_VIREO_FUNCTION(SoundTone, "p(i(.UInt8),i(.UInt16),i(.UInt16))");
    DEFINE_VIREO_FUNCTION(SoundPlay, "p(i(.UInt8),i(.String))");
    DEFINE_VIREO_FUNCTION(SoundPlayLoop, "p(i(.UInt8),i(.String))");
    DEFINE_VIREO_FUNCTION(SoundTest,"p(o(.UInt8))");
DEFINE_VIREO_END()

