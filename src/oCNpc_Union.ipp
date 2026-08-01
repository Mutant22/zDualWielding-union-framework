#include <Union/Hook.h>
#include "DualWielding.h"

namespace GOTHIC_NAMESPACE {
	oCItem* __fastcall Hooked_oCNpc_GetWeapon(oCNpc* self, void* vtable);
	void __fastcall Hooked_oCNpc_EquipWeapon(oCNpc* self, void* vtable, oCItem* weaponToEquip);
	void __fastcall Hooked_oCNpc_SetWeaponMode2_novt(oCNpc* self, void* vtable, zSTRING const& newWeaponMode);
	void __fastcall Hooked_oCNpc_DoDie(oCNpc* self, void* vtable, oCNpc* killer);
	void __fastcall Hooked_oCNpc_DropUnconscious(oCNpc* self, void* vtable, float hitAngle, oCNpc* instigator);

	static auto Hook_oCNpc_GetWeapon_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::GetWeapon), &Hooked_oCNpc_GetWeapon, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_EquipWeapon_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::EquipWeapon), &Hooked_oCNpc_EquipWeapon, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_SetWeaponMode2_novt_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::SetWeaponMode2_novt), &Hooked_oCNpc_SetWeaponMode2_novt, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_DoDie_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::DoDie), &Hooked_oCNpc_DoDie, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_DropUnconscious_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::DropUnconscious), &Hooked_oCNpc_DropUnconscious, Union::HookType::Hook_Detours);

	// oCItem* GetWeapon() zCall( 0x007377A0 );
	oCItem* __fastcall Hooked_oCNpc_GetWeapon(oCNpc* self, void* vtable)
	{
		oCItem* Result = Hook_oCNpc_GetWeapon_Union(self, vtable);

		zCModel*         NpcModel      = self->GetModel();
		zCModelNodeInst* LongswordNode = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* LeftHandNode  = NpcModel->SearchNode(NPC_NODE_LEFTHAND);

		if (!LongswordNode || !LeftHandNode) {
			return Result;
		}

		DualWielding DualWielder(self);
		oCItem* WeaponForDamage = DualWielder.GetWeaponForDamage();
		if (WeaponForDamage) {
			return WeaponForDamage;
		}

		return Result;
	}

	// void EquipWeapon( oCItem* ) zCall( 0x0073A030 );
	void __fastcall Hooked_oCNpc_EquipWeapon(oCNpc* self, void* vtable, oCItem* WeaponToEquip)
	{
		DualWielding DualWielder(self);
		DualWielder.RemoveDualAnimations();
		
		oCItem* LeftSwordEquipped  = DualWielder.GetEquippedLeftSword();
		oCItem* RightSwordEquipped = self->GetSlotItem(NPC_NODE_SWORD);
		if (LeftSwordEquipped && RightSwordEquipped) {
			if (WeaponToEquip == LeftSwordEquipped) {
				DualWielder.UnequipLeftWeapon();

				self->EquipItem(RightSwordEquipped);
				self->PutInSlot(NPC_NODE_SWORD, RightSwordEquipped, 1);
				return;
			}

			DualWielder.UnequipRightWeapon();
			DualWielder.UnequipLeftWeapon();

			if (WeaponToEquip != RightSwordEquipped) {
				Hook_oCNpc_EquipWeapon_Union(self, vtable, WeaponToEquip);
			}

			return;
		}

		if (!RightSwordEquipped
			|| RightSwordEquipped == WeaponToEquip
			|| !DualWielder.IsWeaponForDualWielding(RightSwordEquipped)
			|| !DualWielder.IsWeaponForDualWielding(WeaponToEquip)
			) {
			Hook_oCNpc_EquipWeapon_Union(self, vtable, WeaponToEquip);
			return;
		}

		DualWielder.EquipDualWeapons(RightSwordEquipped, WeaponToEquip);
		DualWielder.ApplyDualAnimations();
	}

	// void SetWeaponMode2_novt( zSTRING const& ) zCall( 0x00738C60 );
	void __fastcall Hooked_oCNpc_SetWeaponMode2_novt(oCNpc* self, void* vtable, zSTRING const& NewWeaponMode)
	{
		DualWielding DualWielder(self);

		int FromFightMode = self->fmode;
		Hook_oCNpc_SetWeaponMode2_novt_Union(self, vtable, NewWeaponMode);

		DualWielder.ChangeWeaponMode(NewWeaponMode, FromFightMode);
	}

	// void DoDie( oCNpc* ) zCall( 0x00736760 );
	void __fastcall Hooked_oCNpc_DoDie(oCNpc* self, void* vtable, oCNpc* Killer)
	{
		DualWielding DualWielder(self);

		bool    WasInFightMode = self->fmode != 0;
		oCItem* LeftSword      = nullptr;
		if (WasInFightMode) {
			oCItem* LeftSwordInHand = DualWielder.GetLeftSwordInHand();
			if (LeftSwordInHand && DualWielding::IsWeaponForDualWielding(LeftSwordInHand)) {
				LeftSword = LeftSwordInHand;
				LeftSword->AddRef();
			}
		}

		Hook_oCNpc_DoDie_Union(self, vtable, Killer);

		if (!LeftSword) {
			return;
		}

		DualWielder.DropWeapons(true, nullptr, LeftSword);
	}

	// void DropUnconscious(float, oCNpc*) zCall(0x00735EB0);
	void __fastcall Hooked_oCNpc_DropUnconscious(oCNpc* self, void* vtable, float HitAngle, oCNpc* Instigator)
	{
		DualWielding DualWielder(self);

		bool    WasInFightMode  = self->fmode != 0;
		bool    WasDualWielding = false;
		oCItem* LeftSword       = nullptr;
		oCItem* RightSword      = nullptr;
		if (WasInFightMode) {
			oCItem* LeftSwordInHand = DualWielder.GetLeftSwordInHand();
			if (LeftSwordInHand && DualWielding::IsWeaponForDualWielding(LeftSwordInHand)) {
				WasDualWielding = true;
				LeftSword = LeftSwordInHand;
				LeftSword->AddRef();
			}
		}
		else {
			LeftSword  = DualWielder.GetEquippedLeftSword();
			RightSword = self->GetSlotItem(NPC_NODE_SWORD);
			if (LeftSword && RightSword) {
				WasDualWielding = true;
				LeftSword->AddRef();
				RightSword->AddRef();
			}
		}

		Hook_oCNpc_DropUnconscious_Union(self, vtable, HitAngle, Instigator);

		if (!WasDualWielding) {
			return;
		}

		DualWielder.DropWeapons(WasInFightMode, RightSword, LeftSword);
	}
}
