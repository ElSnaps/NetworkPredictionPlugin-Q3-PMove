// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MesaPlayerController.generated.h"

/*
	MesaPlayerController, does what it says on the tin mate.
*/
UCLASS()
class MESACORE_API AMesaPlayerController : public APlayerController
{
	GENERATED_BODY()
public:

/////////////////////////////////////////////////////////////////////////////////////
//	~Begin Network Clock
//		We don't use the built in network clock because epic can't make up their
//      mind on if we use floats or doubles for net time and it's inaccurate.
//		We use a modified version of Vori's circular buffer netclock.
//		https://vorixo.github.io/devtricks/non-destructive-synced-net-clock/
/////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure)
	float GetServerWorldTimeDelta() const;

	UFUNCTION(BlueprintPure)
	float GetServerWorldTime() const;

	void PostNetInit() override;

private:

	// Frequency that the client requests to adjust it's local clock. Set to zero to disable periodic updates.
	UPROPERTY(EditDefaultsOnly, meta=(AllowPrivateAccess = "true"))
	float NetClockResyncFrequency = 1.f;

	void RequestWorldTime();
	
	UFUNCTION(Server, Unreliable)
	void ServerRequestWorldTime(float ClientTimestamp);
	
	UFUNCTION(Client, Unreliable)
	void ClientUpdateWorldTime(float ClientTimestamp, float ServerTimestamp);

	float ServerWorldTimeDelta = 0.f;
	TArray<float> RTTCircularBuffer;

/////////////////////////////////////////////////////////////////////////////////////
//	~End Network Clock
/////////////////////////////////////////////////////////////////////////////////////

};
