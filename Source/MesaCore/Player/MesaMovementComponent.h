// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "MesaMovementTypes.h"
#include "Templates/PimplPtr.h"
#include "MesaMovementComponent.generated.h"

class UCapsuleComponent;
struct FMesaMovementInputCmd;
struct FMesaMovementSyncState;
struct FMesaMovementAuxState;
class FMesaMovementSimulation;

/*
	Base Component for Game Movement designed to be a lightweight VR alternative to CMC.
	Uses Network Prediction Plugin for replication and does not derive from UMovementComponent.
*/ 
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESACORE_API UMesaMovementComponent : public UNetworkPredictionComponent
{
	GENERATED_BODY()
public:	

	UMesaMovementComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:

	virtual void InitializeComponent() override;
	virtual void OnRegister() override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;

	// Used by NetworkPrediction driver for physics interpolation case
	UPrimitiveComponent* GetPhysicsPrimitiveComponent() const { return UpdatedPrimitive; }

protected:

	// Basic "Update Component/Ticking"
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	virtual void UpdateTickRegistration();

	UPROPERTY()
	USceneComponent* UpdatedComponent = nullptr;

	UPROPERTY()
	UPrimitiveComponent* UpdatedPrimitive = nullptr;

public:

	// Forward input producing event to someone else (probably the owning actor)
	DECLARE_DELEGATE_TwoParams(FProduceMesaInput, const int32 /*SimTime*/, FMesaMovementInputCmd& /*Cmd*/)
	FProduceMesaInput ProduceInputDelegate;

	// --------------------------------------------------------------------------------
	// NP Driver
	// --------------------------------------------------------------------------------

	// Get latest local input prior to simulation step
	void ProduceInput(const int32 DeltaTimeMS, FMesaMovementInputCmd* Cmd);

	// Restore a previous frame prior to resimulating
	void RestoreFrame(const FMesaMovementSyncState* SyncState, const FMesaMovementAuxState* AuxState);

	// Take output for simulation
	void FinalizeFrame(const FMesaMovementSyncState* SyncState, const FMesaMovementAuxState* AuxState);

	// Seed initial values based on component's state
	void InitializeSimulationState(FMesaMovementSyncState* Sync, FMesaMovementAuxState* Aux);

protected:

	// Network Prediction
	virtual void InitializeNetworkPredictionProxy();
	TPimplPtr<FMesaMovementSimulation> OwnedMovementSimulation; // If we instantiate the sim in InitializeNetworkPredictionProxy, its stored here
	FMesaMovementSimulation* ActiveMovementSimulation = nullptr; // The sim driving us, set in InitMesaMovementSimulation. Could be child class that implements InitializeNetworkPredictionProxy.

	void InitMesaMovementSimulation(FMesaMovementSimulation* Simulation);

	static float GetDefaultMaxSpeed();

private:

	/** Transient flag indicating whether we are executing OnRegister(). */
	bool bInOnRegister = false;
	
	/** Transient flag indicating whether we are executing InitializeComponent(). */
	bool bInInitializeComponent = false;

	UPROPERTY(Transient) // Used to avoid recalculating velocity when true.
	bool bPositionCorrected 				= false;

	UPROPERTY(Transient)
	UCapsuleComponent*	OwnerCapsule 		= nullptr;

	FHitResult			GroundTrace;
	EMovementType 		MovementType 		= EMovementType::Falling;
	bool				bPendingJump 		= false;
};
