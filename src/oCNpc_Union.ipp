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

	template<typename Callback>
	static void UnconsciousOrDieHandler(oCNpc* self, Callback&& callback)
	{
		DualWielding DualWielder(self);

		bool    WasInFightMode  = self->fmode == NPC_WEAPON_1HS; //< when dual wielding character uses e.g. bow, we want to clear npc wapons from back
		bool    WasDualWielding = false;
		oCItem* LeftSword       = nullptr;
		oCItem* RightSword      = nullptr;
		if (WasInFightMode) {
			LeftSword = DualWielder.GetLeftSwordInHand();
			RightSword = self->GetSlotItem(NPC_NODE_RIGHTHAND);

			if (LeftSword) {
				Union::StringANSI::Format("zDualWielding: LeftSword: {0}\n", LeftSword->name.ToChar()).StdPrintLine();
			}
			if (RightSword) {
				Union::StringANSI::Format("zDualWielding: RightSword: {0}\n", RightSword->name.ToChar()).StdPrintLine();
			}

			if (LeftSword && RightSword && DualWielder.IsWeaponForDualWielding(LeftSword) && DualWielder.IsWeaponForDualWielding(RightSword)) {
				WasDualWielding = true;
				// references are cleared in DropWeapons
				LeftSword->AddRef();
				RightSword->AddRef();
			}
		} else {
			LeftSword  = DualWielder.GetEquippedLeftSword();
			RightSword = self->GetSlotItem(NPC_NODE_SWORD);

			if (LeftSword && RightSword && DualWielder.IsWeaponForDualWielding(LeftSword) && DualWielder.IsWeaponForDualWielding(RightSword)) {
				WasDualWielding = true;
				// references are cleared in DropWeapons
				LeftSword->AddRef();
				RightSword->AddRef();
			}
		}

		if (WasDualWielding) {
			// sometimes one of the weapons stayed "equipped", equip them explicitely before callback
			DualWielder.UnequipRightWeapon();
			DualWielder.UnequipLeftWeapon();
		}

		callback();

		if (WasDualWielding) {
			DualWielder.DropWeapons(WasInFightMode, RightSword, LeftSword);
		}
	}

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

		if (!DualWielder.CanDualWield()) {
			Hook_oCNpc_EquipWeapon_Union(self, vtable, WeaponToEquip);
			return;
		}
		
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
		UnconsciousOrDieHandler(self, [&]() {
			Hook_oCNpc_DoDie_Union(self, vtable, Killer);
		});
	}

	// void DropUnconscious(float, oCNpc*) zCall(0x00735EB0);
	void __fastcall Hooked_oCNpc_DropUnconscious(oCNpc* self, void* vtable, float HitAngle, oCNpc* Instigator)
	{
		UnconsciousOrDieHandler(self, [&]() {
			Hook_oCNpc_DropUnconscious_Union(self, vtable, HitAngle, Instigator);
		});
	}
}
