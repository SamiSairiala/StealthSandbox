#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StealthSandboxCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAIPerceptionStimuliSourceComponent;
struct FInputActionValue;
class USpotLightComponent;
class AWeaponPickup;
class AAmmoPickup;
class UUserWidget;

UCLASS(config = Game)
class STEALTHSANDBOX_API AStealthSandboxCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthSandboxCharacter();

	// Called by enemy AI when the player is attacked.
	UFUNCTION(BlueprintCallable, Category = "Health")
		void TakeDamageFromEnemy(float DamageAmount);

	// Used by enemy AI so it does not keep attacking a dead player.
	UFUNCTION(BlueprintCallable, Category = "Health")
		bool IsDead() const;

	// Gives the player a pistol and optional starting ammo.
	// Pickups should call this instead of directly changing weapon variables.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
		void GivePistol(int32 StartingAmmo, bool bAutoEquip);

	// Adds reserve pistol ammo to the player's inventory.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
		void AddPistolAmmo(int32 AmmoAmount);

	// Equips the pistol if the player owns it.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
		void EquipPistol();

	// Unequips the pistol and returns the player to fists/melee.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
		void UnequipPistol();

	// Used by UI to decide if the Equip button should be available.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
		bool CanEquipPistol() const;

	// The player starts without a gun.
	// If false, left click performs a quiet melee attack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
		bool bHasPistol = false;

	// If true, left click uses pistol behavior instead of unarmed melee.
	// TODO: this can become a real equipped item slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
		bool bPistolEquipped = false;

	// Simple ammo inventory.
	// TODO: this can move into a real inventory component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory-Ammo")
		int32 PistolAmmoInInventory = 0;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		float GetCurrentHealth() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		FString GetWeaponDisplayName() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		int32 GetPistolAmmoInMagazine() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		int32 GetPistolMagazineSize() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		int32 GetPistolAmmoInInventory() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		bool IsReloading() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		bool HasPistol() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		bool IsPistolEquipped() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
		TSubclassOf<UUserWidget> PlayerHUDClass;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		FText GetFeedbackMessageText() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
		bool HasFeedbackMessage() const;

	UPROPERTY()
		TObjectPtr<UUserWidget> PlayerHUDInstance;

	// Inventory UI

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory UI")
		TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY()
		TObjectPtr<UUserWidget> InventoryWidgetInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory UI")
		bool bInventoryOpen = false;

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
		void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
		void OpenInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
		void CloseInventory();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
		bool HasFocusedInteractable() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
		FText GetInteractionPromptText() const;



protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
		TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
		TObjectPtr<UCameraComponent> TopDownCamera;

	// AI Perception Source
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
		TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;

	// Input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> ShootAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> InteractAction;

	// Mouse look for FPS camera control.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> CameraRotateHoldAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> CameraRotateAction;

	// Combat



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Pistol")
		float ShootRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Pistol")
		float PistolDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Melee")
		float MeleeRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Melee")
		float MeleeDamage = 8.0f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Pistol")
		int32 PistolAmmoInMagazine = 0;

	// TODO: If decide to implement modification system to weapons this can be edited.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Pistol")
		int32 PistolMagazineSize = 8;

	// Reload settings. // These might be later migrated to be in their own weapon script.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat-Pistol")
		float ReloadTime = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat-Pistol")
		bool bIsReloading = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat-Pistol")
		float ReloadTimer = 0.0f;

	// Health

	// Simple player health for the prototype.
	//TODO: this can become a full survival health/injury system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
		float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
		float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
		bool bIsDead = false;


	// Aiming
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimDeadZone = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimInterpSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimPlaneZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimStopAngle = 1.5f;

	// FPS Look
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS")
		float MouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS")
		float MinPitch = -80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS")
		float MaxPitch = 80.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS")
		float CurrentPitch = 0.0f;

	// Interaction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
		float InteractionRange = 250.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
		TObjectPtr<AActor> FocusedInteractableActor = nullptr;

	// Camera Rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraPitch = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraRotationSpeed = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera Rotation")
		bool bIsRotatingCamera = false;

	// Player Vision
	// 

	// A forward-facing light cone used to show what the player can clearly see.
	// Since the player already rotates toward the mouse, this naturally follows aim direction.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vision")
		TObjectPtr<USpotLightComponent> VisionLight;

	// Main brightness of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionLightIntensity = 80000.0f;

	// How far the player can see.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionLightRange = 1800.0f;

	// Bright center of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionInnerConeAngle = 18.0f;

	// Outer soft edge of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionOuterConeAngle = 38.0f;

	// Slight downward tilt so the light hits the floor in front of the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionPitch = -20.0f;

	// Visibility Cone	
	//

	// If true, enemies outside the player's view cone are hidden.
	// This gives a Project Zomboid style "you cannot see behind you" effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		bool bUseEnemyVisibilityCone = true;

	// How far the player can see enemies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float EnemyVisionDistance = 3000.0f;

	// Total vision angle in front of the player.
	// Example: 110 means 55 degrees left and 55 degrees right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float EnemyVisionAngle = 110.0f;

	// Very close enemies are visible even behind the player.
	// This creates a "hearing / presence sense" so zombies chasing behind you do not vanish completely.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float CloseAwarenessDistance = 650.0f;

	// If true, walls block enemy visibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		bool bEnemyVisionUsesLineOfSight = true;

	// If true, pickups are also hidden outside vision.
	// This makes loot discovery follow the same rules as enemy visibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		bool bUsePickupVisibilityCone = true;

	// Temporary HUD feedback message.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD Feedback")
		FText FeedbackMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD Feedback")
		float FeedbackMessageTimer = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Feedback")
		float FeedbackMessageDuration = 2.0f;

	void UpdatePickupVisibility();
	bool CanSeeActorWithVisionRules(AActor* TargetActor, float MaxDistance) const;

	void UpdateEnemyVisibility();
	bool CanSeeEnemy(AActor* EnemyActor) const;

	void ApplyVisionLightSettings();
	void SanitizePrototypeDefaults();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Attack();
	void Shoot();
	void MeleeAttack();
	void Interact();
	void UpdateInteractionTrace();
	void ReloadPistol();
	void FinishReload();
	void UpdateReload(float DeltaTime);
	void AimAtMouseCursor();
	void ShowFeedbackMessage(const FText& Message);
	void UpdateFeedbackMessage(float DeltaTime);

	void StartCameraRotate();
	void StopCameraRotate();
	void RotateCamera(const FInputActionValue& Value);
	void UpdateCameraRotation();

	bool GetMouseAimPoint(FVector& OutAimPoint) const;
	void HandleDeath();

};