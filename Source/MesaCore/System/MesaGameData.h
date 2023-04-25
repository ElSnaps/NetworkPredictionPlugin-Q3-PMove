// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MesaGameData.generated.h"

class UGameplayEffect;
//class UFMODEvent;
class UInputMappingContext;
class UInputAction;

/*
	UMesaGameData.
	Non-mutable data asset that contains global game data.
*/
UCLASS(BlueprintType, Const, Meta = (DisplayName = "Mesa Game Data", ShortTooltip = "Data asset containing global game data."))
class UMesaGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// Returns the loaded game data.
	static const UMesaGameData& Get();

	//UPROPERTY(EditDefaultsOnly, Category = "Audio")
	//TSoftObjectPtr<UFMODEvent> RespawnAudioEvent;

	//UPROPERTY(EditDefaultsOnly, Category = "Audio")
	//TSoftObjectPtr<UFMODEvent> JumpAudioEvent;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> DesktopInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> VRInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InputActionMove;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InputActionLook;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InputActionJump;

};
