// Copyright Snaps 2022, All Rights Reserved.

#include "MesaMovementComponent.h"
#include "MesaMovementSimulation.h"
#include "MesaPawn.h"
#include "MesaPlayerController.h"
#include "MesaCoreMacros.h"

#include "Components/CapsuleComponent.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyWrite.h"

float UMesaMovementComponent::GetDefaultMaxSpeed() { return 1200.f; }

// ----------------------------------------------------------------------------------------------------------
//	FMesaMovementModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FMesaMovementModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = FMesaMovementSimulation;
	using StateTypes = MesaMovementStateTypes;
	using Driver = UMesaMovementComponent;

	static const TCHAR* GetName() { return TEXT("MesaMovement"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers; }
};

NP_MODEL_REGISTER(FMesaMovementModelDef);

/////////////////////////////////////////////////////////////////////////////////////////////////////

UMesaMovementComponent::UMesaMovementComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
	
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);
}

void UMesaMovementComponent::InitializeComponent()
{
	TGuardValue<bool> InInitializeComponentGuard(bInInitializeComponent, true);

	// RootComponent is null in OnRegister for blueprint (non-native) root components.
	if (!UpdatedComponent)
	{
		// Auto-register owner's root component if found.
		if (AActor* MyActor = GetOwner())
		{
			if (USceneComponent* NewUpdatedComponent = MyActor->GetRootComponent())
			{
				SetUpdatedComponent(NewUpdatedComponent);
			}
			else
			{
				ensureMsgf(false, TEXT("No root component found on %s. Simulation initialization will most likely fail."), *GetPathNameSafe(MyActor));
			}
		}
	}
	
	Super::InitializeComponent();
}

void UMesaMovementComponent::OnRegister()
{
	TGuardValue<bool> InOnRegisterGuard(bInOnRegister, true);

	UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	Super::OnRegister();

	const UWorld* MyWorld = GetWorld();
	if (MyWorld && MyWorld->IsGameWorld())
	{
		USceneComponent* NewUpdatedComponent = UpdatedComponent;
		if (!UpdatedComponent)
		{
			// Auto-register owner's root component if found.
			AActor* MyActor = GetOwner();
			if (MyActor)
			{
				NewUpdatedComponent = MyActor->GetRootComponent();
			}
		}

		SetUpdatedComponent(NewUpdatedComponent);
	}
}

void UMesaMovementComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	// Super may start up the tick function when we don't want to.
	UpdateTickRegistration();

	// If the owner ticks, make sure we tick first. This is to ensure the owner's location will be up to date when it ticks.
	AActor* Owner = GetOwner();
	
	if (bRegister && PrimaryComponentTick.bCanEverTick && Owner && Owner->CanEverTick())
	{
		Owner->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
	}
}

void UMesaMovementComponent::UpdateTickRegistration()
{
	const bool bHasUpdatedComponent = (UpdatedComponent != NULL);
	SetComponentTickEnabled(bHasUpdatedComponent && bAutoActivate);
}


void UMesaMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (UpdatedComponent && UpdatedComponent != NewUpdatedComponent)
	{
		// remove from tick prerequisite
		UpdatedComponent->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick); 
	}

	// Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
	UpdatedComponent = IsValid(NewUpdatedComponent) ? NewUpdatedComponent : NULL;
	UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);

	// Assign delegates
	if (UpdatedComponent && !UpdatedComponent->IsPendingKill())
	{
		// force ticks after movement component updates
		UpdatedComponent->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick); 
	}

	UpdateTickRegistration();
}

void UMesaMovementComponent::InitializeNetworkPredictionProxy()
{
	OwnedMovementSimulation = MakePimpl<FMesaMovementSimulation>();
	InitMesaMovementSimulation(OwnedMovementSimulation.Get());
	
	NetworkPredictionProxy.Init<FMesaMovementModelDef>(GetWorld(), GetReplicationProxies(), OwnedMovementSimulation.Get(), this);
}

void UMesaMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ENetRole OwnerRole = GetOwnerRole();
	static bool bDrawSimLocation = true;
	static bool bDrawPresentationLocation = true;
}

void UMesaMovementComponent::ProduceInput(const int32 DeltaTimeMS, FMesaMovementInputCmd* Cmd)
{
	// This isn't ideal. It probably makes sense for the component to do all the input binding rather.
	ProduceInputDelegate.ExecuteIfBound(DeltaTimeMS, *Cmd);
}

void UMesaMovementComponent::RestoreFrame(const FMesaMovementSyncState* SyncState, const FMesaMovementAuxState* AuxState)
{
	FTransform Transform(SyncState->Rotation.Quaternion(), SyncState->Location, UpdatedComponent->GetComponentTransform().GetScale3D() );
	UpdatedComponent->SetWorldTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	UpdatedComponent->ComponentVelocity = SyncState->Velocity;
}

void UMesaMovementComponent::FinalizeFrame(const FMesaMovementSyncState* SyncState, const FMesaMovementAuxState* AuxState)
{
	// The component will often be in the "right place" already on FinalizeFrame, so a comparison check makes sense before setting it.
	if (UpdatedComponent->GetComponentLocation().Equals(SyncState->Location) == false || UpdatedComponent->GetComponentQuat().Rotator().Equals(SyncState->Rotation, FMesaMovementSimulation::ROTATOR_TOLERANCE) == false)
	{
		RestoreFrame(SyncState, AuxState);
	}
}

void UMesaMovementComponent::InitializeSimulationState(FMesaMovementSyncState* Sync, FMesaMovementAuxState* Aux)
{
	npCheckSlow(UpdatedComponent);
	npCheckSlow(Sync);
	npCheckSlow(Aux);

	Sync->Location = UpdatedComponent->GetComponentLocation();
	Sync->Rotation = UpdatedComponent->GetComponentQuat().Rotator();
}

// Init function. This is broken up from ::InstantiateNetworkedSimulation and templated so that subclasses can share the init code
void UMesaMovementComponent::InitMesaMovementSimulation(FMesaMovementSimulation* Simulation)
{
	check(UpdatedComponent);
	check(ActiveMovementSimulation == nullptr); // Reinstantiation not supported
	ActiveMovementSimulation = Simulation;

	Simulation->SetComponents(UpdatedComponent, UpdatedPrimitive);
}