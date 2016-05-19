/* ========================================================================
   $File: tools/ctime/ctime.c $
   $Date: 2016/05/08 04:16:55PM $
   $Revision: 7 $
   $Creator: Casey Muratori $
   $Notice:

   The author of this software MAKES NO WARRANTY as to the RELIABILITY,
   SUITABILITY, or USABILITY of this software. USE IT AT YOUR OWN RISK.

   This is a simple timing utility.  It is in the public domain.
   Anyone can use it, modify it, roll'n'smoke hardcopies of the source
   code, sell it to the terrorists, etc.

   But the author makes absolutely no warranty as to the reliability,
   suitability, or usability of the software.  There might be bad bugs
   in here.  It could delete all your files.  It could format your
   hard drive.  I have no idea.  If you lose all your files from using
   it, it is your fault.

   $

   ctime is a simple utility that helps you keep track of how much time
   you spend building your projects.  You use it the same way you would
   use a begin/end block profiler in your normal code, only instead of
   profiling your code, you profile your build.

   BASIC INSTRUCTIONS
   ------------------

   On the very first line of your build script, you do something like this:

       ctime -begin timings_file_for_this_build.ctm

   and then on the very last line of your build script, you do

       ctime -end timings_file_for_this_build.ctm

   That's all there is to it!  ctime will keep track of every build you
   do, when you did it, and how long it took.  Later, when you'd like to
   get a feel for how your build times have evolved, you can type

       ctime -stats timings_file_for_this_build.ctm

   and it will tell you a number of useful statistics!


   ADVANCED INSTRUCTIONS
   ---------------------

   ctime has the ability to track the difference between _failed_ builds
   and _successful_ builds.  If you would like it to do so, you can capture
   the error status in your build script at whatever point you want,
   for example:

       set LastError=%ERRORLEVEL%

   and then when you eventually call ctime to end the profiling, you simply
   pass that error code to it:

       ctime -end timings_file_for_this_build.ctm %LastError%

   ctime can also dump all timings from a timing file into a textual
   format for use in other types of tools.  To get a CSV you can import
   into a graphing program or database, use:

       ctime -csv timings_file_for_this_build.ctm

   Also, you may want to do things like timing multiple builds separately,
   or timing builds based on what compiler flags are active.  To do this,
   you can use separate timing files for each configuration by using
   the shell variables for the build at the filename, eg.:

       ctime -begin timings_for_%BUILD_NAME%.ctm
       ...
       ctime -end timings_for_%BUILD_NAME%.ctm

   ======================================================================== */

#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#pragma pack(push,1)

#define MAGIC_VALUE 0xCA5E713F
typedef struct timing_file_header
{
    int unsigned MagicValue;
} timing_file_header;

typedef struct timing_file_date
{
    long long unsigned E;
} timing_file_date;

enum timing_file_entry_flag
{
    TFEF_Complete = 0x1,
    TFEF_NoErrors = 0x2,
};
typedef struct timing_file_entry
{
    timing_file_date StartDate;
    int unsigned Flags;
    int unsigned MillisecondsElapsed;
} timing_file_entry;

#pragma pack(pop)

typedef struct timing_entry_array
{
    int EntryCount;
    timing_file_entry *Entries;
} timing_entry_array;

//
// TODO(casey): More platforms?  Sadly, ANSI C doesn't support high-resolution timing across runs of a process AFAICT :(
//

#ifdef _WIN32

#include <windows.h>

static int unsigned
GetClock(void)
{
    if(sizeof(int unsigned) != sizeof(DWORD))
    {
        fprintf(stderr, "ERROR: Unexpected integer size - timing will not work on this platform!\n");
    }

    return(timeGetTime());
}

#elif __APPLE__
#include "TargetConditionals.h"
#    if TARGET_OS_MAC
#include <mach/mach_time.h>

static int unsigned
GetClock(void)
{
    static mach_timebase_info_data_t ToNanoseconds;
    if (ToNanoseconds.denom == 0)
    {
        mach_timebase_info(&ToNanoseconds);
    }
    uint64_t Ticks = mach_absolute_time();
    int unsigned Result = Ticks *
        ToNanoseconds.numer / 1000000 / ToNanoseconds.denom;
    return Result;
}
#    else
#        error "Unknown Apple target"
#    endif
#else

// This was written for Linux

static int unsigned
GetClock(void)
{
    struct timespec TimeSpec;
    int unsigned Result;

    clock_gettime(CLOCK_REALTIME, &TimeSpec);
    Result = TimeSpec.tv_sec * 1000 + TimeSpec.tv_nsec / 1000000;

    return Result;
}

#endif

//
//
//

static timing_file_date
GetDate(void)
{
    timing_file_date Result = {0};
    Result.E = time(NULL);
    return(Result);
}

static void
PrintDate(timing_file_date Date)
{
    time_t Time;
    struct tm *LocalTime;
    char Str[256];

    Time = Date.E;
    LocalTime = localtime(&Time);
    strftime(Str, 256, "%Y-%m-%d %H:%M:%S", LocalTime);
    fprintf(stdout, "%s", Str);
}

static long long unsigned
SecondDifference(timing_file_date A, timing_file_date B)
{
    long long unsigned Result = A.E - B.E;
    return Result;
}

static int unsigned
DayIndex(timing_file_date A)
{
    time_t Time;
    struct tm *LocalTime;

    Time = A.E;
    LocalTime = localtime(&Time);
    return LocalTime->tm_yday;
}

static void
Usage(void)
{
    fprintf(stderr, "CTime v1.0 by Casey Muratori\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  ctime -begin <timing file>\n");
    fprintf(stderr, "  ctime -end <timing file> [error level]\n");
    fprintf(stderr, "  ctime -stats <timing file>\n");
    fprintf(stderr, "  ctime -csv <timing file>\n");
}

static timing_entry_array
ReadAllEntries(FILE* Handle)
{
    timing_entry_array Result = {0};

    int EntriesBegin = sizeof(timing_file_header);
    int FileSize;
    if((fseek(Handle, 0, SEEK_END) == 0) && ((FileSize = ftell(Handle)) >= 0))
    {
        int EntriesSize = FileSize - EntriesBegin;
        Result.Entries = (timing_file_entry *)malloc(EntriesSize);
        if(Result.Entries)
        {
            fseek(Handle, EntriesBegin, SEEK_SET);
            int ReadSize = (int)fread(Result.Entries, 1, EntriesSize, Handle);
            if(ReadSize == EntriesSize)
            {
                Result.EntryCount = EntriesSize / sizeof(timing_file_entry);
            }
            else
            {
                fprintf(stderr, "ERROR: Unable to read timing entries from file.\n");
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Unable to allocate %d for storing timing entries.\n", EntriesSize);
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Unable to determine file size of timing file.\n");
    }

    return(Result);
}

static void
FreeAllEntries(timing_entry_array Array)
{
    if(Array.Entries)
    {
        free(Array.Entries);
        Array.EntryCount = 0;
        Array.Entries = 0;
    }
}

static void
CSV(timing_entry_array Array, char *TimingFileName)
{
    int EntryIndex;
    timing_file_entry *Entry = Array.Entries;

    fprintf(stdout, "%s Timings\n", TimingFileName);
    fprintf(stdout, "ordinal, date, duration, status\n");
    {for(EntryIndex = 0;
         EntryIndex < Array.EntryCount;
         ++EntryIndex, ++Entry)
    {
        fprintf(stdout, "%d, ", EntryIndex);
        PrintDate(Entry->StartDate);
        if(Entry->Flags & TFEF_Complete)
        {
            fprintf(stdout, ", %0.3fs, %s", (double)Entry->MillisecondsElapsed / 1000.0,
                    (Entry->Flags & TFEF_NoErrors) ? "succeeded" : "failed");
        }
        else
        {
            fprintf(stdout, ", (never completed), failed");
        }

        fprintf(stdout, "\n");
    }}
}

typedef struct time_part
{
    char *Name;
    double MillisecondsPer;
} time_part;

static void
PrintTime(double Milliseconds)
{
    double MillisecondsPerSecond = 1000;
    double MillisecondsPerMinute = 60*MillisecondsPerSecond;
    double MillisecondsPerHour = 60*MillisecondsPerMinute;
    double MillisecondsPerDay = 24*MillisecondsPerHour;
    double MillisecondsPerWeek = 7*MillisecondsPerDay;
    time_part Parts[] =
    {
        {"week", MillisecondsPerWeek},
        {"day", MillisecondsPerDay},
        {"hour", MillisecondsPerHour},
        {"minute", MillisecondsPerMinute},
    };
    int unsigned PartIndex;
    double Q = Milliseconds;

    for(PartIndex = 0;
        PartIndex < (sizeof(Parts)/sizeof(Parts[0]));
        ++PartIndex)
    {
        double MsPer = Parts[PartIndex].MillisecondsPer;
        double This = (double)(int)(Q / MsPer);

        if(This > 0)
        {
            fprintf(stdout, "%d %s%s, ", (int)This, Parts[PartIndex].Name,
                (This != 1) ? "s" : "");
        }
        Q -= This*MsPer;
    }

    fprintf(stdout, "%0.3f seconds", (double)Q / 1000.0);
}

static void
PrintTimeStat(char *Name, int unsigned Milliseconds)
{
    fprintf(stdout, "%s: ", Name);
    PrintTime((double)Milliseconds);
    fprintf(stdout, "\n");
}

typedef struct stat_group
{
    int unsigned Count;

    int unsigned SlowestMs;
    int unsigned FastestMs;
    double TotalMs;

} stat_group;

#define GRAPH_HEIGHT 10
#define GRAPH_WIDTH 30
typedef struct graph
{
    stat_group Buckets[GRAPH_WIDTH];
} graph;

static void
PrintStatGroup(char *Title, stat_group *Group)
{
    int unsigned AverageMs = 0;
    if(Group->Count >= 1)
    {
        AverageMs = (int unsigned)(Group->TotalMs / (double)Group->Count);
    }

    if(Group->Count > 0)
    {
        fprintf(stdout, "%s (%d):\n", Title, Group->Count);
        PrintTimeStat("  Slowest", Group->SlowestMs);
        PrintTimeStat("  Fastest", Group->FastestMs);
        PrintTimeStat("  Average", AverageMs);
        PrintTimeStat("  Total", (int unsigned)Group->TotalMs);
    }
}

static void
UpdateStatGroup(stat_group *Group, timing_file_entry *Entry)
{
    if(Group->SlowestMs < Entry->MillisecondsElapsed)
    {
        Group->SlowestMs = Entry->MillisecondsElapsed;
    }

    if(Group->FastestMs > Entry->MillisecondsElapsed)
    {
        Group->FastestMs = Entry->MillisecondsElapsed;
    }

    Group->TotalMs += (double)Entry->MillisecondsElapsed;

    ++Group->Count;
}

static int
MapToDiscrete(double Value, double InMax, double OutMax)
{
    int Result;

    if(InMax == 0)
    {
        InMax = 1;
    }

    Result = (int)((Value / InMax) * OutMax);

    return(Result);
}

static void
PrintGraph(char *Title, double DaySpan, graph *Graph)
{
    int BucketIndex;
    int LineIndex;
    int unsigned MaxCountInBucket = 0;
    int unsigned SlowestMs = 0;
    double DPB = DaySpan / (double)GRAPH_WIDTH;

    for(BucketIndex = 0;
        BucketIndex < GRAPH_WIDTH;
        ++BucketIndex)
    {
        stat_group *Group = Graph->Buckets + BucketIndex;

        if(Group->Count)
        {
//            double AverageMs = Group->TotalMs / (double)Group->Count;
            if(MaxCountInBucket < Group->Count)
            {
                MaxCountInBucket = Group->Count;
            }

            if(SlowestMs < Group->SlowestMs)
            {
                SlowestMs = Group->SlowestMs;
            }
        }
    }

    fprintf(stdout, "\n%s (%f day%s/bucket):\n", Title, DPB, (DPB == 1) ? "" : "s");
    for(LineIndex = GRAPH_HEIGHT - 1;
        LineIndex >= 0;
        --LineIndex)
    {
        fputc('|', stdout);
        for(BucketIndex = 0;
            BucketIndex < GRAPH_WIDTH;
            ++BucketIndex)
        {
            stat_group *Group = Graph->Buckets + BucketIndex;
            int This = -1;
            if(Group->Count)
            {
//                double AverageMs = Group->TotalMs / (double)Group->Count;
                This = MapToDiscrete(Group->SlowestMs, SlowestMs, GRAPH_HEIGHT - 1);
            }
            fputc((This >= LineIndex) ? '*' : ' ', stdout);
        }
        if(LineIndex == (GRAPH_HEIGHT - 1))
        {
            fputc(' ', stdout);
            PrintTime(SlowestMs);
        }
        fputc('\n', stdout);
    }
    fputc('+', stdout);
    for(BucketIndex = 0; BucketIndex < GRAPH_WIDTH; ++BucketIndex) {fputc('-', stdout);}
    fputc(' ', stdout);
    PrintTime(0);
    fputc('\n', stdout);
    fputc('\n', stdout);
    for(LineIndex = GRAPH_HEIGHT - 1;
        LineIndex >= 0;
        --LineIndex)
    {
        fputc('|', stdout);
        for(BucketIndex = 0;
            BucketIndex < GRAPH_WIDTH;
            ++BucketIndex)
        {
            stat_group *Group = Graph->Buckets + BucketIndex;
            int This = -1;
            if(Group->Count)
            {
                This = MapToDiscrete(Group->Count, MaxCountInBucket, GRAPH_HEIGHT - 1);
            }
            fputc((This >= LineIndex) ? '*' : ' ', stdout);
        }
        if(LineIndex == (GRAPH_HEIGHT - 1))
        {
            fprintf(stdout, " %u", MaxCountInBucket);
        }
        fputc('\n', stdout);
    }
    fputc('+', stdout);
    for(BucketIndex = 0; BucketIndex < GRAPH_WIDTH; ++BucketIndex) {fputc('-', stdout);}
    fprintf(stdout, " 0\n");
}

static void
Stats(timing_entry_array Array, char *TimingFileName)
{
    stat_group WithErrors = {0};
    stat_group NoErrors = {0};
    stat_group AllStats = {0};

    int unsigned IncompleteCount = 0;
    int unsigned DaysWithTimingCount = 0;
    int unsigned DaySpanCount = 0;

    int EntryIndex;

    timing_file_entry *Entry = Array.Entries;
    int unsigned LastDayIndex = 0;

    double AllMs = 0;

    double FirstDayAt = 0;
    double LastDayAt = 0;
    double DaySpan = 0;

    graph TotalGraph = {0};
    graph RecentGraph = {0};

    WithErrors.FastestMs = 0xFFFFFFFF;
    NoErrors.FastestMs = 0xFFFFFFFF;

    if(Array.EntryCount >= 2)
    {
        long long unsigned SecondD = SecondDifference(Array.Entries[Array.EntryCount - 1].StartDate, Array.Entries[0].StartDate);
        DaySpanCount = (int unsigned)(SecondD / (60 * 60 * 24));

        FirstDayAt = (double)DayIndex(Array.Entries[0].StartDate);
        LastDayAt = (double)DayIndex(Array.Entries[Array.EntryCount - 1].StartDate);
        DaySpan = (LastDayAt - FirstDayAt);
    }
    DaySpan += 1;

    for(EntryIndex = 0;
        EntryIndex < Array.EntryCount;
        ++EntryIndex, ++Entry)
    {
        if(Entry->Flags & TFEF_Complete)
        {
            stat_group *Group = (Entry->Flags & TFEF_NoErrors) ? &NoErrors : &WithErrors;

            int unsigned ThisDayIndex = DayIndex(Entry->StartDate);
            if(LastDayIndex != ThisDayIndex)
            {
                LastDayIndex = ThisDayIndex;
                ++DaysWithTimingCount;
            }

            UpdateStatGroup(Group, Entry);
            UpdateStatGroup(&AllStats, Entry);

            AllMs += (double)Entry->MillisecondsElapsed;

            {
                int GraphIndex = (int)(((double)(ThisDayIndex-FirstDayAt)/DaySpan)*(double)GRAPH_WIDTH);
                UpdateStatGroup(TotalGraph.Buckets + GraphIndex, Entry);
            }

            {
                int GraphIndex = (int)(ThisDayIndex - (LastDayAt - GRAPH_WIDTH + 1));
                if(GraphIndex >= 0)
                {
                    UpdateStatGroup(RecentGraph.Buckets + GraphIndex, Entry);
                }
            }
        }
        else
        {
            ++IncompleteCount;
        }
    }

    fprintf(stdout, "\n%s Statistics\n\n", TimingFileName);
    fprintf(stdout, "Total complete timings: %d\n", WithErrors.Count + NoErrors.Count);
    fprintf(stdout, "Total incomplete timings: %d\n", IncompleteCount);
    fprintf(stdout, "Days with timings: %d\n", DaysWithTimingCount);
    fprintf(stdout, "Days between first and last timing: %d\n", DaySpanCount);
    PrintStatGroup("Timings marked successful", &NoErrors);
    PrintStatGroup("Timings marked failed", &WithErrors);

    PrintGraph("All", (LastDayAt - FirstDayAt), &TotalGraph);
    PrintGraph("Recent", GRAPH_WIDTH, &RecentGraph);

    fprintf(stdout, "\nTotal time spent: ");
    PrintTime(AllMs);
    fprintf(stdout, "\n");
}

int
main(int ArgCount, char **Args)
{
    // TODO(casey): It would be nice if this supported 64-bit file sizes, but I can't really
    // tell right now if "ANSI C" supports this.  I feel like it should by now, but the
    // MSVC docs seem to suggest you have to use __int64 to do 64-bit stuff with the CRT
    // low-level IO routines, and I'm pretty sure that isn't a portable type :(

    // NOTE(casey): We snap the clock time right on entry, to minimize any overhead on
    // "end" times that might occur from opening the file.
    int unsigned EntryClock = GetClock();

    if((ArgCount == 3) || (ArgCount == 4))
    {
        char *Mode = Args[1];
        int ModeIsBegin = (strcmp(Mode, "-begin") == 0);
        char *TimingFileName = Args[2];
        timing_file_header Header = {0};

        FILE* Handle = fopen(TimingFileName, "r+b");
        if(Handle != NULL)
        {
            // NOTE(casey): The file exists - check the magic value
            fread(&Header, sizeof(Header), 1, Handle);
            if(Header.MagicValue == MAGIC_VALUE)
            {
                // NOTE(casey): The file is at least nominally valid.
            }
            else
            {
                fprintf(stderr, "ERROR: Unable to verify that \"%s\" is actually a ctime-compatible file.\n", TimingFileName);

                fclose(Handle);
                Handle = NULL;
            }
        }
        else if(ModeIsBegin)
        {
            // NOTE(casey): The file doesn't exist and we're starting a new timing, so create it.

            Handle = fopen(TimingFileName, "w+b");
            if(Handle != NULL)
            {
                Header.MagicValue = MAGIC_VALUE;
                if(fwrite(&Header, sizeof(Header), 1, Handle) == 1)
                {
                    // NOTE(casey): File creation was (presumably) successful.
                }
                else
                {
                    fprintf(stderr, "ERROR: Unable to write header to \"%s\".\n", TimingFileName);
                }
            }
            else
            {
                fprintf(stderr, "ERROR: Unable to create timing file \"%s\".\n", TimingFileName);
            }
        }

        if(Handle != NULL)
        {
            if(ModeIsBegin)
            {
                timing_file_entry NewEntry = {0};
                NewEntry.StartDate = GetDate();
                NewEntry.MillisecondsElapsed = GetClock();
                if((fseek(Handle, 0, SEEK_END) == 0) &&
                   (fwrite(&NewEntry, sizeof(NewEntry), 1, Handle) == 1))
                {
                    // NOTE(casey): Timer begin entry was written successfully.
                }
                else
                {
                    fprintf(stderr, "ERROR: Unable to append new entry to file \"%s\".\n", TimingFileName);
                }
            }
            else if(strcmp(Mode, "-end") == 0)
            {
                timing_file_entry LastEntry = {0};
                if((fseek(Handle, -(int)sizeof(timing_file_entry), SEEK_END) == 0) &&
                   (fread(&LastEntry, sizeof(LastEntry), 1, Handle) == 1))
                {
                    if(!(LastEntry.Flags & TFEF_Complete))
                    {
                        int unsigned StartClockD = LastEntry.MillisecondsElapsed;
                        int unsigned EndClockD = EntryClock;
                        LastEntry.Flags |= TFEF_Complete;
                        LastEntry.MillisecondsElapsed = 0;
                        if(StartClockD < EndClockD)
                        {
                            LastEntry.MillisecondsElapsed = (EndClockD - StartClockD);
                        }

                        if((ArgCount == 3) ||
                           ((ArgCount == 4) && (atoi(Args[3]) == 0)))
                        {
                            LastEntry.Flags |= TFEF_NoErrors;
                        }

                        if((fseek(Handle, -(int)sizeof(timing_file_entry), SEEK_END) == 0) &&
                           (fwrite(&LastEntry, sizeof(LastEntry), 1, Handle) == 1))
                        {
                            fprintf(stdout, "CTIME: ");
                            PrintTime(LastEntry.MillisecondsElapsed);
                            fprintf(stdout, " (%s)\n", TimingFileName);
                        }
                        else
                        {
                            fprintf(stderr, "ERROR: Unable to rewrite last entry to file \"%s\".\n", TimingFileName);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: Last entry in file \"%s\" is already closed - unbalanced/overlapped calls?\n", TimingFileName);
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR: Unable to read last entry from file \"%s\".\n", TimingFileName);
                }
            }
            else if(strcmp(Mode, "-stats") == 0)
            {
                timing_entry_array Array = ReadAllEntries(Handle);
                Stats(Array, TimingFileName);
                FreeAllEntries(Array);
            }
            else if(strcmp(Mode, "-csv") == 0)
            {
                timing_entry_array Array = ReadAllEntries(Handle);
                CSV(Array, TimingFileName);
                FreeAllEntries(Array);
            }
            else
            {
                fprintf(stderr, "ERROR: Unrecognized command \"%s\".\n", Mode);
            }

            fclose(Handle);
            Handle = NULL;
        }
        else
        {
            fprintf(stderr, "ERROR: Cannnot open file \"%s\".\n", TimingFileName);
        }
    }
    else
    {
        Usage();
    }
}
