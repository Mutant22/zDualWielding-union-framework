#include <ZenGin/zGothicAPI.h>

namespace GOTHIC_NAMESPACE {
	class DualWielding {
	public:
		static const char* NPC_MDS_DUALWIELDING;

		static const char* NPC_NODE_LEFTSWORD;
		static const char* NPC_NODE_LEFTHANDSWORD;

		DualWielding(oCNpc* Npc);
		~DualWielding() {}
		
		bool HasLeftWeaponSlots() const;
		void CreateLeftWeaponSlots() const;
		void CreateNode(zCModelNodeInst* TemplateNode, const zSTRING& NodeName, zMAT4 NodeTrafo) const;
		void LoadWeaponState() const;

		void EquipDualWeapons(oCItem* RightHandWeapon, oCItem* LeftHandWeapon) const;
		void UnequipLeftWeapon() const;
		void UnequipRightWeapon() const;
		oCItem* GetEquippedLeftSword() const;
		oCItem* GetLeftSwordInHand() const;

		void ChangeWeaponMode(zSTRING const& NewWeaponMode, int FromFightMode) const;
		void DrawSwords() const;
		void SheathSwords() const;
		oCItem* GetWeaponForDamage();
		void DropWeapons(bool WasInFightMode, oCItem* RightSword, oCItem* LeftSword);

		void ApplyDualAnimations() const;
		void RemoveDualAnimations() const;

		static bool IsWeaponForDualWielding(oCItem* Weapon);

	private:
		oCNpc* Npc;
		static oCItem* CombinedSword;
	};
}
