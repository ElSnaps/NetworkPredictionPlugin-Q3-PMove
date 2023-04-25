// Copyright Snaps 2022, All Rights Reserved.

#include "MesaPawn.h"
#include "Player/MesaPlayerController.h"
#include "System/MesaGameData.h"
#include "MesaCoreMacros.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

//#include "FMODEvent.h"
//#include "FMODBlueprintStatics.h"

AMesaPawn::AMesaPawn()
{
	SetReplicatingMovement(false);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComponent->SetCapsuleRadius(16.f);
	CapsuleComponent->SetCapsuleHalfHeight(88.f);
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->bDynamicObstacle = true;
	SetRootComponent(CapsuleComponent);

	BodyMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body Mesh"));
	BodyMeshComponent->SetupAttachment(CapsuleComponent);

	MovementComponent = CreateDefaultSubobject<UMesaMovementComponent>(TEXT("Mesa Movement"));
}

void AMesaPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (auto* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UInputAction* MoveAction = UMesaGameData::Get().InputActionMove.LoadSynchronous();
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);

		UInputAction* LookAction = UMesaGameData::Get().InputActionLook.LoadSynchronous();
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);

		UInputAction* JumpAction = UMesaGameData::Get().InputActionJump.LoadSynchronous();
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::StopJump);
	}
}

// Setup Enhanced Input Mappings.
void AMesaPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (AMesaPlayerController* PC = Cast<AMesaPlayerController>(GetController()))
	{
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings(); // PawnClientRestart can run multiple times, clear out leftover mappings.

			check(GetInputMappingContext()); // Invalid Input Mapping Context.
			Subsystem->AddMappingContext(GetInputMappingContext(), (int32)GetInputPriority());
		}
	}
}

UInputMappingContext* AMesaPawn::GetInputMappingContext()
{
	if(!InputMappingContext)
	{
		InputMappingContext = UMesaGameData::Get().DesktopInputMappingContext.LoadSynchronous();
	}
	return InputMappingContext;
}

void AMesaPawn::BeginPlay()
{
	Super::BeginPlay();

	// Play Spawn SFX
	//UFMODBlueprintStatics::PlayEventAttached(
	//	UMesaGameData::Get().RespawnAudioEvent.LoadSynchronous(), // @TODO, Dynamic Load.
	//	CapsuleComponent,
	//	NAME_None,
	//	FVector::ZeroVector,
	//	EAttachLocation::SnapToTarget,
	//	false,
	//	true,
	//	true);

	check(MovementComponent)
	MovementComponent->ProduceInputDelegate.BindUObject(this, &ThisClass::ProduceInput);
}

void AMesaPawn::Tick( float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Do whatever you want here. By now we have the latest movement state and latest input processed.

	if (bFakeAutonomousProxy && GetLocalRole() == ROLE_Authority)
	{
		if (GetRemoteRole() != ROLE_AutonomousProxy)
		{
			SetAutonomousProxy(true);
		}
	}
}

void AMesaPawn::Move(const FInputActionValue& Value)
{
	FVector2D MoveInput = Value.Get<FVector2D>();

	AddMovementInput( // Strafe Input
		FVector(0.f, 1.f, 0.f),
		FMath::Clamp(MoveInput.X, -1.f, 1.f)
	);

	AddMovementInput( // Forward Input
	    FVector(1.f, 0.f, 0.f),
	    FMath::Clamp(MoveInput.Y, -1.f, 1.f)
	);
}

void AMesaPawn::Look(const FInputActionValue& Value)
{
	LastLookInput = Value.Get<FVector2D>();
}

void AMesaPawn::Jump()
{
	bJumpPressed = true;
}

void AMesaPawn::StopJump()
{
	bJumpPressed = false;
}

// @TODO Move MC movement state to Aux, that way it tracks better, should fixup fucky gravity..
void AMesaPawn::ProduceInput(const int32 DeltaMS, FMesaMovementInputCmd& Cmd)
{
	if (!Controller) // no local controller, this is ok. sim proxies just use previous input when extrapolating
	{
		return;
	}

	const float DeltaTimeSeconds = (float)DeltaMS / 1000.f;
	static float LookRateYaw = 150.f;
	static float ControllerLookRateYaw = 5.0f;

	AddActorWorldRotation(FRotator(0.f, LastLookInput.X * LookRateYaw, 0.f));
	Cmd.YawInput = LastLookInput.X * LookRateYaw;

	Cmd.MovementInput = GetPendingMovementInputVector();
	Cmd.bJumpPressed = bJumpPressed;

	Internal_ConsumeMovementInputVector();
	LastLookInput = FVector2D::ZeroVector;
}