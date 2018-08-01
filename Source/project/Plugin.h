#pragma once
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"
#include "FSM.h"

class IBaseInterface;
class IExamInterface;

enum class INVENTORY
{
	MEDKIT,
	PISTOL,
	FOOD,
	GARBAGE, 
	EMPTY,
	UNKNOWN
};

enum class HouseTag
{
	SAFE,
	DANGER
};

struct InventorySlot
{
	INVENTORY itemType = INVENTORY::EMPTY;
	bool filled = false;
};

struct House
{
	HouseTag tag = HouseTag::SAFE;
	Elite::Vector2 pos;
	float searchTimer = 0.0f;
	float recentTimer = 0.0f;

	float timer = 0.0f;
	float waitTime = 45.0f;
	float searchTime = 75.0f;
	float recentTime = 30.0f;

	bool searched = false;
	bool recentVisit = false;
};

struct Item
{
	Elite::Vector2 pos;
	INVENTORY type = INVENTORY::UNKNOWN;
	bool needed = true;
};

struct FSMVariables
{
	float agentEnergy = 10.0f;
	float agentHealth = 10.0f;
	float agentStamina = 10.0f;
	float energyThreshold = 5.0f;
	float staminaThreshold = 7.0f;
	float ignoreTime = 1.5f;
	float timer = 0.0f;
	float hideTime = 4.0f;
	float runTime = 4.0f;
	float scavengeTime = 4.5f;
	float clearItemTime = 1.5f;
	float turnIntervalTime = 4.0f;
	float turnTime = 0.4f;
	float grabRange = 3.0f;
	float orientation = 0.0f;
	float turnspeed = 1.0f;
	float moveSpeed = 1.0f;
	float fightTime = 0.75f;
	float getTime = 0.5f;
	float dt = 0.0f;
	bool hasDestination = false;
	bool knowHouse = false;
	bool lookingBack = false;	
	bool canGrab = true;
	bool canRun = false;
	bool inHouse = false;
	bool cleared = false;
	bool bitten = false;
	bool usedItem = false;
	Elite::Vector2 destination = {};
	Elite::Vector2 checkpointDirection = {};
	Elite::Vector2 agentPosition = {};
	vector<HouseInfo> houses;
	vector<EntityInfo> entities;
	vector<InventorySlot> inventorySlots;
	vector<House> knownHouses;
	vector<Item> knownItems;
	IExamInterface* interface = nullptr;
	SteeringPlugin_Output steering = SteeringPlugin_Output();
};

class Plugin :public IExamPlugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void ProcessEvents(const SDL_Event& e) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

	void Heal(bool override = false);
	void Eat(bool override = false);
	void InitializeFSM();

	bool AddHouse();
	bool AddItem();
	bool CheckHouse(Elite::Vector2 center);
	bool HasFood();

	static void AddItem(ItemInfo &item, FSMVariables& FSM_Var, 
		bool medOverride = false, bool foodOverride = false);
	static void Shoot(EnemyInfo enemy, FSMVariables& FSM_Var);
	static void SetHouseAsDanger(FSMVariables& FSM_Var);
	static void SetHouseAsSearched(FSMVariables& FSM_Var);
	static void SetHouseAsRecent(FSMVariables& FSM_Var);
	static void NotNeeded(FSMVariables& FSM_Var, Elite::Vector2 itemPos);
	static void RemoveItem(FSMVariables& FSM_Var, Elite::Vector2 itemPos);
	static Elite::Vector2 GetForward(FSMVariables& FSM_Var);
	static Elite::Vector2 GetClosestHouseToSearch(FSMVariables& FSM_Var);
	static Elite::Vector2 GetClosestItem(FSMVariables& FSM_Var);
	static bool CheckNeeded(FSMVariables& FSM_Var, Elite::Vector2 itemPos);
	static float GetAngleBetween(Elite::Vector2 agentDir, Elite::Vector2 otherDir, 
		bool rad = false);
	static float Clamp(float x, float min, float max);

private:
	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	static vector<HouseInfo> GetHousesInFOV(FSMVariables& FSM_Var);
	static vector<EntityInfo> GetEntitiesInFOV(FSMVariables& FSM_Var);

	//FSM
	FSM* m_pStateMachine = nullptr;
	vector<FSMConditionBase*> m_Conditions = {};
	vector<FSMDelegateBase*> m_Delegates = {};
	vector<FSMState*> m_States = {};
	FSMVariables m_FSMVariables = FSMVariables{};

	//FSM Functions
	static void MoveToCheckpoint(FSMVariables& FSM_Var);
	static void Flee(FSMVariables& FSM_Var);
	static void Hide(FSMVariables& FSM_Var);
	static void Bitten(FSMVariables& FSM_Var);
	static void Scavenge(FSMVariables& FSM_Var);
	static void Healing(FSMVariables& FSM_Var);
	static void Eating(FSMVariables& FSM_Var);
	static void Return(FSMVariables& FSM_Var);
	static void LookForItem(FSMVariables& FSM_Var);
	static void GetItem(FSMVariables& FSM_Var);
	static void FightAggressive(FSMVariables& FSM_Var);
	static void FightPassive(FSMVariables& FSM_Var);
};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}