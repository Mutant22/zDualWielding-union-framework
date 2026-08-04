#include <ZenGin/zGothicAPI.h>

namespace GOTHIC_NAMESPACE {
	class DualWielding {
	public:
		static const char* NPC_MDS_DUALWIELDING; //< animation to use for dual wielding

		static const char* NPC_NODE_LEFTSWORD; //< node on back for second sword
		static const char* NPC_NODE_LEFTHANDSWORD; //< node in hand for second sword, also does damage, if specified in MDS animations

		DualWielding(oCNpc* Npc);
		~DualWielding() {}

		/**
		 * As we only have one animation for dual wielding, NPC must be master to do so
		 * Or some AIVAR could represent the knowledge of dual wielding
		 * Should be configurable in Gothic.ini
		**/
		bool CanDualWield() const;
		
		bool HasLeftWeaponSlots() const;
		void CreateLeftWeaponSlots() const;
		void CreateNode(zCModelNodeInst* TemplateNode, const zSTRING& NodeName, zMAT4 NodeTrafo) const;
		void LoadWeaponState() const;

		void EquipDualWeapons(oCItem* RightHandWeapon, oCItem* LeftHandWeapon) const;
		void UnequipLeftWeapon() const;
		void UnequipRightWeapon(oCItem* RightSwordHint = nullptr) const;
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
