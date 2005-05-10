#define WIN32_LEAN_AND_MEAN
#include <windows.h>			// for mmsystem.h
#include <mmsystem.h>			// for timeGetTime() staff
#include "Core.h"


double GSecondsPerCycle;		// really, should use GCyclesPerSecond, but this will require divide operation every
								// time we use this, so GSecondsPerCycle=1/GCyclesPerSecond
double GMSecondsPerCycle;		// == 1000*GSecondsPerCycle
static unsigned timeBase;

static bool IsMMX, IsSSE, IsRDTSC, Is3DNow;


// We should subtract timeBase from acquired appCycles64 value to determine time since
// application start, this is useful only for improving precision of retreived float
// milliseconds value (large integer parts will reduce precision of float part); for
// the same reason, we use "double" instead of "float" as a result value.

double appSeconds ()
{
	if (IsRDTSC)
	{
		__int64 cycles = appCycles64 ();
		return cycles * GSecondsPerCycle;
	}
	else
		return (timeGetTime () - timeBase) / 1000.0;
}


double appMillisecondsf ()
{
	if (IsRDTSC)
	{
		__int64 cycles = appCycles64 ();
		return cycles * GMSecondsPerCycle;
	}
	else
		return timeGetTime () - timeBase;
}


unsigned appMilliseconds ()
{
	if (IsRDTSC)
		return appFloor (appMillisecondsf ());
	else
		return timeGetTime () - timeBase;
}


static unsigned cpuidRegs[4];

void cpuid (unsigned code)
{
	__asm {
		mov		eax,code
		cpuid
		mov		cpuidRegs,eax
		mov		cpuidRegs+4,ebx
		mov		cpuidRegs+8,ecx
		mov		cpuidRegs+12,edx
	}
}

#if 1

#pragma warning(push)
#pragma warning(disable : 4035)		// "no return value"
inline unsigned cpuid0 (unsigned code)
{
	__asm {
		mov		eax,code
		cpuid
	}
}

inline unsigned cpuid3 (unsigned code)
{
	__asm {
		mov		eax,code
		cpuid
		mov		eax,edx
	}
}
#pragma warning(pop)

#else

inline unsigned cpuid0 (unsigned code)
{
	cpuid (code);
	return cpuidRegs[0];
}
inline unsigned cpuid3 (unsigned code)
{
	cpuid (code);
	return cpuidRegs[3];
}
#endif

static void CheckCpuModel (void)
{
#define MAX_CPU_NAME_PARTS	12	// 3 times of 4 regs
	union {
		unsigned reg[MAX_CPU_NAME_PARTS];
		char	name[MAX_CPU_NAME_PARTS*4+1];
	} t;

	// cpuid presence
	unsigned cpu0;
	__try
	{
		cpu0 = cpuid0 (0x80000000);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		// no CPUID available
		appPrintf ("Unknown 386/486 CPU\n");
		return;
	}

	// CPU name
	if (cpu0 >= 0x80000004)		// extended vendor string available
	{
		int i = 0;
		for (unsigned fn = 0x80000002; fn <= 0x80000004; fn++)
		{
			cpuid (fn);
			memcpy (t.reg+i, cpuidRegs, sizeof(cpuidRegs));
			i += 4;
		}
		t.name[i*4] = 0;
		const char *s = t.name;
		while (*s == ' ') s++;
		appPrintf ("CPU name: %s\n", s);
	}
	else
	{
		cpuid (0);
		t.reg[0] = cpuidRegs[1];
		t.reg[1] = cpuidRegs[3];
		t.reg[2] = cpuidRegs[2];
		t.name[12] = 0;		// ASCIIZ
		appPrintf ("CPU name: %s\n", t.name);
	}

	// caps
	appPrintf ("CPU caps: [ ");

	unsigned tmp = cpuid3 (1);
	if (tmp & 0x00000001)	Com_Printf ("FPU ");
	//!! NOTE: if planning to use MMX/SSE/SSE2/3DNow! - should check OS support for this tech
	if (tmp & 0x00800000)
	{
		appPrintf ("MMX ");
		IsMMX = true;
	}
	if (tmp & 0x02000000)
	{
		appPrintf ("SSE ");
		IsSSE = true;
	}
	if (tmp & 0x04000000)	Com_Printf ("SSE2 ");
	if (tmp & 0x00000010)
	{
		appPrintf ("RDTSC ");
		IsRDTSC = true;
	}

	// check extended features
	if (cpu0 >= 0x80000001)		// largest recognized extended function
	{
		tmp = cpuid3 (0x80000001);
		if (tmp & 0x80000000)
		{
			appPrintf ("3DNow! ");
			Is3DNow = true;
		}
	}

	appPrintf ("]\n");
}


static void CheckCpuSpeed (void)
{
	if (!IsRDTSC) return;

	timeBeginPeriod (1);

	GMSecondsPerCycle = 1.0;					// initial value
	for (int tries = 0; tries < 4; tries++)
	{
		__int64 stamp1 = appCycles64 ();
		unsigned time1 = timeGetTime ();
		unsigned time2;
		while (true)
		{
			time2 = timeGetTime ();
			if (time2 - time1 > 200) break;		// 200ms enough to compute CPU speed
		}
		__int64 stamp2 = appCycles64 ();
		double msecPerCycle = (time2 - time1) / (((double)stamp2 - (double)stamp1));
		if (msecPerCycle < GMSecondsPerCycle) GMSecondsPerCycle = msecPerCycle;

//		Com_Printf ("try #%d\n", tries);
//		Com_Printf ("stampd: %u %u   timed: %d\n", stamp2 - stamp1, time2 - time1);
//		Com_Printf ("CPU speed: %g MHz\n", 1e-6 / secPerCycle1);
	}
	GSecondsPerCycle = GMSecondsPerCycle / 1000.0;
	appPrintf ("CPU speed: %g MHz\n", 1e-6 / GSecondsPerCycle);

	timeEndPeriod (1);
}


void appInitPlatform ()
{
	CheckCpuModel ();
	CheckCpuSpeed ();
#if 0
	// DEBUG: force mmsystem timing functions
	IsRDTSC = false;
	appWPrintf ("DEBUG: force mmsystem time measuring\n");
#endif
	// NOTE: under WinXP (Win2k too?) without timeBeginPeriod(1) Sleep() (and some another time-related
	//	functions too) will work in bad resolution; so, we should use timeBeginPeriod(1) even if don't
	//	use timeGetTime()
	timeBeginPeriod (1);
	if (!IsRDTSC) timeBase = timeGetTime ();
}


void appShutdownPlatform ()
{
	timeEndPeriod (1);
}