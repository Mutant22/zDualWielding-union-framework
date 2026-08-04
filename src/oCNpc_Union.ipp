#include <Union/Hook.h>
#include "DualWielding.h"

namespace GOTHIC_NAMESPACE {
	// DropVob/RemoveAmount can be re-entered, so keep the active drop state per thread and
	// chain nested calls instead of using a single global scratch slot.
	struct DropCaptureContext {
		oCNpc* npc;
		oCItem* requestedItem;
		oCItem* removedItem;
		DropCaptureContext* prev;
	};

	static thread_local DropCaptureContext* g_DropCaptureCtx = nullptr;

	oCItem* __fastcall Hooked_oCNpc_GetWeapon(oCNpc* self, void* vtable);
	void __fastcall Hooked_oCNpc_EquipWeapon(oCNpc* self, void* vtable, oCItem* weaponToEquip);
	void __fastcall Hooked_oCNpc_SetWeaponMode2_novt(oCNpc* self, void* vtable, zSTRING const& newWeaponMode);
	void __fastcall Hooked_oCNpc_DoDie(oCNpc* self, void* vtable, oCNpc* killer);
	void __fastcall Hooked_oCNpc_DropUnconscious(oCNpc* self, void* vtable, float hitAngle, oCNpc* instigator);
	int __fastcall Hooked_oCNpc_DoDropVob(oCNpc* self, void* vtable, zCVob* vobToDrop);
	oCItem* __fastcall Hooked_oCNpcInventory_RemoveAmount(oCNpcInventory* self, void* vtable, oCItem* item, int amount);

	static auto Hook_oCNpc_GetWeapon_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::GetWeapon), &Hooked_oCNpc_GetWeapon, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_EquipWeapon_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::EquipWeapon), &Hooked_oCNpc_EquipWeapon, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_SetWeaponMode2_novt_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::SetWeaponMode2_novt), &Hooked_oCNpc_SetWeaponMode2_novt, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_DoDie_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::DoDie), &Hooked_oCNpc_DoDie, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_DropUnconscious_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::DropUnconscious), &Hooked_oCNpc_DropUnconscious, Union::HookType::Hook_Detours);
	static auto Hook_oCNpc_DoDropVob_Union = Union::CreateHook(SIGNATURE_OF(&oCNpc::DoDropVob), &Hooked_oCNpc_DoDropVob, Union::HookType::Hook_Detours);
	static auto Hook_oCNpcInventory_RemoveAmount_Union = Union::CreateHook(SIGNATURE_OF(static_cast<oCItem* (oCNpcInventory::*)(oCItem*, int)>(&oCNpcInventory::Remove)), &Hooked_oCNpcInventory_RemoveAmount, Union::HookType::Hook_Detours);

	template<typename Callback>
	static void UnconsciousOrDieHandler(oCNpc* self, Callback&& callback)
	{
		if (!self->IsHuman()) {
			callback();
			return;
		}

		DualWielding DualWielder(self);

		bool    WasInFightMode  = self->fmode == NPC_WEAPON_1HS; //< when dual wielding character uses e.g. bow, we want to clear npc wapons from back
		bool    WasDualWielding = false;
		oCItem* LeftSword       = nullptr;
		oCItem* RightSword      = nullptr;
		if (WasInFightMode) {
			LeftSword = DualWielder.GetLeftSwordInHand();
			RightSword = self->GetSlotItem(NPC_NODE_RIGHTHAND);

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
			// Unequip the left weapon first so the right-side slot can be restored/cleaned up
			// without the left-hand state interfering with the engine's item transfer logic.
			DualWielder.UnequipLeftWeapon();
			DualWielder.UnequipRightWeapon();
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

				// Do not call EquipWeapon on an already equipped right weapon, because
				// vanilla EquipWeapon toggles equipped items off.
				if (RightSwordEquipped && !RightSwordEquipped->HasFlag(ITM_FLAG_ACTIVE)) {
					self->EquipItem(RightSwordEquipped);
				}
				if (RightSwordEquipped && !self->GetSlotItem(NPC_NODE_SWORD)) {
					self->PutInSlot(NPC_NODE_SWORD, RightSwordEquipped, 1);
				}

				return;
			}

			if (WeaponToEquip == RightSwordEquipped) {
				DualWielder.UnequipLeftWeapon();

				if (RightSwordEquipped && !RightSwordEquipped->HasFlag(ITM_FLAG_ACTIVE)) {
					self->EquipItem(RightSwordEquipped);
				}
				if (RightSwordEquipped && !self->GetSlotItem(NPC_NODE_SWORD)) {
					self->PutInSlot(NPC_NODE_SWORD, RightSwordEquipped, 1);
				}

				return;
			}

			DualWielder.UnequipLeftWeapon();
			// Left first keeps the right weapon's slot state stable while we hand control
			// back to vanilla equip logic or the manual right-weapon restore path.
			DualWielder.UnequipRightWeapon(RightSwordEquipped);

			if (WeaponToEquip && WeaponToEquip->HasFlag(ITM_FLAG_ACTIVE)) {
				if (!self->GetSlotItem(NPC_NODE_SWORD)) {
					self->PutInSlot(NPC_NODE_SWORD, WeaponToEquip, 1);
				}
				self->EquipItem(WeaponToEquip);
			} else {
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

	// int DoDropVob(zCVob*) zCall(0x00744DD0);
	int __fastcall Hooked_oCNpc_DoDropVob(oCNpc* self, void* vtable, zCVob* VobToDrop)
	{
		oCItem* droppedItem = VobToDrop ? VobToDrop->CastTo<oCItem>() : nullptr;
		oCItem* inInvBefore = (self && droppedItem) ? self->inventory2.IsIn(droppedItem, 1) : nullptr;
		bool isPlayer = self && ogame && self == ogame->GetSelfPlayerVob();

		// Push a temporary context for this exact drop call.
		DropCaptureContext ctx = { self, droppedItem, nullptr, g_DropCaptureCtx };
		g_DropCaptureCtx = &ctx;

		int result = Hook_oCNpc_DoDropVob_Union(self, vtable, VobToDrop);
		oCItem* removedDuringDrop = ctx.removedItem;
		g_DropCaptureCtx = ctx.prev;
		oCItem* inInvAfter = (self && droppedItem) ? self->inventory2.IsIn(droppedItem, 1) : nullptr;

		// Recovery for rare invalid state: engine reports success, but exact dropped pointer
		// is still in inventory, which can produce ghost/unpickable world items.
		if (isPlayer && droppedItem && result != 0 && inInvBefore && inInvAfter) {
			int removeAmount = droppedItem->amount > 0 ? droppedItem->amount : 1;
			if (removedDuringDrop && removedDuringDrop != droppedItem) {
				// The engine removed a different instance than the one it tried to drop, so
				// swap the inventory state back to keep one copy in inventory and one in world.
				oCItem* removedRequested = self->inventory2.RemoveByPtr(droppedItem, removeAmount);
				if (!removedRequested) {
					removedRequested = self->inventory2.Remove(droppedItem, removeAmount);
				}
				(void)removedRequested;

				if (!self->inventory2.IsIn(removedDuringDrop, 1)) {
					self->inventory2.Insert(removedDuringDrop);
				}
			} else {
				oCItem* forcedRemoved = self->inventory2.RemoveByPtr(droppedItem, removeAmount);
				if (!forcedRemoved) {
					forcedRemoved = self->inventory2.Remove(droppedItem, removeAmount);
				}
				(void)forcedRemoved;
			}
		}
		return result;
	}

	oCItem* __fastcall Hooked_oCNpcInventory_RemoveAmount(oCNpcInventory* self, void* vtable, oCItem* Item, int Amount)
	{
		oCItem* removed = Hook_oCNpcInventory_RemoveAmount_Union(self, vtable, Item, Amount);
		if (g_DropCaptureCtx && !g_DropCaptureCtx->removedItem) {
			oCNpc* owner = self ? self->GetOwner() : nullptr;
			// Only record the first removal that belongs to the active drop call.
			if (!g_DropCaptureCtx->npc || owner == g_DropCaptureCtx->npc) {
				g_DropCaptureCtx->removedItem = removed;
			}
		}

		return removed;
	}
}
