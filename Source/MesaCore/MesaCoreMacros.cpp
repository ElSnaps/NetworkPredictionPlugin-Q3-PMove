// Copyright Snaps 2022, All Rights Reserved.

#include "MesaCoreMacros.h"

/*----------------------------------------------------------------------------
	Logging & Printing
----------------------------------------------------------------------------*/

// Log Categories
DEFINE_LOG_CATEGORY(LogMesa);
DEFINE_LOG_CATEGORY(LogMesaOnline);

/*----------------------------------------------------------------------------
	Profiling
----------------------------------------------------------------------------*/

namespace MesaSysScope
{
	const FName	Rendering	= FName(TEXT("Rendering"));
	const FName	Replication	= FName(TEXT("Replication"));
	const FName	Network		= FName(TEXT("Network"));
	const FName	Movement 	= FName(TEXT("Movement"));
	const FName	Physics		= FName(TEXT("Physics"));
	const FName	Animation	= FName(TEXT("Animation"));
	const FName	Pawn		= FName(TEXT("Pawn"));
	const FName	Grippables	= FName(TEXT("Grippables"));
	const FName	Gameplay	= FName(TEXT("Gameplay"));
	const FName	Misc		= FName(TEXT("Misc"));
}

#if !UE_BUILD_SHIPPING

DEFINE_MESA_PROFILE_CATEGORY(MesaRendering);
DEFINE_MESA_PROFILE_CATEGORY(MesaReplication);
DEFINE_MESA_PROFILE_CATEGORY(MesaNetwork);
DEFINE_MESA_PROFILE_CATEGORY(MesaMovement);
DEFINE_MESA_PROFILE_CATEGORY(MesaPhysics);
DEFINE_MESA_PROFILE_CATEGORY(MesaAnimation);
DEFINE_MESA_PROFILE_CATEGORY(MesaPawn);
DEFINE_MESA_PROFILE_CATEGORY(MesaGrippables);
DEFINE_MESA_PROFILE_CATEGORY(MesaGameplay);
DEFINE_MESA_PROFILE_CATEGORY(MesaMisc);

#endif