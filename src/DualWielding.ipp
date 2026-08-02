#include "DualWielding.h"

namespace GOTHIC_NAMESPACE {

	static inline void TranslateNodeTrafo(zMAT4& trafo, const zVEC3& delta)
	{
	#if ENGINE == Engine_G2
		trafo.SetTranslation(trafo.GetTranslation() + delta);
	#else
		trafo.Translate(delta);
	#endif
	}

	static inline void RemoveFromSlotCompat(oCNpc* npc, const zSTRING& slot)
	{
	#if ENGINE == Engine_G2A
		npc->RemoveFromSlot(slot, 1, 1);
	#else
		npc->RemoveFromSlot(slot, 1);
	#endif
	}

	const char* DualWielding::NPC_MDS_DUALWIELDING = "HUMANS_2X2ST3.MDS";
	const char* DualWielding::NPC_NODE_LEFTSWORD = "ZS_LEFTSWORD"; //< node on back for second sword
	const char* DualWielding::NPC_NODE_LEFTHANDSWORD = "ZS_LEFTHANDSWORD"; //< node in hand for second sword, also does damage, if specified in MDS animations

	oCItem* DualWielding::CombinedSword = nullptr; //< fake virtual sword, that combines damage when both nodes hit target

	DualWielding::DualWielding(oCNpc* Npc) : Npc(Npc)
	{
		if (!HasLeftWeaponSlots()) {
			CreateLeftWeaponSlots();
		}
	}

	bool DualWielding::CanDualWield() const
	{
		// better compatibility with G1/G2, NPC must be master
		return Npc->GetTalentSkill(oCNpcTalent::NPC_TAL_1H) >= 2;
	}

	bool DualWielding::HasLeftWeaponSlots() const
	{
		TNpcSlot*        LeftWeaponInvSlot = Npc->GetInvSlot(NPC_NODE_LEFTSWORD);
		zCModel*         NpcModel          = Npc->GetModel();
		zCModelNodeInst* LeftSwordNode     = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LeftHandSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTHANDSWORD);
		return LeftWeaponInvSlot && LeftSwordNode && LeftHandSwordNode;
	}

	void DualWielding::CreateLeftWeaponSlots() const
	{
		TNpcSlot*        LeftWeaponInvSlot = Npc->GetInvSlot(NPC_NODE_LEFTSWORD);
		zCModel*         NpcModel          = Npc->GetModel();
		zCModelNodeInst* LongswordNode     = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* LeftHandNode      = NpcModel->SearchNode(NPC_NODE_LEFTHAND);
		zCModelNodeInst* LeftSwordNode     = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LeftHandSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTHANDSWORD);

		if (!LeftWeaponInvSlot) {
			Npc->CreateInvSlot(NPC_NODE_LEFTSWORD);
			Npc->UpdateSlots();
		}

		if (!LeftSwordNode) {
			zMAT4 NodeTrafo = LongswordNode->trafo;
			NodeTrafo.PostRotateX(180.0f);
			NodeTrafo.PostRotateY(-60.0f);
			NodeTrafo.PostRotateZ(10.0f);
			TranslateNodeTrafo(NodeTrafo, zVEC3(0.0f, 35.0f, -5.0f));

			CreateNode(LongswordNode, NPC_NODE_LEFTSWORD, NodeTrafo);
		}

		if (!LeftHandSwordNode) {
			zMAT4 NodeTrafo = LeftHandNode->trafo;
			NodeTrafo.PostRotateX(180.0f);
			TranslateNodeTrafo(NodeTrafo, zVEC3(0.0f, -5.0f, 0.0f));

			CreateNode(LeftHandNode, NPC_NODE_LEFTHANDSWORD, NodeTrafo);
		}
	}

	void DualWielding::CreateNode(zCModelNodeInst* TemplateNode, const zSTRING& NodeName, zMAT4 NodeTrafo) const
	{
		zCModel*         NpcModel    = Npc->GetModel();
		zCModelNode*     NewNode     = new zCModelNode();
		zCModelNodeInst* NewNodeInst = new zCModelNodeInst();

		NewNode->nodeName = NodeName;
		NewNode->parentNode = TemplateNode->protoNode->parentNode;
		NewNode->lastInstNode = nullptr;
		NewNode->trafo = NodeTrafo;
		NewNodeInst->InitByModelProtoNode(NewNode);
		NewNodeInst->parentNode = TemplateNode->parentNode;
		NewNodeInst->trafo = NodeTrafo;
		NewNode->lastInstNode = NewNodeInst;

		NpcModel->nodeList.Insert(NewNodeInst);
	}

	void DualWielding::LoadWeaponState() const
	{
		if (!HasLeftWeaponSlots()) {
			return;
		}

		if (Npc->IsSlotFree(NPC_NODE_LEFTSWORD)) {
			return;
		}

		RemoveDualAnimations();

		zCModel*         NpcModel      = Npc->GetModel();
		zCModelNodeInst* SwordNode     = NpcModel->SearchNode(NPC_NODE_SWORD);
		zCModelNodeInst* LongswordNode = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* LeftSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);

		//moved from lower
		oCItem* LeftSwordEquipped  = Npc->GetSlotItem(NPC_NODE_LEFTSWORD);

		if (Npc->fmode == 0) {
			
			oCItem* RightSwordEquipped = Npc->GetSlotItem(NPC_NODE_SWORD);
			Npc->PutInSlot(NPC_NODE_LEFTSWORD, LeftSwordEquipped, 1);

			NpcModel->SetNodeVisual(SwordNode, nullptr, 0);
			NpcModel->SetNodeVisual(LongswordNode, RightSwordEquipped->visual, 0);
			NpcModel->SetNodeVisual(LeftSwordNode, LeftSwordEquipped->visual, 0);
		} else {
			DrawSwords();
		}

		ApplyDualAnimations();
	}

	void DualWielding::EquipDualWeapons(oCItem* RightHandWeapon, oCItem* LeftHandWeapon) const
	{
		zCModel*         NpcModel      = Npc->GetModel();
		zCModelNodeInst* LongswordNode = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* SwordNode     = NpcModel->SearchNode(NPC_NODE_SWORD);
		zCModelNodeInst* LeftSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);

		Npc->EquipItem(LeftHandWeapon);
		Npc->EquipItem(RightHandWeapon);
		Npc->PutInSlot(NPC_NODE_SWORD, RightHandWeapon, 1);
		Npc->PutInSlot(NPC_NODE_LEFTSWORD, LeftHandWeapon, 1);

		NpcModel->SetNodeVisual(SwordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LongswordNode, RightHandWeapon->visual, 0);
		NpcModel->SetNodeVisual(LeftSwordNode, LeftHandWeapon->visual, 0);
	}

	void DualWielding::UnequipLeftWeapon() const
	{
		zCModel*         NpcModel          = Npc->GetModel();
		zCModelNodeInst* LeftSwordNode     = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LongswordNode     = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		oCItem*          LeftSwordEquipped = Npc->GetSlotItem(NPC_NODE_LEFTSWORD);

		RemoveFromSlotCompat(Npc, NPC_NODE_LEFTSWORD);
		Npc->UnequipItem(LeftSwordEquipped);
		Npc->GetModel()->SetNodeVisual(LeftSwordNode, nullptr, 0);
		Npc->GetModel()->SetNodeVisual(LongswordNode, nullptr, 0);
	}

	void DualWielding::UnequipRightWeapon() const
	{
		zCModel*         NpcModel           = Npc->GetModel();
		zCModelNodeInst* LongswordNode      = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		oCItem*          RightSwordEquipped = Npc->GetSlotItem(NPC_NODE_SWORD);

		RemoveFromSlotCompat(Npc, NPC_NODE_SWORD);
		Npc->UnequipItem(RightSwordEquipped);
		Npc->GetModel()->SetNodeVisual(LongswordNode, nullptr, 0);
	}

	oCItem* DualWielding::GetEquippedLeftSword() const
	{
		return Npc->GetSlotItem(NPC_NODE_LEFTSWORD);
	}

	oCItem* DualWielding::GetLeftSwordInHand() const
	{
		return Npc->GetSlotItem(NPC_NODE_LEFTHAND);
	}

	void DualWielding::ChangeWeaponMode(zSTRING const& NewWeaponMode, int FromFightMode) const
	{
		oCItem* LeftSwordEquipped = Npc->GetSlotItem(NPC_NODE_LEFTSWORD);
		oCItem* LeftSwordInHand   = Npc->GetSlotItem(NPC_NODE_LEFTHAND);

		if (NewWeaponMode.IsEmpty() && FromFightMode == NPC_WEAPON_1HS && LeftSwordInHand && IsWeaponForDualWielding(LeftSwordInHand)) {
			SheathSwords();
			return;
		}
		
		if (NewWeaponMode == "1H" && LeftSwordEquipped && !LeftSwordInHand) {
			DrawSwords();
			return;
		}
	}

	void DualWielding::DrawSwords() const
	{
		oCItem*          LeftSwordEquipped = Npc->GetSlotItem(NPC_NODE_LEFTSWORD);
		zCModel*         NpcModel          = Npc->GetModel();
		zCModelNodeInst* SwordNode         = NpcModel->SearchNode(NPC_NODE_SWORD);
		zCModelNodeInst* LongswordNode     = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* LeftHandNode      = NpcModel->SearchNode(NPC_NODE_LEFTHAND);
		zCModelNodeInst* LeftSwordNode     = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LeftHandSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTHANDSWORD);

		RemoveFromSlotCompat(Npc, NPC_NODE_LEFTHAND);
		Npc->PutInSlot(NPC_NODE_LEFTHAND, LeftSwordEquipped, 1);

		NpcModel->SetNodeVisual(SwordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftSwordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LongswordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftHandNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftHandSwordNode, LeftSwordEquipped->visual, 0);
	}

	void DualWielding::SheathSwords() const
	{
		oCItem*          RightSwordEquipped = Npc->GetSlotItem(NPC_NODE_SWORD);
		oCItem*          LeftSwordInHand    = Npc->GetSlotItem(NPC_NODE_LEFTHAND);
		zCModel*         NpcModel           = Npc->GetModel();
		zCModelNodeInst* SwordNode          = NpcModel->SearchNode(NPC_NODE_SWORD);
		zCModelNodeInst* LongswordNode      = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* RightHandNode      = NpcModel->SearchNode(NPC_NODE_RIGHTHAND);
		zCModelNodeInst* LeftHandNode       = NpcModel->SearchNode(NPC_NODE_LEFTHAND);
		zCModelNodeInst* LeftSwordNode      = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LeftHandSwordNode  = NpcModel->SearchNode(NPC_NODE_LEFTHANDSWORD);

		RemoveFromSlotCompat(Npc, NPC_NODE_LEFTHAND);
		Npc->PutInSlot(NPC_NODE_LEFTHAND, nullptr, 1);

		NpcModel->SetNodeVisual(LeftSwordNode, LeftSwordInHand->visual, 0);
		NpcModel->SetNodeVisual(LongswordNode, RightSwordEquipped->visual, 0);
		NpcModel->SetNodeVisual(SwordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftHandNode, nullptr, 0);
		NpcModel->SetNodeVisual(RightHandNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftHandSwordNode, nullptr, 0);
	}

	oCItem* DualWielding::GetWeaponForDamage()
	{
		oCItem* LeftSwordEquipped  = GetEquippedLeftSword();
		oCItem* RightSwordEquipped = Npc->GetSlotItem(NPC_NODE_RIGHTHAND);

		if (!RightSwordEquipped || !LeftSwordEquipped || !IsWeaponForDualWielding(RightSwordEquipped)) {
			return nullptr;
		}

		zCModel*          NpcModel      = Npc->GetModel();
		zCModelNodeInst*  RightHandNode = NpcModel->SearchNode(NPC_NODE_RIGHTHAND);
		zCModelNodeInst*  LeftHandNode  = NpcModel->SearchNode(NPC_NODE_LEFTHAND);
		zCModelNodeInst** HitLimbs      = Npc->GetAnictrl()->hitlimb;

		bool DamageFromRightWeapon = false;
		bool DamageFromLeftWeapon  = false;
		for (int i = 0; i < ANI_HITLIMB_MAX; i++) {
			if (HitLimbs[i] == RightHandNode) {
				DamageFromRightWeapon = true;
			}

			if (HitLimbs[i] == LeftHandNode) {
				DamageFromLeftWeapon = true;
			}
		}

		if (DamageFromRightWeapon && DamageFromLeftWeapon) {
			if (CombinedSword) {
				CombinedSword->Release();
				CombinedSword = nullptr;
			}

			CombinedSword = RightSwordEquipped->CreateCopy()->CastTo<oCItem>();
			CombinedSword->AddRef();
			CombinedSword->damageTotal += LeftSwordEquipped->damageTotal;
			CombinedSword->flags &= LeftSwordEquipped->flags;
			for (int i = 0; i < oEDamageIndex_MAX; i++) {
				// Use 60% of damage of each weapon - TODO: make configurable
				// When one weapon does not have any damage of that type, use full damage of stronger weapon
				if (CombinedSword->damage[i] == 0 || LeftSwordEquipped->damage[i] == 0) {
					CombinedSword->damage[i] = (CombinedSword->damage[i] > LeftSwordEquipped->damage[i]) ? CombinedSword->damage[i] : LeftSwordEquipped->damage[i];
					continue;
				}
				CombinedSword->damage[i] = static_cast<int>((CombinedSword->damage[i] * 0.60) + (LeftSwordEquipped->damage[i] * 0.60));
			}

			return CombinedSword;
		}

		if (DamageFromRightWeapon) {
			return RightSwordEquipped;
		}

		if (DamageFromLeftWeapon) {
			return LeftSwordEquipped;
		}

		return nullptr;
	}

	void DualWielding::DropWeapons(bool WasInFightMode, oCItem* RightSword, oCItem* LeftSword)
	{
		zCModel*         NpcModel          = Npc->GetModel();
		zCModelNodeInst* SwordNode         = NpcModel->SearchNode(NPC_NODE_SWORD);
		zCModelNodeInst* LongswordNode     = NpcModel->SearchNode(NPC_NODE_LONGSWORD);
		zCModelNodeInst* LeftHandNode      = NpcModel->SearchNode(NPC_NODE_LEFTHAND);
		zCModelNodeInst* RightHandNode     = NpcModel->SearchNode(NPC_NODE_RIGHTHAND);
		zCModelNodeInst* LeftSwordNode     = NpcModel->SearchNode(NPC_NODE_LEFTSWORD);
		zCModelNodeInst* LeftHandSwordNode = NpcModel->SearchNode(NPC_NODE_LEFTHANDSWORD);

		UnequipRightWeapon();
		UnequipLeftWeapon();

		NpcModel->SetNodeVisual(LeftHandSwordNode, nullptr, 0);
		NpcModel->SetNodeVisual(LeftHandNode, nullptr, 0);

		if (WasInFightMode) {
			if (LeftSword) {
				ogame->GetGameWorld()->AddVob(LeftSword);
				ogame->GetGameWorld()->EnableVob(LeftSword, nullptr);
				zMAT4 Trafo = NpcModel->GetTrafoNodeToModel(LeftHandNode);
				LeftSword->SetPositionWorld(Npc->GetPositionWorld() + Trafo.GetTranslation());
				LeftSword->physicsEnabled = true;
				LeftSword->SetSleeping(false);
			}
			if (RightSword) {
				ogame->GetGameWorld()->AddVob(RightSword);
				ogame->GetGameWorld()->EnableVob(RightSword, nullptr);
				zMAT4 Trafo = NpcModel->GetTrafoNodeToModel(RightHandNode);
				RightSword->SetPositionWorld(Npc->GetPositionWorld() + Trafo.GetTranslation());
				RightSword->physicsEnabled = true;
				RightSword->SetSleeping(false);
			}
		}

		if (LeftSword) {
			LeftSword->Release();
		}
		if (RightSword) {
			RightSword->Release();
		}
	}

	void DualWielding::ApplyDualAnimations() const
	{
		Npc->ApplyOverlay(NPC_MDS_DUALWIELDING);
	}

	void DualWielding::RemoveDualAnimations() const
	{
		Npc->RemoveOverlay(NPC_MDS_DUALWIELDING);
	}

	bool DualWielding::IsWeaponForDualWielding(oCItem* Weapon)
	{
		return Weapon->HasFlag(ITM_FLAG_SWD) || Weapon->HasFlag(ITM_FLAG_AXE);
	}
}
