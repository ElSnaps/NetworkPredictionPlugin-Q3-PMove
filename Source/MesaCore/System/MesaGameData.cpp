// Copyright Snaps 2022, All Rights Reserved.

#include "MesaGameData.h"
#include "MesaAssetManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
//#include "FMODEvent.h"

const UMesaGameData& UMesaGameData::Get()
{
	return UMesaAssetManager::Get().GetGameData();
}