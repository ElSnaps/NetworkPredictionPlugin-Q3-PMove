// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "MesaMovementTypes.h"
#include "Misc/StringBuilder.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionSimulation.h"

#define QUAKESTYLE		0

namespace MesaMovementConfig
{
	const float MovementSpeed 		= 500.f;
	const float Gravity 			= 800.f;
	const float StopSpeed 			= 100.f;
	const float Acceleration 		= 10.f;
	const float AirAcceleration 	= 1.f;
	const float FlightAcceleration	= 8.f;
	const float Friction 			= 6.f;
	const float FlightFriction 		= 3.f;
	const float JumpSpeed 			= 350.f;
}

/*
	Base Simulation for Game Movement designed to be a lightweight alternative to CMC.
	The "Simulation" provides all the actual movement code, of which we derive from Quake.
*/

struct FMesaMovementInputCmd // Input Cmd generated by the Client
{
	float YawInput;
	FVector MovementInput;
	bool bJumpPressed;

	FMesaMovementInputCmd()
		: 	YawInput(ForceInitToZero),
			MovementInput(ForceInitToZero),
			bJumpPressed(false)
	{}

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << YawInput;
		P.Ar << MovementInput;
		P.Ar << bJumpPressed;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("YawInput: %.2f\n", YawInput);
		Out.Appendf("MovementInput: X=%.2f Y=%.2f Z=%.2f\n", MovementInput.X, MovementInput.Y, MovementInput.Z);
		Out.Appendf("bJumpPressed: X=%\n", bJumpPressed ? TEXT("True") : TEXT("False"));
	}
};

struct FMesaMovementSyncState // State we are evolving frame to frame and keeping in sync
{
	FVector Location;
	FVector Velocity;
	FRotator Rotation;

	FMesaMovementSyncState()
	: Location(ForceInitToZero)
	, Velocity(ForceInitToZero)
	, Rotation(ForceInitToZero)
	{ }

	bool ShouldReconcile(const FMesaMovementSyncState& AuthorityState) const;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << Location;
		P.Ar << Velocity;
		P.Ar << Rotation;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Loc: X=%.2f Y=%.2f Z=%.2f\n", Location.X, Location.Y, Location.Z);
		Out.Appendf("Vel: X=%.2f Y=%.2f Z=%.2f\n", Velocity.X, Velocity.Y, Velocity.Z);
		Out.Appendf("Rot: P=%.2f Y=%.2f R=%.2f\n", Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
	}

	void Interpolate(const FMesaMovementSyncState* From, const FMesaMovementSyncState* To, float PCT)
	{
		static constexpr float TeleportThreshold = 1000.f * 1000.f;
		if (FVector::DistSquared(From->Location, To->Location) > TeleportThreshold)
		{
			*this = *To;
		}
		else
		{
			Location = FMath::Lerp(From->Location, To->Location, PCT);
			Velocity = FMath::Lerp(From->Velocity, To->Velocity, PCT);
			Rotation = FMath::Lerp(From->Rotation, To->Rotation, PCT);
		}
	}
};

// Epic naming is so shit, guess "Aux" just means "Extra" - fuck knows.
// Auxiliary state that is input into the simulation.
struct FMesaMovementAuxState
{	
	bool ShouldReconcile(const FMesaMovementAuxState& AuthorityState) const;

	void NetSerialize(const FNetSerializeParams& P)
	{
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
	}

	void Interpolate(const FMesaMovementAuxState* From, const FMesaMovementAuxState* To, float PCT)
	{
	}
};

using MesaMovementStateTypes = TNetworkPredictionStateTypes<FMesaMovementInputCmd, FMesaMovementSyncState, FMesaMovementAuxState>;

class FMesaMovementSimulation
{
public:

	bool SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport) const;
	bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport) const;
	FTransform GetUpdateComponentTransform() const;

	FVector GetPenetrationAdjustment(const FHitResult& Hit) const;

	bool OverlapTest(const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, 
	    const FCollisionShape& CollisionShape, const AActor* IgnoreActor) const;
	
	void InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const;
	bool ResolvePenetration(const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat) const;

	/**  Flags that control the behavior of calls to MoveComponent() on our UpdatedComponent. */
	mutable EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags; // Mutable because we sometimes need to disable these flags ::ResolvePenetration. Better way may be possible

	void SetComponents(USceneComponent* InUpdatedComponent, UPrimitiveComponent* InPrimitiveComponent)
	{
		UpdatedComponent = InUpdatedComponent;
		UpdatedPrimitive = InPrimitiveComponent;
	}

protected:

	USceneComponent* UpdatedComponent = nullptr;
	UPrimitiveComponent* UpdatedPrimitive = nullptr;

public:

	/** Main update function */
	MESACORE_API void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MesaMovementStateTypes>& Input, 
	    const TNetSimOutput<MesaMovementStateTypes>& Output);

	// general tolerance value for rotation checks
	static constexpr float ROTATOR_TOLERANCE = (1e-3);

	/** Dev tool to force simple mispredict */
	static bool ForceMispredict;

protected:

	float SlideAlongSurface(const FVector& Delta, float Time, const FQuat Rotation, const FVector& Normal,
	    FHitResult& Hit, bool bHandleImpact);


public:

	FRotator			PlayerRotation		= FRotator::ZeroRotator;
	FVector				MovementInput		= FVector::ZeroVector;
	FVector				Velocity			= FVector::ZeroVector;
	FHitResult			GroundTrace			= {};
	EMovementType 		MovementType 		= EMovementType::Falling;
	bool				bPendingJump 		= false;

	void Accelerate(float DeltaTime, FVector WishDirection, float WishSpeed, float Acceleration);
	void ApplyFriction(float DeltaTime);

	void WalkMove(float DeltaTime);
	void AirMove(float DeltaTime);
	void FlyMove(float DeltaTime);

	bool CheckJump();
	void TraceForGround();
};