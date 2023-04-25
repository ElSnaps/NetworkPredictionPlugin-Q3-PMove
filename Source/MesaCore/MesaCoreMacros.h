// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "ProfilingDebugging/CsvProfiler.h"

/*----------------------------------------------------------------------------
	Logging & Printing
----------------------------------------------------------------------------*/

// Log Categories
MESACORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMesa, Log, All);
MESACORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMesaOnline, Log, All);

// Easy usage wrapper for OnScreenDebugMessage / Print String Macro
#define UE_PRINT(text, ...) \
{ \
	FString LogPrefix = FString(__FUNCTION__); \
	FString OutPrint = LogPrefix + ": " + *FString::Printf(TEXT(text), ##__VA_ARGS__); \
	GEngine->AddOnScreenDebugMessage( \
	    - 1, \
	    10.f, \
	    FColor::White, \
	    *OutPrint); \
}

/*----------------------------------------------------------------------------
	Profiling
----------------------------------------------------------------------------*/

// Mesa system profiler categories
namespace MesaSysScope
{
	extern MESACORE_API const FName Rendering;
	extern MESACORE_API const FName Replication;
	extern MESACORE_API const FName Network;
	extern MESACORE_API const FName Movement;
	extern MESACORE_API const FName Physics;
	extern MESACORE_API const FName Animation;
	extern MESACORE_API const FName Pawn;
	extern MESACORE_API const FName Grippables;
	extern MESACORE_API const FName Gameplay;
	extern MESACORE_API const FName Misc;
}

#if !UE_BUILD_SHIPPING

#define DEFINE_MESA_PROFILE_CATEGORY(category) 		CSV_DEFINE_CATEGORY(category, true)
#define MESA_PROFILE_SCOPED(category, statname) 	CSV_SCOPED_TIMING_STAT(category, statname)

#endif