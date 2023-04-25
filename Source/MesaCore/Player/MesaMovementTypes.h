// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MesaMovementTypes.generated.h"

UENUM(BlueprintType)
enum class EMovementType : uint8
{
	Walking,
	Falling,
	Flying
};

// FMovementCommand is sent to the server each client frame.
USTRUCT()
struct FMovementCommand
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 CommandNumber = 0;

	UPROPERTY()
	float Timestamp	= 0.f;

	UPROPERTY()
	uint8 MovementInputs = 0;

	FMovementCommand() = default;

	FMovementCommand(uint32 CommandNumber, float Timestamp, uint8 MovementInputs)
		: CommandNumber(CommandNumber), Timestamp(Timestamp), MovementInputs(MovementInputs) {}
};