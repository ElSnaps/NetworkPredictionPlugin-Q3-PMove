// Copyright Snaps 2022, All Rights Reserved.

#include "MesaMovementSimulation.h"
#include "System/MesaGameData.h"

#include "Components/CapsuleComponent.h"
#include "NetworkPredictionTrace.h"
#include "DrawDebugHelpers.h"

//#include "FMODEvent.h"
//#include "FMODBlueprintStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogMesaPawnSimulation, Log, All);

namespace MesaPawnSimCVars
{
	static float ErrorTolerance = 10.f;
	static FAutoConsoleVariableRef CVarErrorTolerance(
		TEXT("MesaMovement.ErrorTolerance"),
		ErrorTolerance,
		TEXT("Location tolerance for reconcile"), 
		ECVF_Default
	);
}

// -------------------------------------------------------------------------------------------------------

bool FMesaMovementAuxState::ShouldReconcile(const FMesaMovementAuxState& AuthorityState) const
{
	return false;
}

bool FMesaMovementSyncState::ShouldReconcile(const FMesaMovementSyncState& AuthorityState) const
{
	const float ErrorTolerance = MesaPawnSimCVars::ErrorTolerance;
	UE_NP_TRACE_RECONCILE(!AuthorityState.Location.Equals(Location, ErrorTolerance), "Loc:");
	return false;
}

// -------------------------------------------------------------------------------------------------------

bool FMesaMovementSimulation::ForceMispredict = false;
static FVector ForceMispredictVelocityMagnitude = FVector(2000.f, 0.f, 0.f);

namespace BaseMovementCVars
{
	static float PenetrationPullbackDistance = 0.125f;
	static FAutoConsoleVariableRef CVarPenetrationPullbackDistance(
		TEXT("bm.PenetrationPullbackDistance"),
		PenetrationPullbackDistance,
	    TEXT("Pull out from penetration of an object by this extra distance.\n")
	    TEXT("Distance added to penetration fix-ups."),
	    ECVF_Default
	);

	static float PenetrationOverlapCheckInflation = 0.100f;
	static FAutoConsoleVariableRef CVarPenetrationOverlapCheckInflation(
		TEXT("bm.PenetrationOverlapCheckInflation"),
	    PenetrationOverlapCheckInflation,
	   	TEXT("Inflation added to object when checking if a location is free of blocking collision.\n")
	    TEXT("Distance added to inflation in penetration overlap check."),
	    ECVF_Default
	);

	static int32 RequestMispredict = 0;
	static FAutoConsoleVariableRef CVarRequestMispredict(
		TEXT("bm.RequestMispredict"),
	    RequestMispredict,
		TEXT("Causes a misprediction by inserting random value into stream on authority side"), 
		ECVF_Default
	);
}

DEFINE_LOG_CATEGORY_STATIC(LogBaseMovement, Log, All);

// ----------------------------------------------------------------------------------------------------------
//	Movement System Driver
//
//	NOTE: Most of the Movement Driver is not ideal! We are at the mercy of the UpdateComponent since it is the
//	the object that owns its collision data and its MoveComponent function. Ideally we would have everything within
//	the movement simulation code and it do its own collision queries. But instead we have to come back to the Driver/Component
//	layer to do this kind of stuff.
//
// ----------------------------------------------------------------------------------------------------------

FVector FMesaMovementSimulation::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	if (!Hit.bStartPenetrating)
	{
		return FVector::ZeroVector;
	}

	FVector Result;
	const float PullBackDistance = FMath::Abs(BaseMovementCVars::PenetrationPullbackDistance);
	const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);

	Result = Hit.Normal * (PenetrationDepth + PullBackDistance);

	return Result;
}

bool FMesaMovementSimulation::OverlapTest(const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MovementOverlapTest), false, IgnoreActor);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	return UpdatedComponent->GetWorld()->OverlapBlockingTestByChannel(Location, RotationQuat, CollisionChannel, CollisionShape, QueryParams, ResponseParam);
}

void FMesaMovementSimulation::InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const
{
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->InitSweepCollisionParams(OutParams, OutResponseParam);
	}
}

bool FMesaMovementSimulation::ResolvePenetration(const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat) const
{
	// SceneComponent can't be in penetration, so this function really only applies to PrimitiveComponent.
	const FVector Adjustment = ProposedAdjustment; //ConstrainDirectionToPlane(ProposedAdjustment);
	if (!Adjustment.IsZero() && UpdatedPrimitive)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_BaseMovementComponent_ResolvePenetration);
		// See if we can fit at the adjusted location without overlapping anything.
		AActor* ActorOwner = UpdatedComponent->GetOwner();
		if (!ActorOwner)
		{
			return false;
		}

		UE_LOG(LogBaseMovement, Verbose, TEXT("ResolvePenetration: %s.%s at location %s inside %s.%s at location %s by %.3f (netmode: %d)"),
		       *ActorOwner->GetName(),
		       *UpdatedComponent->GetName(),
		       *UpdatedComponent->GetComponentLocation().ToString(),
		       *GetNameSafe(Hit.GetActor()),
		       *GetNameSafe(Hit.GetComponent()),
		       Hit.Component.IsValid() ? *Hit.GetComponent()->GetComponentLocation().ToString() : TEXT("<unknown>"),
		       Hit.PenetrationDepth,
		       (uint32)ActorOwner->GetNetMode());

		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.
		const float OverlapInflation = BaseMovementCVars::PenetrationOverlapCheckInflation;
		bool bEncroached = OverlapTest(Hit.TraceStart + Adjustment, NewRotationQuat, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(OverlapInflation), ActorOwner);
		if (!bEncroached)
		{
			// Move without sweeping.
			MoveUpdatedComponent(Adjustment, NewRotationQuat, false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogBaseMovement, Verbose, TEXT("ResolvePenetration:   teleport by %s"), *Adjustment.ToString());
			return true;
		}
		else
		{
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);
			bool bMoved = MoveUpdatedComponent(Adjustment, NewRotationQuat, true, &SweepOutHit, ETeleportType::TeleportPhysics);
			UE_LOG(LogBaseMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (success = %d)"), *Adjustment.ToString(), bMoved);

			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = MoveUpdatedComponent(CombinedMTD, NewRotationQuat, true, nullptr, ETeleportType::TeleportPhysics);
					UE_LOG(LogBaseMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (MTD combo success = %d)"), *CombinedMTD.ToString(), bMoved);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = (Hit.TraceEnd - Hit.TraceStart); //ConstrainDirectionToPlane(Hit.TraceEnd - Hit.TraceStart);
				if (!MoveDelta.IsZero())
				{
					bMoved = MoveUpdatedComponent(Adjustment + MoveDelta, NewRotationQuat, true, nullptr, ETeleportType::TeleportPhysics);
					UE_LOG(LogBaseMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (adjusted attempt success = %d)"), *(Adjustment + MoveDelta).ToString(), bMoved);
				}
			}	

			return bMoved;
		}
	}

	return false;
}

bool FMesaMovementSimulation::SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport) const
{
	if (UpdatedComponent == NULL)
	{
		OutHit.Reset(1.f);
		return false;
	}

	bool bMoveResult = false;

	// Scope for move flags
	{
		bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit, Teleport);
	}

	// Handle initial penetrations
	if (OutHit.bStartPenetrating && UpdatedComponent)
	{
		const FVector RequestedAdjustment = GetPenetrationAdjustment(OutHit);
		if (ResolvePenetration(RequestedAdjustment, OutHit, NewRotation))
		{
			// Retry original move
			bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit, Teleport);
		}
	}

	return bMoveResult;
}

bool FMesaMovementSimulation::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport) const
{
	if (UpdatedComponent)
	{
		const FVector NewDelta = Delta;
		return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
	}

	return false;
}


FTransform FMesaMovementSimulation::GetUpdateComponentTransform() const
{
	if (ensure(UpdatedComponent))
	{
		return UpdatedComponent->GetComponentTransform();		
	}
	return FTransform::Identity;
}

bool IsExceedingMaxSpeed(const FVector& Velocity, float InMaxSpeed)
{
	InMaxSpeed = FMath::Max(0.f, InMaxSpeed);
	const float MaxSpeedSquared = FMath::Square(InMaxSpeed);
	
	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;
	return (Velocity.SizeSquared() > MaxSpeedSquared * OverVelocityPercent);
}

FVector ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit)
{
	// Commented out original plane project, which is the original way that UE handles movement
	// against surfaces, you end up being clamped if you walk into a wall which breaks surfing.
	// return (FVector::VectorPlaneProject(Delta, Normal) * Time;

	// Normalize Plane Projected Delta, then Multiply it by Speed
	return (FVector::VectorPlaneProject(Delta, Normal).GetSafeNormal() * Delta.Size()) * Time;
}

void TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal)
{
	FVector Delta = OutDelta;
	const FVector HitNormal = Hit.Normal;

	if ((OldHitNormal | HitNormal) <= 0.f) //90 or less corner, so use cross product for direction
	{
		const FVector DesiredDir = Delta;
		FVector NewDir = (HitNormal ^ OldHitNormal);
		NewDir = NewDir.GetSafeNormal();
		Delta = (Delta | NewDir) * (1.f - Hit.Time) * NewDir;
		if ((DesiredDir | Delta) < 0.f)
		{
			Delta = -1.f * Delta;
		}
	}
	else //adjust to new wall
	{
		const FVector DesiredDir = Delta;
		Delta = ComputeSlideVector(Delta, 1.f - Hit.Time, HitNormal, Hit);
		if ((Delta | DesiredDir) <= 0.f)
		{
			Delta = FVector::ZeroVector;
		}
		else if ( FMath::Abs((HitNormal | OldHitNormal) - 1.f) < KINDA_SMALL_NUMBER )
		{
			// we hit the same wall again even after adjusting to move along it the first time
			// nudge away from it (this can happen due to precision issues)
			Delta += HitNormal * 0.01f;
		}
	}

	OutDelta = Delta;
}

float FMesaMovementSimulation::SlideAlongSurface(const FVector& Delta, float Time, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PercentTimeApplied = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit, ETeleportType::None);

		//DrawDebugLine(
		//	UpdatedComponent->GetWorld(),
		//	GetUpdateComponentTransform().GetLocation(),
		//	SlideDelta + GetUpdateComponentTransform().GetLocation(),
		//	FColor(1.f, 1.f, 0.f, 0.f),
		//	false,
		//	1.f,
		//	255,
		//	5.f
		//);

		const float FirstHitPercent = Hit.Time;
		PercentTimeApplied = FirstHitPercent;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (bHandleImpact)
			{
				// !HandleImpact(Hit, FirstHitPercent * Time, SlideDelta);
			}
		
			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, Hit, OldHitNormal);
		
			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit, ETeleportType::None);
				const float SecondHitPercent = Hit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;
		
				// Notify second impact
				if (bHandleImpact && Hit.bBlockingHit)
				{
					// !HandleImpact(Hit, SecondHitPercent * Time, SlideDelta);
				}
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}

void FMesaMovementSimulation::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MesaMovementStateTypes>& Input, const TNetSimOutput<MesaMovementStateTypes>& Output)
{
	*Output.Sync = *Input.Sync;

	UE_LOG(LogTemp, Display, TEXT("MOVE MS SIMULATION - %f"), (float)TimeStep.StepMS);

	//FTransform CachedLastMove = GetUpdateComponentTransform(); // Cache the last move for extrapolation based on speed.
	const float DeltaSeconds = (float)TimeStep.StepMS / 1000.f;

	// --------------------------------------------------------------
	//	Rotation Update
	//	We do the rotational update inside the movement sim so that things like server side teleport will work.
	//	(We want rotation to be treated the same as location, with respect to how its updated, corrected, etc).
	//	In this simulation, the rotation update isn't allowed to "fail". We don't expect the collision query to be able to fail the rotational update.
	// --------------------------------------------------------------

	Output.Sync->Rotation.Yaw += Input.Cmd->YawInput * DeltaSeconds;
	Output.Sync->Rotation.Normalize();

	UE_LOG(LogTemp, Display, TEXT("SIM YAW - %f"), Output.Sync->Rotation.Yaw);

	const FQuat OutputQuat = Output.Sync->Rotation.Quaternion();
	   	
	// --------------------------------------------------------------
	// Calculate Output.Sync->RelativeVelocity based on Input
	// --------------------------------------------------------------
	{
		Velocity = Input.Sync->Velocity;
		PlayerRotation = Input.Sync->Rotation;
		MovementInput = Input.Cmd->MovementInput;
		bPendingJump = Input.Cmd->bJumpPressed;
		
		TraceForGround();

		switch(MovementType) // Select Movetype
		{
			case EMovementType::Walking:
				WalkMove(DeltaSeconds);
				break;
			case EMovementType::Falling:
				AirMove(DeltaSeconds);
				break;
			case EMovementType::Flying:
				FlyMove(DeltaSeconds);
				break;
		}

		// Finally, output velocity that we calculated
		Output.Sync->Velocity = Velocity;
		
		if (FMesaMovementSimulation::ForceMispredict)
		{
			Output.Sync->Velocity += ForceMispredictVelocityMagnitude;
			ForceMispredict = false;
		}
	}

	FVector Delta = Output.Sync->Velocity * DeltaSeconds;

	if (!Delta.IsNearlyZero(1e-6f))
	{
		// Naughty as fuck method that worked before to make surfing work on UMovementComponent, doesn't work here.
		//FVector VelocityDelta = (GetUpdateComponentTransform().GetLocation() - CachedLastMove.GetLocation());
		//Output.Sync->Velocity = VelocityDelta.GetSafeNormal() * Velocity.Size();

		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, OutputQuat, true, Hit, ETeleportType::None);

		if (Hit.IsValidBlockingHit())
		{
			// Try to slide the remaining distance along the surface.
			SlideAlongSurface(Delta, 1.f-Hit.Time, OutputQuat, Hit.Normal, Hit, true);
		}
	}

	const FTransform UpdateComponentTransform = GetUpdateComponentTransform();
	Output.Sync->Location = UpdateComponentTransform.GetLocation();

	// Note that we don't pull the rotation out of the final update transform. Converting back from a quat will lead to a different FRotator than what we are storing
	// here in the simulation layer. This may not be the best choice for all movement simulations, but is ok for this one.
}

void FMesaMovementSimulation::WalkMove(float DeltaTime)
{
	FVector WishDirection, WishVelocity;
	float WishSpeed;

	if(CheckJump()) // Check if we initiated a jump & swap to AirMove
	{
		// Play Jump SFX
		//UFMODBlueprintStatics::PlayEventAttached(
		//	UMesaGameData::Get().JumpAudioEvent.LoadSynchronous(), // @TODO, Dynamic Load.
		//	UpdatedComponent,
		//	NAME_None,
		//	FVector::ZeroVector,
		//	EAttachLocation::SnapToTarget,
		//	false,
		//	true,
		//	true);

		AirMove(DeltaTime);
		return;
	}

	ApplyFriction(DeltaTime);

	// ClipVelocty here should project the forward and right directions onto the ground plane
	// however I think we can just use FMath's vector projection here. We want to project our
	// movement to the floor for walking up and down ramps.

	WishVelocity = PlayerRotation.RotateVector(MovementInput).GetSafeNormal(); // Normalize clamps input to stop doubling while holding W + A ect.
	WishVelocity.Z = 0.f;

	WishDirection = WishVelocity;
	WishSpeed = WishDirection.Size();

	if(WishSpeed != 0.f)
	{
		WishVelocity *= MesaMovementConfig::MovementSpeed / WishSpeed;
		WishSpeed = MesaMovementConfig::MovementSpeed;
	}

	Velocity.Z = 0;
	Accelerate(DeltaTime, WishDirection, WishSpeed, MesaMovementConfig::Acceleration);
	Velocity.Z = 0;

	// Base Velocity can be used for standing on treadmills ect.
	// Velocity += BaseVelocity;

	if(Velocity.Size() < 1.f) // Nullify Velocity if it's nearly dead. (Probably not necessary)
	{
		Velocity = FVector::ZeroVector;
		return;
	}
}

void FMesaMovementSimulation::AirMove(float DeltaTime)
{
	FVector WishDirection, WishVelocity;
	float WishSpeed;

	ApplyFriction(DeltaTime);

	WishVelocity = PlayerRotation.RotateVector(MovementInput).GetSafeNormal();
	WishVelocity.Z = 0.f;

	WishDirection = WishVelocity;
	WishSpeed = WishDirection.Size();
	WishDirection.Normalize();

	if(WishSpeed != 0.f)
	{
		WishVelocity *= MesaMovementConfig::MovementSpeed / WishSpeed;
		WishSpeed = MesaMovementConfig::MovementSpeed;
	}

	Accelerate(DeltaTime, WishDirection, WishSpeed, MesaMovementConfig::AirAcceleration); // Normal clamps movement to stop doubling.

	// Base Velocity can be used for standing on treadmills ect.
	// Velocity += BaseVelocity;

	// Apply Gravity, we don't use UMovementComponent::GetGravityZ because it expects players to have the
	// same gravity as Physics Objects (Feels bad), However this means Physics Volumes don't affect Players.
	Velocity.Z -= MesaMovementConfig::Gravity * DeltaTime;
}

void FMesaMovementSimulation::FlyMove(float DeltaTime)
{
	ApplyFriction(DeltaTime);
	Accelerate(DeltaTime, PlayerRotation.RotateVector(MovementInput).GetSafeNormal(), MesaMovementConfig::MovementSpeed, MesaMovementConfig::FlightAcceleration); // Normal clamps movement to stop doubling.
}

bool FMesaMovementSimulation::CheckJump()
{
	if(!bPendingJump) // We aren't jumping.
	{
		return false;
	}

	//AMesaPawn* OwnerPawn = GetPawnOwner<AMesaPawn>();
	//if(OwnerPawn && OwnerPawn->bIsDead) // Cancel jump if dead.
	//{
	//	return false;
	//}

	GroundTrace = {};
	MovementType = EMovementType::Falling;
	Velocity.Z = MesaMovementConfig::JumpSpeed;
	return true;
}

// Handles user acceleration input (Quake 2 Style Acceleration)
void FMesaMovementSimulation::Accelerate(float DeltaTime, FVector WishDirection, float WishSpeed, float Acceleration)
{
	float AddSpeed, AccelerationSpeed, CurrentSpeed;

	CurrentSpeed = Velocity | WishDirection;					// See if we are changing direction a bit
	AddSpeed = WishSpeed - CurrentSpeed;						// Reduce wishspeed by the amount of veer.
	if(AddSpeed <= 0) 											// If not going to add any speed, done.
	{
		return;
	}

	AccelerationSpeed = Acceleration * DeltaTime * WishSpeed;	// Determine amount of acceleration.
	if(AccelerationSpeed > AddSpeed)							// Cap at addspeed
	{
		AccelerationSpeed = AddSpeed;
	}

	Velocity += AccelerationSpeed * WishDirection;				// Adjust velocity.
}

void FMesaMovementSimulation::ApplyFriction(float DeltaTime)
{
	float Speed, NewSpeed, Control, Drop;

#if QUAKESTYLE // QUAKE 3 ARENA STYLE
	if(MovementType == EMovementType::Walking)
	{
		Velocity.Z = 0.f; // Ignore slope movement.
	}

	Speed = Velocity.Size(); // Calculate speed.
	if(Speed < 1.f) // If too slow, return.
	{
		Velocity = FVector(FVector2D(0.f), Velocity.Z);
		return;
	}

	Drop = 0.f;

	if(MovementType == EMovementType::Walking) // Apply ground friction
	{
		Control = Speed < MesaMovementConfig::StopSpeed ? MesaMovementConfig::StopSpeed : Speed;
		Drop += Control * MesaMovementConfig::Friction * DeltaTime;
	}

	if(MovementType == EMovementType::Flying)
	{
		Drop += Speed * MesaMovementConfig::FlightFriction * DeltaTime;
	}

	NewSpeed = Speed - Drop; // Scale the velocity
	if(NewSpeed < 0.f)
	{
		NewSpeed = 0.f;
	}
	NewSpeed /= Speed;
	Velocity *= NewSpeed;
#else // SOURCE STYLE
	float Friction;

	Speed = Velocity.Size(); 	// Calculate speed.
	if(Speed < 0.1f) 			// If too slow, return.
	{
		return;
	}

	Drop = 0.f;

	if(MovementType == EMovementType::Walking)
	{
		Friction = MesaMovementConfig::Friction;

		// Bleed of some speed, but if we have less than the bleed threshold, bleed the threshold value.
		Control = (Speed < MesaMovementConfig::StopSpeed) ? MesaMovementConfig::StopSpeed : Speed;
		Drop += Control * Friction * DeltaTime;
	}

	NewSpeed = Speed - Drop; 	// Scale the velocity.
	if(NewSpeed < 0)
	{
		NewSpeed = 0;
	}

	if(NewSpeed != Speed)
	{
		NewSpeed /= Speed; 		// Determine proportion of old speed we are using.
		Velocity *= NewSpeed; 	// Adjust velocity according to proportion.
	}
#endif
}

void FMesaMovementSimulation::TraceForGround()
{
	UCapsuleComponent* OwnerCapsule = Cast<UCapsuleComponent>(UpdatedComponent);
	FVector CapsuleOrigin = OwnerCapsule->GetComponentLocation() - OwnerCapsule->GetScaledCapsuleHalfHeight();
	
	UpdatedComponent->GetWorld()->LineTraceSingleByChannel( // Trace down 1 unit for ground
		GroundTrace,
		CapsuleOrigin,
		CapsuleOrigin - FVector(0.f, 0.f, 1.f),
		ECollisionChannel::ECC_Visibility
	);
	MovementType = GroundTrace.bBlockingHit ? EMovementType::Walking : EMovementType::Falling;
}