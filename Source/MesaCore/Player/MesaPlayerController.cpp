// Copyright Snaps 2022, All Rights Reserved.

#include "MesaPlayerController.h"
#include "MesaCoreMacros.h"

/////////////////////////////////////////////////////////////////////////////////////
//	~Begin Network Clock
/////////////////////////////////////////////////////////////////////////////////////

float AMesaPlayerController::GetServerWorldTimeDelta() const
{
	return ServerWorldTimeDelta;
}

float AMesaPlayerController::GetServerWorldTime() const
{
	return GetWorld()->GetTimeSeconds() + ServerWorldTimeDelta;
}

void AMesaPlayerController::PostNetInit()
{
	Super::PostNetInit();
	if (GetLocalRole() != ROLE_Authority)
	{
		RequestWorldTime();
		if (NetClockResyncFrequency > 0.f)
		{
			FTimerHandle TimerHandle;
			GetWorldTimerManager().SetTimer(TimerHandle, this, &ThisClass::RequestWorldTime, NetClockResyncFrequency, true);
		}
	}
}

void AMesaPlayerController::RequestWorldTime()
{
	ServerRequestWorldTime(GetWorld()->GetTimeSeconds());
}

void AMesaPlayerController::ClientUpdateWorldTime_Implementation(float ClientTimestamp, float ServerTimestamp)
{
	//MESA_PROFILE_SCOPED(MesaSysScope::Network, AMesaPlayerController::ClientUpdateWorldTime_Implementation);
	float AdjustedRTT = 0;
	const float RoundTripTime = GetWorld()->GetTimeSeconds() - ClientTimestamp;

	RTTCircularBuffer.Add(RoundTripTime);
	if (RTTCircularBuffer.Num() == 10)
	{
		TArray<float> TempBuffer = RTTCircularBuffer;
		TempBuffer.Sort();
		for (int32 Index = 1; Index < 9; ++Index)
		{
			AdjustedRTT += TempBuffer[Index];
		}
		AdjustedRTT /= 8;
		RTTCircularBuffer.RemoveAt(0);
	}
	else
	{
		AdjustedRTT = RoundTripTime;
	}
	
	ServerWorldTimeDelta = ServerTimestamp - ClientTimestamp - AdjustedRTT / 2.f;
}

void AMesaPlayerController::ServerRequestWorldTime_Implementation(float ClientTimestamp)
{
	const float Timestamp = GetWorld()->GetTimeSeconds();
	ClientUpdateWorldTime(ClientTimestamp, Timestamp);
}

/////////////////////////////////////////////////////////////////////////////////////
//	~End Network Clock
/////////////////////////////////////////////////////////////////////////////////////