// Copyright Snaps 2022, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MesaMovementTypes.h"
#include "MesaMovementComponent.h"
#include "MesaMovementSimulation.h"
#include "MesaCoreTypes.h"
#include "InputActionValue.h"
#include "MesaPawn.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UInputMappingContext;

/*
	AMesaPawn is a lightweight alternative to ACharacter.
*/ 
UCLASS(ABSTRACT)
class MESACORE_API AMesaPawn : public APawn
{
	GENERATED_BODY()
public:

	AMesaPawn();

	UFUNCTION(BlueprintPure, Category = "Movement")
	UMesaMovementComponent* GetMesaPawnMovement() const { return MovementComponent; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual UInputMappingContext* 	GetInputMappingContext();
	virtual EMesaInputPriority 		GetInputPriority() { return EMesaInputPriority::Desktop; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void StopJump();

	bool bJumpPressed = false;

//////////////////////////////////////////////////////////////////////
//	~Begin Movement Netcode
//////////////////////////////////////////////////////////////////////

	friend class UMesaMovementComponent;

	/** Actor will behave like autonomous proxy even though not posessed by an APlayercontroller. To be used in conjuction with InputPreset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Automation")
	bool bFakeAutonomousProxy = false;

	void ProduceInput(const int32 DeltaMS, FMesaMovementInputCmd& Cmd);

//////////////////////////////////////////////////////////////////////
//	~End Movement Netcode
//////////////////////////////////////////////////////////////////////

protected:

	UPROPERTY()
	UInputMappingContext* InputMappingContext;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UCapsuleComponent* CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	USkeletalMeshComponent* BodyMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UMesaMovementComponent* MovementComponent;

	bool bIsDead = false;
	FVector2D LastLookInput;
};
