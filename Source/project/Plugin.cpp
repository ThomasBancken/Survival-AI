
#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include <algorithm>

//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);
	m_FSMVariables.interface = m_pInterface;

	auto agentInfo = m_pInterface->Agent_GetInfo();

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "Tult Zit";
	info.Student_FirstName = "Thomas";
	info.Student_LastName = "Bancken";
	info.Student_Class = "2DAE4";

	//Custom Init of variables
	for (size_t i = 0; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		m_FSMVariables.inventorySlots.push_back(InventorySlot());
		m_FSMVariables.inventorySlots[i].itemType = INVENTORY::EMPTY;
		m_FSMVariables.inventorySlots[i].filled = false;
	}

	InitializeFSM();
}

//Called only once
void Plugin::DllInit()
{
	//Can be used to figure out the source of a Memory Leak
	//Possible undefined behavior, you'll have to trace the source manually 
	//if you can't get the origin through _CrtSetBreakAlloc(0) [See CallStack]
	//_CrtSetBreakAlloc(0);

	//Called when the plugin is loaded
}

//Called only once
void Plugin::DllShutdown()
{
	//Called wheb the plugin gets unloaded
	//Cleanup
	SAFE_DELETE(m_pStateMachine);

	for (int i = 0; i < static_cast<int>(m_Conditions.size()); ++i)
		SAFE_DELETE(m_Conditions[i]);
	m_Conditions.clear();

	for (int i = 0; i < static_cast<int>(m_Delegates.size()); ++i)
		SAFE_DELETE(m_Delegates[i]);
	m_Delegates.clear();

	for (int i = 0; i < static_cast<int>(m_States.size()); ++i)
		SAFE_DELETE(m_States[i]);
	m_States.clear();
}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be usefull to inspect certain behaviours (Default = false)
							//params.LevelFile = "LevelTwo.gppl";
	params.AutoGrabClosestItem = false; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
	params.OverrideDifficulty = false; //Override Difficulty?
	params.Difficulty = 1.f; //Difficulty Override: 0 > 1 (Overshoot is possible, >1)
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::ProcessEvents(const SDL_Event& e)
{
	//Demo Event Code
	//In the end your AI should be able to walk around without external input
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	m_FSMVariables.cleared = false;
	m_FSMVariables.usedItem = false;

	//auto steering = SteeringPlugin_Output();

	//Use the Interface (IAssignmentInterface) to 'interface' with the AI_Framework
	auto agentInfo = m_pInterface->Agent_GetInfo();

	//Retrieve the current location of our CheckPoint
	auto checkpointLocation = m_pInterface->World_GetCheckpointLocation();

	//Use the navmesh to calculate the next navmesh point
	auto nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(checkpointLocation);

	//OR, Use the mouse target
	//auto nextTargetPos = m_Target; //Uncomment this to use mouse position as guidance

	
	auto vHousesInFOV = GetHousesInFOV(m_FSMVariables);//uses m_pInterface->Fov_GetHouseByIndex(...)
	auto vEntitiesInFOV = GetEntitiesInFOV(m_FSMVariables); //uses m_pInterface->Fov_GetEntityByIndex(...)

	/*
	ItemInfo item;
	Elite::Vector2 dir;
	bool hasGun = false;
	bool enemyInSight = false;

	if (m_Obey)
	{
		m_State = AI_STATE::OBEY;
	}

	if (agentInfo.Position.Magnitude() > Elite::Vector2(160, 160).Magnitude())
	{
		m_State = AI_STATE::RETURN;
	}
	*/
	//cout << (checkpointLocation - agentInfo.Position).Magnitude() << endl;

#pragma region FSM
	m_FSMVariables.agentEnergy = agentInfo.Energy;
	m_FSMVariables.agentStamina = agentInfo.Stamina;
	m_FSMVariables.agentHealth = agentInfo.Health;
	m_FSMVariables.agentPosition = agentInfo.Position;
	m_FSMVariables.checkpointDirection = m_pInterface->NavMesh_GetClosestPathPoint(checkpointLocation);
	m_FSMVariables.inHouse = agentInfo.IsInHouse;	
	m_FSMVariables.orientation = agentInfo.Orientation;
	m_FSMVariables.entities = vEntitiesInFOV;
	m_FSMVariables.houses = vHousesInFOV;
	m_FSMVariables.dt = dt;
	m_FSMVariables.grabRange = agentInfo.GrabRange;
	m_FSMVariables.turnspeed = agentInfo.MaxAngularSpeed;
	m_FSMVariables.moveSpeed = agentInfo.MaxLinearSpeed;
	m_FSMVariables.bitten = agentInfo.Bitten;

	//cout << m_FSMVariables.checkpointDirection.x << " " << m_FSMVariables.checkpointDirection.y << endl;
#pragma endregion

	//AI
	//**

	if (m_pStateMachine)
	{
		m_pStateMachine->Update();
	}

	m_FSMVariables.steering.RunMode = m_FSMVariables.canRun;

	//cout << m_FSMVariables.steering.RunMode << m_FSMVariables.canRun << endl;

	
	if (m_FSMVariables.agentHealth < 10.0f)
	{
		Heal();
	}
	if (m_FSMVariables.agentEnergy < 10.0f)
	{
		Eat();
	}

	//HOUSES
	//******
	AddHouse();

	for (size_t i = 0; i < m_FSMVariables.knownHouses.size(); ++i)
	{
		if (m_FSMVariables.knownHouses[i].tag == HouseTag::DANGER)
		{
			m_FSMVariables.knownHouses[i].timer += dt;

			if (m_FSMVariables.knownHouses[i].timer >= m_FSMVariables.knownHouses[i].waitTime)
			{
				cout << "A house has been marked: SAFE" << endl;

				m_FSMVariables.knownHouses[i].tag = HouseTag::SAFE;
				m_FSMVariables.knownHouses[i].timer = 0;
				m_FSMVariables.knownHouses[i].recentVisit = false;

			}
		}
		else
		{
			m_FSMVariables.knownHouses[i].timer = 0;
		}

		if (m_FSMVariables.knownHouses[i].searched)
		{
			m_FSMVariables.knownHouses[i].searchTimer += dt;
			if (m_FSMVariables.knownHouses[i].searchTimer >= m_FSMVariables.knownHouses[i].searchTime)
			{
				cout << "A house has been marked: UNSEARCHED" << endl;

				m_FSMVariables.knownHouses[i].searched = false;
				m_FSMVariables.knownHouses[i].searchTimer = 0;
			}
		}

		if (m_FSMVariables.knownHouses[i].recentVisit)
		{
			m_FSMVariables.knownHouses[i].recentTimer += dt;
			if (m_FSMVariables.knownHouses[i].recentTimer >= m_FSMVariables.knownHouses[i].recentTime)
			{
				cout << "A house has been marked: NOT RECENT" << endl;

				m_FSMVariables.knownHouses[i].recentVisit = false;
				m_FSMVariables.knownHouses[i].recentTimer = 0;
			}
		}
	}

	//Items
	if (!m_FSMVariables.cleared)
	{
		AddItem();
	}

	//cout << "Agent Position: " << agentInfo.Position.x << ", " << agentInfo.Position.y << endl;
	//cout << "Agent Position: " << agentInfo.Position.x << ", " << agentInfo.Position.y << " Agent Orientation: " << agentInfo.Orientation << " Agent forward: " << GetForward().x << ", " << GetForward().y << endl;
	
	//cout << m_FSMVariables.moveSpeed << endl;
	
	return m_FSMVariables.steering;
	//return steering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	auto agentInfo = m_FSMVariables.interface->Agent_GetInfo();

	//This Render function should only contain calls to Interface->Draw_... functions
}

void Plugin::AddItem(ItemInfo &item, FSMVariables& FSM_Var, bool medOverride, bool foodOverride)
{
	NotNeeded(FSM_Var, item.Location);

	ItemInfo currStoredItem;
	ItemInfo toCheckItem;
	ItemInfo worstCaseItem;

	if (medOverride)
	{
		bool succes = false;

		for (size_t i = 0; i < FSM_Var.inventorySlots.size(); ++i)
		{
			if (!FSM_Var.inventorySlots[i].filled)
			{
				FSM_Var.interface->Inventory_AddItem(i, item);

				switch (item.Type)
				{
				case eItemType::PISTOL:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::PISTOL;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::MEDKIT:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::MEDKIT;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::FOOD:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::FOOD;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::GARBAGE:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::GARBAGE;
					FSM_Var.inventorySlots[i].filled = true;

					FSM_Var.interface->Inventory_RemoveItem(i);
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				default:
					cout << "Switch default called1" << endl;
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				}

				succes = true;

				break;
			}
			else if(currStoredItem.Type == eItemType::MEDKIT)
			{
				FSM_Var.interface->Inventory_GetItem(i, currStoredItem);
				int health = FSM_Var.interface->Item_GetMetadata(currStoredItem, "health");
				int otherHealth = FSM_Var.interface->Item_GetMetadata(item, "health");
			
				if (health < otherHealth)
				{
					FSM_Var.interface->Inventory_RemoveItem(i);
					FSM_Var.inventorySlots[i].filled = false;
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;

					FSM_Var.interface->Inventory_AddItem(i, item);
					FSM_Var.inventorySlots[i].filled = true;
					FSM_Var.inventorySlots[i].itemType = INVENTORY::MEDKIT;
				}
			}
		}
	}
	else if(foodOverride)
	{
		bool succes = false;

		for (size_t i = 0; i < FSM_Var.inventorySlots.size(); ++i)
		{
			if (!FSM_Var.inventorySlots[i].filled)
			{
				FSM_Var.interface->Inventory_AddItem(i, item);

				switch (item.Type)
				{
				case eItemType::PISTOL:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::PISTOL;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::MEDKIT:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::MEDKIT;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::FOOD:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::FOOD;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::GARBAGE:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::GARBAGE;
					FSM_Var.inventorySlots[i].filled = true;

					FSM_Var.interface->Inventory_RemoveItem(i);
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				default:
					cout << "Switch default called2" << endl;

					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				}

				succes = true;

				break;
			}
			else if (currStoredItem.Type == eItemType::FOOD)
			{
				FSM_Var.interface->Inventory_GetItem(i, currStoredItem);
				int food = FSM_Var.interface->Item_GetMetadata(currStoredItem, "energy");
				int otherFood = FSM_Var.interface->Item_GetMetadata(item, "energy");

				if (food < otherFood)
				{
					FSM_Var.interface->Inventory_RemoveItem(i);
					FSM_Var.inventorySlots[i].filled = false;
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;

					FSM_Var.interface->Inventory_AddItem(i, item);
					FSM_Var.inventorySlots[i].filled = true;
					FSM_Var.inventorySlots[i].itemType = INVENTORY::FOOD;
				}
			}
		}
	}
	else
	{
		bool succes = false;

		for (size_t i = 0; i < FSM_Var.inventorySlots.size(); ++i)
		{
			if (!FSM_Var.inventorySlots[i].filled)
			{
				FSM_Var.interface->Inventory_AddItem(i, item);

				switch (item.Type)
				{
				case eItemType::PISTOL:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::PISTOL;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::MEDKIT:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::MEDKIT;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::FOOD:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::FOOD;
					FSM_Var.inventorySlots[i].filled = true;
					break;
				case eItemType::GARBAGE:
					FSM_Var.inventorySlots[i].itemType = INVENTORY::GARBAGE;
					FSM_Var.inventorySlots[i].filled = true;

					FSM_Var.interface->Inventory_RemoveItem(i);
					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				default:
					cout << "Switch default called3" << endl;

					FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
					FSM_Var.inventorySlots[i].filled = false;
					break;
				}

				succes = true;

				break;
			}
		}

		if (succes)
		{
			RemoveItem(FSM_Var, item.Location);

			return;
		}
		else
		{
			int worstCase = -1;

			int pistols = 0;
			int medpacks = 0;
			int foodpacks = 0;
			int garbage = 0;
			int emptySlots = 0;

			for (size_t i = 0; i < FSM_Var.inventorySlots.size(); ++i)
			{
				if (FSM_Var.inventorySlots[i].filled)
				{
					switch (FSM_Var.inventorySlots[i].itemType)
					{
					case INVENTORY::PISTOL:
						++pistols;
						break;
					case INVENTORY::MEDKIT:
						++medpacks;
						break;
					case INVENTORY::FOOD:
						++foodpacks;
						break;
					case INVENTORY::GARBAGE:
						++garbage;
						break;
					case INVENTORY::EMPTY:
						++emptySlots;
						FSM_Var.inventorySlots[i].filled = false;
						break;
					}
				}
			}

			bool substituted = false;


					int ammo;
					int otherAmmo;
					int health;
					int otherHealth;
					int food;
					int otherFood;

					switch (item.Type)
					{
					case eItemType::PISTOL:
						if (medpacks >= 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::MEDKIT)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										health = FSM_Var.interface->Item_GetMetadata(toCheckItem, "health");
										otherHealth = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "health");

										if (health < otherHealth)
										{
											worstCase = j;
										}
									}
								}
							}

							if (FSM_Var.agentHealth < 10.0f && !FSM_Var.usedItem)
							{
								FSM_Var.interface->Inventory_UseItem(worstCase);
								FSM_Var.usedItem = true;
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Used a Medkit1" << endl;
							}
							else
							{
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Threw out a Medkit1" << endl;
							}

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::PISTOL;
							substituted = true;
							RemoveItem(FSM_Var, item.Location);

						}
						else if (pistols > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::PISTOL)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										ammo = FSM_Var.interface->Item_GetMetadata(toCheckItem, "ammo");
										otherAmmo = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "ammo");

										if (ammo < otherAmmo)
										{
											worstCase = j;
										}
									}
								}
							}

							int checkAmmo = FSM_Var.interface->Item_GetMetadata(item, "ammo");

							FSM_Var.interface->Inventory_RemoveItem(worstCase);
							FSM_Var.inventorySlots[worstCase].filled = false;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::PISTOL;
							substituted = true;
							cout << "Threw out a Pistol" << endl;
							RemoveItem(FSM_Var, item.Location);

						}
						else if (foodpacks > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::FOOD)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										food = FSM_Var.interface->Item_GetMetadata(toCheckItem, "energy");
										otherFood = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "energy");


										if (food < otherFood)
										{
											worstCase = j;
										}
									}
								}
							}
							if (FSM_Var.agentEnergy < 10.0f && !FSM_Var.usedItem)
							{
								FSM_Var.interface->Inventory_UseItem(worstCase);
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.usedItem = true;
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Used a FoodPack1" << endl;
							}
							else
							{
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Threw out a FoodPack1" << endl;
							}

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::PISTOL;
							substituted = true;
							RemoveItem(FSM_Var, item.Location);

						}
						else
						{
							RemoveItem(FSM_Var, item.Location);

							Item newItem;
							newItem.pos = item.Location;
							newItem.needed = false;
							FSM_Var.knownItems.push_back(newItem);
						}

						break;
					case eItemType::MEDKIT:
						if (medpacks >= 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::MEDKIT)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										health = FSM_Var.interface->Item_GetMetadata(toCheckItem, "health");
										otherHealth = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "health");


										if (health < otherHealth)
										{
											worstCase = j;
										}
									}
								}
							}

							int checkHealth = FSM_Var.interface->Item_GetMetadata(item, "health");

								if (FSM_Var.agentHealth < 10.0f && !FSM_Var.usedItem)
								{
									FSM_Var.interface->Inventory_UseItem(worstCase);
									FSM_Var.interface->Inventory_RemoveItem(worstCase);
									FSM_Var.usedItem = true;
									FSM_Var.inventorySlots[worstCase].filled = false;
									FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
									cout << "Used a Medkit2" << endl;
								}
								else
								{
									FSM_Var.interface->Inventory_RemoveItem(worstCase);
									FSM_Var.inventorySlots[worstCase].filled = false;
									FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
									cout << "Threw out a Medkit2" << endl;
								}

								FSM_Var.interface->Inventory_AddItem(worstCase, item);
								FSM_Var.inventorySlots[worstCase].filled = true;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::MEDKIT;
								substituted = true;
								RemoveItem(FSM_Var, item.Location);

						}
						else if (pistols > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::PISTOL)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										ammo = FSM_Var.interface->Item_GetMetadata(toCheckItem, "ammo");
										otherAmmo = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "ammo");


										if (ammo < otherAmmo)
										{
											worstCase = j;
										}
									}
								}
							}

							FSM_Var.interface->Inventory_RemoveItem(worstCase);
							FSM_Var.inventorySlots[worstCase].filled = false;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::MEDKIT;
							substituted = true;
							cout << "Threw out a Pistol2" << endl;
							RemoveItem(FSM_Var, item.Location);

						}
						else if (foodpacks > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::FOOD)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										food = FSM_Var.interface->Item_GetMetadata(toCheckItem, "energy");
										otherFood = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "energy");


										if (food < otherFood)
										{
											worstCase = j;
										}
									}
								}
							}

							if (FSM_Var.agentEnergy < 10.0f && !FSM_Var.usedItem)
							{
								FSM_Var.interface->Inventory_UseItem(worstCase);
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.usedItem = true;
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Used a FoodPack2" << endl;
							}
							else
							{
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Threw out a FoodPack2" << endl;
							}

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::MEDKIT;
							substituted = true;
							RemoveItem(FSM_Var, item.Location);

						}
						else
						{
							cout << "No need to substitute2" << endl;
							RemoveItem(FSM_Var, item.Location);

							Item newItem;
							newItem.pos = item.Location;
							newItem.needed = false;
							FSM_Var.knownItems.push_back(newItem);
						}

						break;
					case eItemType::FOOD:
						if (medpacks >= 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::MEDKIT)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										health = FSM_Var.interface->Item_GetMetadata(toCheckItem, "health");
										otherHealth = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "health");


										if (health < otherHealth)
										{
											worstCase = j;
										}
									}
								}
							}

							if (FSM_Var.agentHealth < 10.0f && !FSM_Var.usedItem)
							{
								FSM_Var.interface->Inventory_UseItem(worstCase);
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.usedItem = true;
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Used a Medkit3" << endl;
							}
							else
							{
								FSM_Var.interface->Inventory_RemoveItem(worstCase);
								FSM_Var.inventorySlots[worstCase].filled = false;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
								cout << "Threw out a Medkit3" << endl;
							}

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::FOOD;
							substituted = true;
							RemoveItem(FSM_Var, item.Location);

						}
						else if (pistols > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::PISTOL)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										ammo = FSM_Var.interface->Item_GetMetadata(toCheckItem, "ammo");
										otherAmmo = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "ammo");


										if (ammo < otherAmmo)
										{
											worstCase = j;
										}
									}
								}
							}

							FSM_Var.interface->Inventory_RemoveItem(worstCase);
							FSM_Var.inventorySlots[worstCase].filled = false;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;

							FSM_Var.interface->Inventory_AddItem(worstCase, item);
							FSM_Var.inventorySlots[worstCase].filled = true;
							FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::FOOD;
							substituted = true;
							cout << "Threw out a Pistol3" << endl;
							RemoveItem(FSM_Var, item.Location);


						}
						else if (foodpacks > 2)
						{
							for (size_t j = 0; j < FSM_Var.inventorySlots.size(); ++j)
							{
								if (FSM_Var.inventorySlots[j].filled && FSM_Var.inventorySlots[j].itemType == INVENTORY::FOOD)
								{
									if (worstCase == -1)
									{
										worstCase = j;
									}
									else
									{
										FSM_Var.interface->Inventory_GetItem(worstCase, worstCaseItem);
										FSM_Var.interface->Inventory_GetItem(j, toCheckItem);

										food = FSM_Var.interface->Item_GetMetadata(toCheckItem, "energy");
										otherFood = FSM_Var.interface->Item_GetMetadata(worstCaseItem, "energy");


										if (food < otherFood)
										{
											worstCase = j;
										}
									}
								}
							}
							int checkFood = FSM_Var.interface->Item_GetMetadata(item, "energy");

								if (FSM_Var.agentEnergy < 10.0f && !FSM_Var.usedItem)
								{
									FSM_Var.interface->Inventory_UseItem(worstCase);
									FSM_Var.interface->Inventory_RemoveItem(worstCase);
									FSM_Var.usedItem = true;
									FSM_Var.inventorySlots[worstCase].filled = false;
									FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
									cout << "Used a FoodPack3" << endl;
								
								}
								else
								{
									FSM_Var.interface->Inventory_RemoveItem(worstCase);
									FSM_Var.inventorySlots[worstCase].filled = false;
									FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::EMPTY;
									cout << "Threw out a FoodPack3" << endl;
								}

								FSM_Var.interface->Inventory_AddItem(worstCase, item);
								FSM_Var.inventorySlots[worstCase].filled = true;
								FSM_Var.inventorySlots[worstCase].itemType = INVENTORY::FOOD;
								substituted = true;
								RemoveItem(FSM_Var, item.Location);

						}
						else
						{
							cout << "No need to substitute3" << endl;
							RemoveItem(FSM_Var, item.Location);

							Item newItem;
							newItem.pos = item.Location;
							newItem.needed = false;
							FSM_Var.knownItems.push_back(newItem);
						}
						break;
					case eItemType::GARBAGE:
						FSM_Var.interface->Inventory_DropItem(0);
						FSM_Var.inventorySlots[0].filled = false;
						FSM_Var.inventorySlots[0].itemType = INVENTORY::EMPTY;

						FSM_Var.interface->Inventory_AddItem(0, item);
						FSM_Var.inventorySlots[0].filled = true;
						FSM_Var.inventorySlots[0].itemType = INVENTORY::GARBAGE;
						cout << "Dropped Item" << endl;

						FSM_Var.interface->Inventory_RemoveItem(0);
						FSM_Var.inventorySlots[0].filled = false;
						FSM_Var.inventorySlots[0].itemType = INVENTORY::EMPTY;
						cout << "Threw out a Garbage" << endl;

						RemoveItem(FSM_Var, item.Location);

						break;
					}
		}
	}
}

void Plugin::Heal(bool override)
{
	if (override)
	{
		for (size_t i = 0; i < m_FSMVariables.inventorySlots.size(); ++i)
		{
			if (m_FSMVariables.inventorySlots[i].filled && m_FSMVariables.inventorySlots[i].itemType == INVENTORY::MEDKIT)
			{
				ItemInfo item;

				m_FSMVariables.interface->Inventory_GetItem(i, item);
				int health = m_FSMVariables.interface->Item_GetMetadata(item, "health");

				if (health >= 1 && !m_FSMVariables.usedItem)
				{
					m_FSMVariables.interface->Inventory_UseItem(i);
					m_FSMVariables.usedItem = true;

					m_FSMVariables.interface->Inventory_RemoveItem(i);
					m_FSMVariables.inventorySlots[i].filled = false;
					m_FSMVariables.inventorySlots[i].itemType = INVENTORY::EMPTY;
						cout << "Medpack not removed, it has " << health << " remaining health points" << endl;
				}
			}
		}
	}
	else
	{
		for (size_t i = 0; i < m_FSMVariables.inventorySlots.size(); ++i)
		{
			if (m_FSMVariables.inventorySlots[i].filled && m_FSMVariables.inventorySlots[i].itemType == INVENTORY::MEDKIT)
			{
				ItemInfo item;

				m_FSMVariables.interface->Inventory_GetItem(i, item);
				int health = m_FSMVariables.interface->Item_GetMetadata(item, "health");

				if (health >= 1 && !m_FSMVariables.usedItem)
				{
					if (m_FSMVariables.agentHealth <= 10.0f - health && !m_FSMVariables.usedItem)
					{
						m_FSMVariables.interface->Inventory_UseItem(i);
						m_FSMVariables.usedItem = true;

						m_FSMVariables.interface->Inventory_RemoveItem(i);
							m_FSMVariables.inventorySlots[i].filled = false;
							m_FSMVariables.inventorySlots[i].itemType = INVENTORY::EMPTY;

						return;
					}				
				}
			}
		}
	}
}

void Plugin::Eat(bool override)
{
	if (override)
	{
		for (size_t i = 0; i < m_FSMVariables.inventorySlots.size(); ++i)
		{
			if (m_FSMVariables.inventorySlots[i].filled && m_FSMVariables.inventorySlots[i].itemType == INVENTORY::FOOD)
			{
				ItemInfo item;

				m_FSMVariables.interface->Inventory_GetItem(i, item);
				int food = m_FSMVariables.interface->Item_GetMetadata(item, "energy");

				if (food >= 1 && !m_FSMVariables.usedItem)
				{
					cout << "Eating with override at food level: " << m_FSMVariables.agentEnergy << endl;

					m_FSMVariables.interface->Inventory_UseItem(i);
					m_FSMVariables.usedItem = true;

					m_FSMVariables.interface->Inventory_RemoveItem(i);
						m_FSMVariables.inventorySlots[i].filled = false;
						m_FSMVariables.inventorySlots[i].itemType = INVENTORY::EMPTY;
				}
			}
		}
	}
	else
	{
		for (size_t i = 0; i < m_FSMVariables.inventorySlots.size(); ++i)
		{
			if (m_FSMVariables.inventorySlots[i].filled && m_FSMVariables.inventorySlots[i].itemType == INVENTORY::FOOD)
			{
				ItemInfo item;

				m_FSMVariables.interface->Inventory_GetItem(i, item);
				int food = m_FSMVariables.interface->Item_GetMetadata(item, "energy");

				if (food >= 1 && !m_FSMVariables.usedItem)
				{
					if (m_FSMVariables.agentEnergy <= 10.0f - food)
					{
						cout << "Using food at energy level: " << m_FSMVariables.agentEnergy << " With other at: " << food << endl;

						m_FSMVariables.interface->Inventory_UseItem(i);
						m_FSMVariables.usedItem = true;

						m_FSMVariables.interface->Inventory_RemoveItem(i);
							m_FSMVariables.inventorySlots[i].filled = false;
							m_FSMVariables.inventorySlots[i].itemType = INVENTORY::EMPTY;					
						return;
					}			
				}
			}
		}
	}	
}

void Plugin::Shoot(EnemyInfo enemy, FSMVariables& FSM_Var)
{
	//cout << "Shoot" << endl;

	Elite::Vector2 forward;

	ItemInfo item;

	for (size_t i = 0; i < FSM_Var.inventorySlots.size(); ++i)
	{
		if (FSM_Var.inventorySlots[i].filled && FSM_Var.inventorySlots[i].itemType == INVENTORY::PISTOL)
		{
			FSM_Var.interface->Inventory_GetItem(i, item);
			int ammo = FSM_Var.interface->Item_GetMetadata(item, "ammo");
			float range = FSM_Var.interface->Item_GetMetadata(item, "range");
			float dps = FSM_Var.interface->Item_GetMetadata(item, "dps");

			if (ammo != 0 && !FSM_Var.usedItem)
			{
				if (((enemy.Location - FSM_Var.agentPosition).Magnitude() < range) && !FSM_Var.usedItem && ((FSM_Var.agentPosition + (GetForward(FSM_Var) * (enemy.Location - FSM_Var.agentPosition).Magnitude())) - enemy.Location).Magnitude() < (enemy.Size - (enemy.Size / 5.f)))
				{
					FSM_Var.usedItem = true;
					FSM_Var.interface->Inventory_UseItem(i);
					//cout << "Shooting" << endl;

					return;
				}
			}
			else if (!FSM_Var.usedItem)
			{
				cout << "Pistol out of ammo" << endl;
				FSM_Var.interface->Inventory_RemoveItem(i);
				FSM_Var.inventorySlots[i].filled = false;
				FSM_Var.inventorySlots[i].itemType = INVENTORY::EMPTY;
			}
		}
	}
}

bool Plugin::AddHouse()
{
	auto vHousesInFOV = GetHousesInFOV(m_FSMVariables);
	bool known = false;
	int id = 0;

	if (vHousesInFOV.size() > 0)
	{
		//cout << "I see a house" << endl;

		if (m_FSMVariables.knownHouses.size() > 0)
		{
			for (size_t i = 0; i < m_FSMVariables.knownHouses.size(); ++i)
			{
				for (size_t j = 0; j < vHousesInFOV.size(); j++)
				{
					//cout << "Checking known houses, i know of: " << vHousesInFOV.size() << endl;

					if (m_FSMVariables.knownHouses[i].pos == vHousesInFOV[j].Center)
					{
						known = true;
						id = j;
					}
				}
			}

			if (!known)
			{
				House house;
				house.pos = vHousesInFOV[id].Center;

				m_FSMVariables.knownHouses.push_back(house);

				cout << "Added a house. Now I know of " << m_FSMVariables.knownHouses.size() << " houses." << endl;

				return true;
			}
		}
		else
		{
			House house;
			house.pos = vHousesInFOV[0].Center;

			m_FSMVariables.knownHouses.push_back(house);

			cout << "Added first house. Now I know of " << m_FSMVariables.knownHouses.size() << " house." << endl;

			return true;
		}
	}

	return false;
}

bool Plugin::AddItem()
{
	auto vEntitiesInFOV = GetEntitiesInFOV(m_FSMVariables);
	bool known = false;
	int id = 0;

	if (vEntitiesInFOV.size() > 0)
	{
		//cout << "I see a house" << endl;

		if (m_FSMVariables.knownItems.size() > 0)
		{
			for (size_t i = 0; i < m_FSMVariables.knownItems.size(); ++i)
			{
				for (size_t j = 0; j < vEntitiesInFOV.size(); j++)
				{
					//cout << "Checking known houses, i know of: " << vHousesInFOV.size() << endl;

					if (vEntitiesInFOV[j].Type == eEntityType::ITEM )
					{
						if (m_FSMVariables.knownItems[i].pos == vEntitiesInFOV[j].Location)
						{
							known = true;
						}
						else
						{
							id = j;
						}
					}
				}
			}

			if (!known && vEntitiesInFOV[id].Type == eEntityType::ITEM)
			{
				Item item;
				item.pos = vEntitiesInFOV[id].Location;

				m_FSMVariables.knownItems.push_back(item);

				cout << "Added a item. Now I know of " << m_FSMVariables.knownItems.size() << " items." << endl;

				return true;
			}
		}
		else if(vEntitiesInFOV[0].Type == eEntityType::ITEM)
		{
			Item item;
			item.pos = vEntitiesInFOV[0].Location;

			m_FSMVariables.knownItems.push_back(item);

			cout << "Added first item. Now I know of " << m_FSMVariables.knownItems.size() << " item." << endl;

			return true;
		}
	}

	return false;
}

bool Plugin::CheckHouse(Elite::Vector2 center)
{
	for (size_t i = 0; i < m_FSMVariables.knownHouses.size(); ++i)
	{
		if (m_FSMVariables.knownHouses[i].pos == center 
			&& m_FSMVariables.knownHouses[i].tag == HouseTag::SAFE)
		{
			return true;
		}
	}

	return false;
}

bool Plugin::HasFood()
{
	bool succes = false;

	for (size_t i = 0; i < m_FSMVariables.inventorySlots.size(); ++i)
	{
		if (m_FSMVariables.inventorySlots[i].itemType == INVENTORY::FOOD)
		{
			succes = true;
		}
	}

	return succes;
}

void Plugin::SetHouseAsDanger(FSMVariables& FSM_Var)
{
	if (FSM_Var.knownHouses.size() > 1)
	{
		int closest = 0;
		for (size_t i = 1; i < FSM_Var.knownHouses.size(); ++i)
		{
			if ((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() > (FSM_Var.knownHouses[i].pos - FSM_Var.agentPosition).Magnitude())
			{
				closest = i;
			}
		}

		if (FSM_Var.knownHouses[closest].tag != HouseTag::DANGER)
		{
			cout << "Set House As Danger" << endl;

			FSM_Var.knownHouses[closest].tag = HouseTag::DANGER;
		}
	}
	else if(FSM_Var.knownHouses.size() == 1)
	{
		if (FSM_Var.knownHouses[0].tag != HouseTag::DANGER)
		{
			cout << "Set House As Danger" << endl;

			FSM_Var.knownHouses[0].tag = HouseTag::DANGER;
		}
	}
}

void Plugin::SetHouseAsSearched(FSMVariables& FSM_Var)
{
	if (FSM_Var.knownHouses.size() > 1)
	{
		int closest = 0;
		for (size_t i = 1; i < FSM_Var.knownHouses.size(); ++i)
		{
			if ((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() >(FSM_Var.knownHouses[i].pos - FSM_Var.agentPosition).Magnitude())
			{
				closest = i;
			}
		}

		if (!FSM_Var.knownHouses[closest].searched)
		{
			cout << "Set House As Searched" << endl;

			FSM_Var.knownHouses[closest].searched = true;
		}
	}
	else if (FSM_Var.knownHouses.size() == 1)
	{
		if (!FSM_Var.knownHouses[0].searched)
		{
			cout << "Set House As Searched" << endl;

			FSM_Var.knownHouses[0].searched = true;
		}
	}
}

void Plugin::SetHouseAsRecent(FSMVariables & FSM_Var)
{
	if (FSM_Var.knownHouses.size() > 1)
	{
		int closest = 0;
		for (size_t i = 1; i < FSM_Var.knownHouses.size(); ++i)
		{
			if ((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() >(FSM_Var.knownHouses[i].pos - FSM_Var.agentPosition).Magnitude())
			{
				closest = i;
			}
		}

		if (!FSM_Var.knownHouses[closest].recentVisit)
		{
			cout << "Set House As Recent" << endl;

			FSM_Var.knownHouses[closest].recentVisit = true;
		}
		else
		{
			cout << "Set House As Danger" << endl;

			FSM_Var.knownHouses[closest].tag = HouseTag::DANGER;
		}
	}
	else if (FSM_Var.knownHouses.size() == 1)
	{
		if (!FSM_Var.knownHouses[0].recentVisit)
		{
			cout << "Set House As Recent" << endl;

			FSM_Var.knownHouses[0].recentVisit = true;
		}
		else
		{
			cout << "Set House As Danger" << endl;

			FSM_Var.knownHouses[0].tag = HouseTag::DANGER;
		}
	}
}

float Plugin::Clamp(float x, float min, float max)
{
	if (x < min)
	{
		x = min;
	}
	if (x > max)
	{
		x = max;
	}

	return x;
}

Elite::Vector2 Plugin::GetForward(FSMVariables& FSM_Var)
{
	Elite::Vector2 forward;

	float degree = FSM_Var.orientation;

	forward.x = 0 * cos(degree) - (-1 * sin(degree));
	forward.y = 0 * sin(degree) + (-1 * cos(degree));

	return forward.GetNormalized();
}

Elite::Vector2 Plugin::GetClosestHouseToSearch(FSMVariables& FSM_Var)
{
	if (FSM_Var.knownHouses.size() > 1)
	{
		int closest = 0;
		for (size_t i = 1; i < FSM_Var.knownHouses.size(); ++i)
		{
			if (((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() 
				> (FSM_Var.knownHouses[i].pos - FSM_Var.agentPosition).Magnitude()) 
				&& !FSM_Var.knownHouses[i].searched)
			{
				closest = i;
			}
		}

		if (FSM_Var.knownHouses[closest].tag != HouseTag::DANGER && !FSM_Var.knownHouses[closest].searched)
		{
			FSM_Var.hasDestination = true;

			if ((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() 
				< (FSM_Var.interface->World_GetCheckpointLocation() - FSM_Var.agentPosition).Magnitude())
			{
				return FSM_Var.knownHouses[closest].pos;
			}

			return FSM_Var.interface->World_GetCheckpointLocation();
		}
	}
	else if (FSM_Var.knownHouses.size() == 1)
	{
		if (FSM_Var.knownHouses[0].tag != HouseTag::DANGER && !FSM_Var.knownHouses[0].searched)
		{
			FSM_Var.hasDestination = true;

			//cout << "Going to House" << endl;

			if ((FSM_Var.knownHouses[0].pos - FSM_Var.agentPosition).Magnitude() < (FSM_Var.interface->World_GetCheckpointLocation() - FSM_Var.agentPosition).Magnitude())
			{
				return FSM_Var.knownHouses[0].pos;
			}

			return FSM_Var.interface->World_GetCheckpointLocation();
		}
	}

	//FALLBACK
	auto checkpointLocation = FSM_Var.interface->World_GetCheckpointLocation();
	FSM_Var.knowHouse = false;
	FSM_Var.hasDestination = false;

	return checkpointLocation;
}

Elite::Vector2 Plugin::GetClosestItem(FSMVariables & FSM_Var)
{
	if (FSM_Var.knownItems.size() > 1)
	{
		int closest = 0;
		for (size_t i = 1; i < FSM_Var.knownItems.size(); ++i)
		{
			if (((FSM_Var.knownItems[closest].pos - FSM_Var.agentPosition).Magnitude() > (FSM_Var.knownItems[i].pos - FSM_Var.agentPosition).Magnitude()) && FSM_Var.knownItems[closest].needed)
			{
				closest = i;
			}
		}
		FSM_Var.hasDestination = true;

		//cout << "Going to Item" << endl;
		Elite::Vector2 house = GetClosestHouseToSearch(FSM_Var);

		if ((house - FSM_Var.agentPosition).Magnitude() < (FSM_Var.knownItems[0].pos - FSM_Var.agentPosition).Magnitude())
		{
			return house;
		}

			return FSM_Var.knownItems[closest].pos;
	}
	else if (FSM_Var.knownItems.size() == 1 && FSM_Var.knownItems[0].needed)
	{
			FSM_Var.hasDestination = true;

			//cout << "Going to Item" << endl;
			Elite::Vector2 house = GetClosestHouseToSearch(FSM_Var);

			if ((house - FSM_Var.agentPosition).Magnitude() < (FSM_Var.knownItems[0].pos - FSM_Var.agentPosition).Magnitude())
			{
				return house;
			}

			return FSM_Var.knownItems[0].pos;
		
	}

	//FALLBACK
	return GetClosestHouseToSearch(FSM_Var);

}

void Plugin::RemoveItem(FSMVariables & FSM_Var, Elite::Vector2 itemPos)
{
	for (size_t i = 0; i < FSM_Var.knownItems.size(); ++i)
	{
		if (FSM_Var.knownItems[i].pos == itemPos)
		{
			FSM_Var.cleared = true;
			cout << "Cleared item" << endl;

			FSM_Var.knownItems.erase(FSM_Var.knownItems.begin() + i);
		}
	}
}

void Plugin::NotNeeded(FSMVariables & FSM_Var, Elite::Vector2 itemPos)
{
	for (size_t i = 0; i < FSM_Var.knownItems.size(); ++i)
	{
		if (FSM_Var.knownItems[i].pos == itemPos)
		{
			FSM_Var.knownItems[i].needed = false;	
		}
	}
}

bool Plugin::CheckNeeded(FSMVariables & FSM_Var, Elite::Vector2 itemPos)
{
	for (size_t i = 0; i < FSM_Var.knownItems.size(); ++i)
	{
		if (FSM_Var.knownItems[i].pos == itemPos && !FSM_Var.knownItems[i].needed)
		{
			return false;
		}
	}
	return true;
}

float Plugin::GetAngleBetween(Elite::Vector2 agentDir, Elite::Vector2 otherDir, bool rad)
{
	double angle = 0.0f;

	float dot = Elite::Dot(agentDir, otherDir);
	float magA = agentDir.Magnitude();
	float magB = otherDir.Magnitude();
	angle = acos(dot / (magA * magB));

	if (!rad)
	{
		angle *= (180.0 / M_PI);
	}

	Elite::Vector3 v1 = Elite::Vector3(agentDir.x, agentDir.y, 0);
	Elite::Vector3 v2 = Elite::Vector3(otherDir.x, otherDir.y, 0);

	if (Elite::Cross(v1, v2).z < 0)
	{
		angle *= -1;
	}

	return float(angle);
}

vector<HouseInfo> Plugin::GetHousesInFOV(FSMVariables& FSM_Var)
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (FSM_Var.interface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV(FSMVariables& FSM_Var)
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (FSM_Var.interface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}

void Plugin::InitializeFSM()
{
	//FSM
	//***
	//Condition lambda's
	auto hungry = [this](const FSMVariables& FSM) -> bool
	{
		if (/*FSM.agentEnergy <= FSM.energyThreshold && */!HasFood())
		{
			return true;
		}
		return false;
	};

	auto notHungry = [this](const FSMVariables& FSM) -> bool
	{
		if (FSM.agentEnergy > FSM.energyThreshold && HasFood())
		{
			return true;
		}
		return false;	};

	auto enemyInSight = [](const FSMVariables& FSM) -> bool
	{
		bool result = false;
		if (FSM.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM.entities.size(); ++i)
			{
				if (FSM.entities[i].Type == eEntityType::ENEMY)
				{
					result = true;
				}
			}
		}
		return result;
	};

	auto noEnemyInSight = [](const FSMVariables& FSM) -> bool
	{
		bool result = true;
		if (FSM.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM.entities.size(); ++i)
			{
				if (FSM.entities[i].Type == eEntityType::ENEMY)
				{
					result = false;
				}
			}
		}
		return result;
	};

	auto houseInSight = [](const FSMVariables& FSM) -> bool
	{
		return FSM.houses.size() > 0;
	};

	auto houseSafe = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.houses.size() > 0 && FSM.knownHouses.size() > 0)
		{
			for (size_t i = 0; i < FSM.houses.size(); ++i)
			{
				for (size_t j = 0; j < FSM.knownHouses.size(); j++)
				{
					if (FSM.houses[i].Center == FSM.knownHouses[j].pos && FSM.knownHouses[j].tag == HouseTag::SAFE)
					{
						return true;
					}
				}
			}
		}
		return false;
	};

	auto houseSearched = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.houses.size() > 0 && FSM.knownHouses.size() > 0)
		{
			for (size_t i = 0; i < FSM.houses.size(); ++i)
			{
				for (size_t j = 0; j < FSM.knownHouses.size(); j++)
				{
					if (FSM.houses[i].Center == FSM.knownHouses[j].pos && !FSM.knownHouses[j].searched)
					{
						return true;
					}
				}
			}
		}
		return false;
	};


	auto itemInSight = [](const FSMVariables& FSM) -> bool
	{
		bool result = false;
		if (FSM.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM.entities.size(); ++i)
			{
				if (FSM.entities[i].Type == eEntityType::ITEM)
				{
					result = true;
				}
			}
		}
		return result;
	};

	auto noItemInSight = [](const FSMVariables& FSM) -> bool
	{
		bool result = true;
		if (FSM.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM.entities.size(); ++i)
			{
				if (FSM.entities[i].Type == eEntityType::ITEM)
				{
					result = false;
				}
			}
		}
		return result;
	};

	auto doneHiding = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.hideTime)
		{
			return true;
		}
		return false;
	};

	auto doneLooking = [hungry](FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.scavengeTime && hungry(FSM))
		{
			SetHouseAsSearched(FSM);

			return true;
		}
		return false;
	};

	auto doneGetting = [hungry](FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.getTime)
		{
			return true;
		}
		return false;
	};

	auto hasGun = [](const FSMVariables& FSM) -> bool
	{
		bool result = false;

		for (size_t i = 0; i < FSM.inventorySlots.size(); ++i)
		{
			if (FSM.inventorySlots[i].itemType == INVENTORY::PISTOL)
			{
				result = true;
			}
		}

		return result;
	};

	auto bitten = [](const FSMVariables& FSM) -> bool
	{
		return FSM.bitten;
	};

	auto doneRunning = [](const FSMVariables& FSM) -> bool 
	{
		if (FSM.timer >= FSM.runTime)
		{
			return true;
		}

		return false;
	};

	auto doneFighting = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.fightTime)
		{
			return true;
		}

		return false;
	};

	auto getFailed = [](FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.clearItemTime)
		{
			cout << "Get Failed" << endl;

			RemoveItem(FSM, GetClosestItem(FSM));
			return true;
		}

		return false;
	};

	auto inside = [](const FSMVariables& FSM) -> bool
	{
		return FSM.inHouse;
	};

	auto tooFar = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.agentPosition.Magnitude() > Elite::Vector2(160.0f, 160.0f).Magnitude())
		{
			return true;
		}
		return false;
	};

	auto notTooFar = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.agentPosition.Magnitude() < 100.0f)
		{
			return true;
		}
		return false;
	};

	auto ignoreDone = [](const FSMVariables& FSM) -> bool
	{
		if (FSM.timer >= FSM.ignoreTime)
		{
			return true;
		}
		return false;
	};

	//Combined Lambda's
	auto hasGun_EnemyInSight = [hasGun, enemyInSight](const FSMVariables& FSM) -> bool
	{
		if (hasGun(FSM) && enemyInSight(FSM))
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	auto hasGun_EnemyInSightPas = [hasGun, enemyInSight](const FSMVariables& FSM) -> bool
	{
		if (hasGun(FSM) && enemyInSight(FSM))
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	auto NOTHasGun_EnemyInSight = [hasGun, enemyInSight](const FSMVariables& FSM) -> bool
	{
		if (!hasGun(FSM))
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	auto enemyInSight_NoGun = [hasGun, enemyInSight](const FSMVariables& FSM) -> bool
	{
		if (!hasGun(FSM) && enemyInSight(FSM))
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	auto safeHouseInSight = [houseInSight, houseSafe, ignoreDone](const FSMVariables& FSM) -> bool
	{
		if (houseInSight(FSM) && houseSafe(FSM) && ignoreDone(FSM))
		{
			return true;
		}
		return false;
	};

	auto insideUnsearched = [houseInSight, houseSafe, houseSearched](const FSMVariables& FSM) -> bool
	{
		if (houseInSight(FSM) && houseSafe(FSM) && houseSearched(FSM))
		{
			return true;
		}
		return false;
	};

	auto enemyInside = [enemyInSight](const FSMVariables& FSM) -> bool
	{
		if (enemyInSight(FSM) && FSM.inHouse)
		{
			return true;
		}
		return false;
	};

	//Action lambda's
	auto transToHide = [](FSMVariables& FSM_Var)
	{
		cout << "State: Hide" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 4.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;
	};

	auto transToFlee = [](FSMVariables& FSM_Var)
	{
		if (FSM_Var.inHouse)
		{
			SetHouseAsDanger(FSM_Var);
		}

		cout << "State: Flee" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 5.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToWalk = [this](FSMVariables& FSM_Var)
	{
		cout << "State: Walk" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;
		FSM_Var.steering.AngularVelocity = 0.0f;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.lookingBack = false;
	};

	auto transAfterHide = [this, hungry](FSMVariables& FSM_Var)
	{
		if (hungry(FSM_Var))
		{
			SetHouseAsSearched(FSM_Var);
		}

		SetHouseAsRecent(FSM_Var);
	};


	auto transToFightAgg = [](FSMVariables& FSM_Var)
	{
		if (FSM_Var.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
			{
				if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
				{
					FSM_Var.destination = FSM_Var.entities[i].Location;
					FSM_Var.hasDestination = true;
				}
			}
		}

		cout << "State: Fight Aggressive" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		//FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToFightPas = [](FSMVariables& FSM_Var)
	{
		cout << "State: Fight Passive" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = false;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToScavenge = [](FSMVariables& FSM_Var)
	{
		cout << "State: Scavenge" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = false;
		FSM_Var.hasDestination = false;
		FSM_Var.steering.AngularVelocity = 0.0f;

		FSM_Var.staminaThreshold = 8.0f;
		FSM_Var.knowHouse = false;
	};

	auto transToGetItem = [](FSMVariables& FSM_Var)
	{
		if (FSM_Var.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
			{
				if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location))
				{
					FSM_Var.destination = FSM_Var.entities[i].Location;
					FSM_Var.hasDestination = true;
					break;
				}
			}
		}

		cout << "State: Get Item" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		//FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToBitten = [](FSMVariables& FSM_Var)
	{
		if (FSM_Var.inHouse)
		{
			SetHouseAsDanger(FSM_Var);
		}

		cout << "State: Bitten" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 0.0f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToLookForItem = [](FSMVariables& FSM_Var)
	{
		cout << "State: Look For Item" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = false;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToReturn = [](FSMVariables& FSM_Var)
	{
		cout << "State: Return" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;

		FSM_Var.staminaThreshold = 9.5f;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToHeal = [](FSMVariables& FSM_Var)
	{
		cout << "State: Heal" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	auto transToEat = [](FSMVariables& FSM_Var)
	{
		cout << "State: Eat" << endl;

		FSM_Var.timer = 0.0f;
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.hasDestination = false;
		FSM_Var.steering.AngularVelocity = 0.0f;

	};

	//Transition Actions
	FSMDelegateBase* pTransWalkFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToWalk, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransWalkFunc);

	FSMDelegateBase* pTransAfterHideFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transAfterHide, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransAfterHideFunc);

	FSMDelegateBase* pTransFleeFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToFlee, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransFleeFunc);

	FSMDelegateBase* pTransHideFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToHide, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransHideFunc);

	FSMDelegateBase* pTransBittenFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToBitten, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransBittenFunc);

	FSMDelegateBase* pTransScavengeFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToScavenge, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransScavengeFunc);

	FSMDelegateBase* pTransFightAggressiveFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToFightAgg, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransFightAggressiveFunc);

	FSMDelegateBase* pTransFightPassiveFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToFightPas, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransFightPassiveFunc);

	FSMDelegateBase* pTransReturnFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToReturn, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransReturnFunc);

	FSMDelegateBase* pTransGetItemFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToGetItem, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransGetItemFunc);

	FSMDelegateBase* pTransLookForItemFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToLookForItem, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransLookForItemFunc);

	FSMDelegateBase* pTransHealFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToHeal, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransHealFunc);

	FSMDelegateBase* pTransEatFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(transToEat, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pTransEatFunc);

	//Transition Conditions
	FSMConditionBase* pConditionHungry = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(hungry, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionHungry);

	FSMConditionBase* pConditionNotHungry = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(notHungry, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionNotHungry);

	FSMConditionBase* pConditionEnemyInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(enemyInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionEnemyInSight);

	FSMConditionBase* pConditionTooFar = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(tooFar, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionTooFar);

	FSMConditionBase* pConditionGetFailed = new FSMCondition<FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, FSMVariables&>(getFailed, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionGetFailed);

	FSMConditionBase* pConditionNotTooFar = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(notTooFar, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionNotTooFar);

	FSMConditionBase* pConditionNoEnemyInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(noEnemyInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionNoEnemyInSight);

	FSMConditionBase* pConditionEnemyInside = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(enemyInside, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionEnemyInside);

	FSMConditionBase* pConditionNoGun = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(NOTHasGun_EnemyInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionNoGun);

	FSMConditionBase* pConditionItemInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(itemInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionItemInSight);

	FSMConditionBase* pConditionNoItemInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(noItemInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionNoItemInSight);

	FSMConditionBase* pConditionHouseInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(houseInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionHouseInSight);

	FSMConditionBase* pConditionDoneRunning = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(doneRunning, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionDoneRunning);

	FSMConditionBase* pConditionSafeHouseInSight = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(safeHouseInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionSafeHouseInSight);

	FSMConditionBase* pConditionHasGun = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(hasGun, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionHasGun);

	FSMConditionBase* pConditionCanShoot = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(hasGun_EnemyInSight, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionCanShoot);

	FSMConditionBase* pConditionCanShootPas = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(hasGun_EnemyInSightPas, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionCanShootPas);

	FSMConditionBase* pConditionInside = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(inside, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionInside);

	FSMConditionBase* pConditionEnemyNoGun = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(enemyInSight_NoGun, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionEnemyNoGun);

	FSMConditionBase* pConditionInsideUnsearched = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(insideUnsearched, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionInsideUnsearched);

	FSMConditionBase* pConditionBitten = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(bitten, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionBitten);

	FSMConditionBase* pConditionDoneHiding = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(doneHiding, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionDoneHiding);

	FSMConditionBase* pConditionDoneFighting = new FSMCondition<const FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, const FSMVariables&>(doneFighting, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionDoneFighting);

	FSMConditionBase* pConditionDoneLooking = new FSMCondition<FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, FSMVariables&>(doneLooking, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionDoneLooking);

	FSMConditionBase* pConditionDoneGetting = new FSMCondition<FSMVariables&>
		(
			{
				FSMDelegateContainer<bool, FSMVariables&>(doneGetting, m_FSMVariables)
			}
	);
	m_Conditions.push_back(pConditionDoneGetting);

	//Define Actions
	FSMDelegateBase* pWalkFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(MoveToCheckpoint, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pWalkFunc);

	FSMDelegateBase* pFleeFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Flee, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pFleeFunc);

	FSMDelegateBase* pHideFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Hide, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pHideFunc);

	FSMDelegateBase* pBittenFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Bitten, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pBittenFunc);

	FSMDelegateBase* pScavengeFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Scavenge, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pScavengeFunc);

	FSMDelegateBase* pFightAggressiveFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(FightAggressive, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pFightAggressiveFunc);

	FSMDelegateBase* pFightPassiveFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(FightPassive, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pFightPassiveFunc);

	FSMDelegateBase* pReturnFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Return, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pReturnFunc);

	FSMDelegateBase* pGetItemFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(GetItem, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pGetItemFunc);

	FSMDelegateBase* pLookForItemFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(LookForItem, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pLookForItemFunc);

	FSMDelegateBase* pHealFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Healing, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pHealFunc);

	FSMDelegateBase* pEatFunc = new FSMDelegate<FSMVariables&>
		(
			{
				FSMDelegateContainer<void, FSMVariables&>(Eating, m_FSMVariables)
			}
	);
	m_Delegates.push_back(pEatFunc);

	//Define States
	FSMState* pWalkState = new FSMState();
	m_States.push_back(pWalkState);

	FSMState* pBittenState = new FSMState();
	m_States.push_back(pBittenState);

	FSMState* pFleeState = new FSMState();
	m_States.push_back(pFleeState);

	FSMState* pHideState = new FSMState();
	m_States.push_back(pHideState);

	FSMState* pGetItemState = new FSMState();
	m_States.push_back(pGetItemState);

	FSMState* pScavengeState = new FSMState();
	m_States.push_back(pScavengeState);

	FSMState* pLookForItemState = new FSMState();
	m_States.push_back(pLookForItemState);

	FSMState* pReturnState = new FSMState();
	m_States.push_back(pReturnState);

	FSMState* pHealState = new FSMState();
	m_States.push_back(pHealState);

	FSMState* pEatState = new FSMState();
	m_States.push_back(pEatState);

	FSMState* pFightAggressiveState = new FSMState();
	m_States.push_back(pFightAggressiveState);

	FSMState* pFightPassiveState = new FSMState();
	m_States.push_back(pFightPassiveState);

	//Build States
	pWalkState->SetActions({ pWalkFunc });
	pWalkState->SetTransitions({
		FSMTransition({ pConditionCanShootPas },{ pTransFightPassiveFunc }, pFightPassiveState),
		FSMTransition({ pConditionCanShoot }, { pTransFightAggressiveFunc }, pFightAggressiveState),
		FSMTransition({ pConditionEnemyNoGun }, { pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionHungry },{ pTransScavengeFunc }, pScavengeState),
		FSMTransition({ pConditionItemInSight },{ pTransGetItemFunc }, pGetItemState),
		FSMTransition({ pConditionBitten }, { pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pFleeState->SetActions({ pFleeFunc });
	pFleeState->SetTransitions({
		FSMTransition({ pConditionSafeHouseInSight },{ pTransHideFunc }, pHideState),
		FSMTransition({ pConditionCanShootPas },{ pTransFightPassiveFunc }, pFightPassiveState),
		FSMTransition({ pConditionCanShoot },{ pTransFightAggressiveFunc }, pFightAggressiveState),
		FSMTransition({ pConditionDoneRunning },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pHideState->SetActions({ pHideFunc });
	pHideState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionDoneHiding },{ pTransWalkFunc, pTransAfterHideFunc }, pWalkState),
		FSMTransition({ pConditionEnemyInside }, { pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionItemInSight },{ pTransGetItemFunc }, pGetItemState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pBittenState->SetActions({ pBittenFunc });
	pBittenState->SetTransitions({
		FSMTransition({ pConditionSafeHouseInSight },{ pTransHideFunc }, pHideState),
		FSMTransition({ pConditionDoneRunning },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pFightAggressiveState->SetActions({ pFightAggressiveFunc });
	pFightAggressiveState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionDoneFighting },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionNoGun },{ pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pFightPassiveState->SetActions({ pFightPassiveFunc });
	pFightPassiveState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionNoEnemyInSight },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionNoGun },{ pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pReturnState->SetActions({ pReturnFunc });
	pReturnState->SetTransitions({
		FSMTransition({ pConditionNotTooFar },{ pTransWalkFunc }, pWalkState)
		});

	pHealState->SetActions({ pHealFunc });
	pHealState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState)
		});

	pEatState->SetActions({ pEatFunc });
	pEatState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState)
		});

	pScavengeState->SetActions({ pScavengeFunc });
	pScavengeState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionNotHungry, pConditionGetFailed },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionCanShootPas },{ pTransFightPassiveFunc }, pFightPassiveState),
		FSMTransition({ pConditionCanShoot },{ pTransFightAggressiveFunc }, pFightAggressiveState),
		FSMTransition({ pConditionEnemyNoGun },{ pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionItemInSight },{ pTransGetItemFunc }, pGetItemState),
		FSMTransition({ pConditionInsideUnsearched },{ pTransLookForItemFunc }, pLookForItemState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pGetItemState->SetActions({ pGetItemFunc });
	pGetItemState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionDoneGetting },{ pTransWalkFunc }, pWalkState),
		FSMTransition({ pConditionCanShootPas },{ pTransFightPassiveFunc }, pFightPassiveState),
		FSMTransition({ pConditionCanShoot },{ pTransFightAggressiveFunc }, pFightAggressiveState),
		FSMTransition({ pConditionEnemyNoGun },{ pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	pLookForItemState->SetActions({ pLookForItemFunc });
	pLookForItemState->SetTransitions({
		FSMTransition({ pConditionBitten },{ pTransBittenFunc }, pBittenState),
		FSMTransition({ pConditionDoneLooking },{ pTransScavengeFunc }, pScavengeState),
		FSMTransition({ pConditionCanShootPas },{ pTransFightPassiveFunc }, pFightPassiveState),
		FSMTransition({ pConditionCanShoot },{ pTransFightAggressiveFunc }, pFightAggressiveState),
		FSMTransition({ pConditionItemInSight },{ pTransGetItemFunc }, pGetItemState),
		FSMTransition({ pConditionEnemyNoGun },{ pTransFleeFunc }, pFleeState),
		FSMTransition({ pConditionTooFar },{ pTransReturnFunc }, pReturnState)
		});

	//Create State Machine
	m_pStateMachine = new FSM(
		{ 
			pWalkState, pFleeState, pHideState, pScavengeState, 
			pFightAggressiveState, pFightPassiveState, pGetItemState, 
			pLookForItemState, pBittenState, pReturnState, pHealState, 
			pEatState },
			pWalkState);

	m_pStateMachine->Start();
}

void Plugin::MoveToCheckpoint(FSMVariables& FSM_Var)
{
	if (FSM_Var.lookingBack)
	{
		FSM_Var.steering.LinearVelocity = FSM_Var.checkpointDirection - FSM_Var.agentPosition;
		FSM_Var.steering.LinearVelocity *= -1;
		FSM_Var.steering.LinearVelocity.Normalize();
		FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

		if (FSM_Var.timer >= FSM_Var.turnTime)
		{
			FSM_Var.timer = 0.0f;
			FSM_Var.lookingBack = false;
		}
		else
		{
			FSM_Var.timer += FSM_Var.dt;
		}
	}
	else
	{
		//Set Checkpoint as target
		FSM_Var.steering.LinearVelocity = FSM_Var.checkpointDirection - FSM_Var.agentPosition;
		FSM_Var.steering.LinearVelocity.Normalize();
		FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

		if (FSM_Var.timer >= FSM_Var.turnIntervalTime)
		{
			FSM_Var.timer = 0.0f;
			FSM_Var.lookingBack = true;
		}
		else
		{
			FSM_Var.timer += FSM_Var.dt;
		}
	}

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}
}

void Plugin::Flee(FSMVariables& FSM_Var)
{
	//if (FSM_Var.timer >= FSM_Var.ignoreTime || !FSM_Var.hasDestination)
	//{
		if (FSM_Var.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
			{
				if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
				{
					if (FSM_Var.inHouse)
					{
						//cout << "Cycle to check" << endl;
						Elite::Vector2 toCheck = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.checkpointDirection);
						Elite::Vector2 run = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.agentPosition + ((FSM_Var.agentPosition - FSM_Var.entities[i].Location).GetNormalized() * 50));

						if (GetAngleBetween(toCheck, run) >= 120.0f)
						{
							FSM_Var.destination = toCheck;
						}
						else
						{
							FSM_Var.destination = run;
						}
					}
					else
					{
						//cout << "Cycle away" << endl;

						FSM_Var.destination = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.agentPosition + ((FSM_Var.agentPosition - FSM_Var.entities[i].Location).GetNormalized() * 50));
					}


					FSM_Var.timer = 0.0f;
					FSM_Var.hasDestination = true;
				}
				else if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location) && (FSM_Var.entities[i].Location - FSM_Var.agentPosition).Magnitude() < FSM_Var.grabRange)
				{
					ItemInfo item;


					if (FSM_Var.interface->Item_Grab(FSM_Var.entities[i], item))
					{
						AddItem(item, FSM_Var);
						FSM_Var.entities = GetEntitiesInFOV(FSM_Var);

						break;
					}
					else
					{
						int a = 42;
					}
				}
			}
		}
	//}

		FSM_Var.timer += FSM_Var.dt;
	

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}

		FSM_Var.steering.LinearVelocity = FSM_Var.destination - FSM_Var.agentPosition;
		FSM_Var.steering.LinearVelocity.Normalize();
		FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

}

void Plugin::Hide(FSMVariables& FSM_Var)
{
	if (FSM_Var.hasDestination)
	{
		if ((FSM_Var.destination - FSM_Var.agentPosition).Magnitude() < 5.0f)
		{
			FSM_Var.steering.LinearVelocity = { 0, 0 };
			FSM_Var.steering.AngularVelocity = FSM_Var.turnspeed;
			FSM_Var.timer += FSM_Var.dt;

		}
		else
		{
			FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
			FSM_Var.steering.LinearVelocity.Normalize();
			FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

			if (FSM_Var.inHouse)
			{
				FSM_Var.steering.AngularVelocity = FSM_Var.turnspeed;
				FSM_Var.steering.AutoOrientate = false;
			}
			else
			{
				FSM_Var.steering.AngularVelocity = 0.0f;
				FSM_Var.steering.AutoOrientate = true;
			}
		}
	}
	else
	{
		if (FSM_Var.houses.size() > 0)
		{
			FSM_Var.destination = FSM_Var.houses[0].Center;
			FSM_Var.hasDestination = true;
		}
	}

	if (FSM_Var.entities.size() > 0 && FSM_Var.inHouse)
	{
		for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
		{
			if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
			{
				if (FSM_Var.knownHouses.size() > 1)
				{
					int closest = 0;
					for (size_t i = 1; i < FSM_Var.knownHouses.size(); ++i)
					{
						if ((FSM_Var.knownHouses[closest].pos - FSM_Var.agentPosition).Magnitude() >(FSM_Var.knownHouses[i].pos - FSM_Var.agentPosition).Magnitude())
						{
							closest = i;
						}
					}

					if (FSM_Var.knownHouses[closest].tag != HouseTag::DANGER)
					{
						cout << "Set House As Danger" << endl;

						FSM_Var.knownHouses[closest].tag = HouseTag::DANGER;
					}
				}
				else if (FSM_Var.knownHouses.size() == 1)
				{
					if (FSM_Var.knownHouses[0].tag != HouseTag::DANGER)
					{
						cout << "Set House As Danger" << endl;

						FSM_Var.knownHouses[0].tag = HouseTag::DANGER;
					}
				}
			}
			else if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location) && (FSM_Var.entities[i].Location - FSM_Var.agentPosition).Magnitude() < FSM_Var.grabRange)
			{
				ItemInfo item;


				if (FSM_Var.interface->Item_Grab(FSM_Var.entities[i], item))
				{
					AddItem(item, FSM_Var);
					FSM_Var.entities = GetEntitiesInFOV(FSM_Var);

					break;
				}
				else
				{
					int a = 42;
				}
			}
		}
	}
}

void Plugin::Bitten(FSMVariables & FSM_Var)
{
	//if (FSM_Var.timer >= FSM_Var.ignoreTime || !FSM_Var.hasDestination)
	//{
	if (FSM_Var.entities.size() > 0)
	{
		for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
		{
			if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
			{
				if (FSM_Var.inHouse)
				{
					//cout << "Cycle to check" << endl;
					Elite::Vector2 run = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.agentPosition + ((FSM_Var.agentPosition - FSM_Var.entities[i].Location).GetNormalized() * 25.0f));

						FSM_Var.destination = run;
					
				}
				else
				{
					//cout << "Cycle away" << endl;

					FSM_Var.destination = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.agentPosition + ((FSM_Var.agentPosition - FSM_Var.entities[i].Location).GetNormalized() * 50));
				}


				FSM_Var.timer = 0.0f;
				FSM_Var.hasDestination = true;
			}
			else if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location) && (FSM_Var.entities[i].Location - FSM_Var.agentPosition).Magnitude() < FSM_Var.grabRange)
			{
				ItemInfo item;

				if (!FSM_Var.interface->Item_Grab(FSM_Var.entities[i], item))
				{
					int a = 42;
				}

				AddItem(item, FSM_Var);
			}
		}
	}
	//}

	FSM_Var.timer += FSM_Var.dt;


	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}

	FSM_Var.steering.LinearVelocity = FSM_Var.destination - FSM_Var.agentPosition;
	FSM_Var.steering.LinearVelocity.Normalize();
	FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

}

void Plugin::Scavenge(FSMVariables& FSM_Var)
{
	if (!FSM_Var.hasDestination)
	{
		FSM_Var.destination = GetClosestItem(FSM_Var);
	}

	FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
	FSM_Var.steering.LinearVelocity.Normalize();
	FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

	if ((FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition).Magnitude() < 1.0f)
	{
		FSM_Var.destination = GetClosestItem(FSM_Var);

		//cout << (FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition).Magnitude() << endl;
		FSM_Var.steering.LinearVelocity = Elite::Vector2(0,0);
		FSM_Var.timer += FSM_Var.dt;
	}
	else
	{
		FSM_Var.timer = 0.0f;
	}

	//if (FSM_Var.hasDestination)
	//{
	//	FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
	//	FSM_Var.steering.LinearVelocity.Normalize();
	//	FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;
	//}
	//else
	//{
	//	FSM_Var.destination = GetClosestHouseToSearch(FSM_Var);
	//	FSM_Var.hasDestination = true;
	//}

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}

	if (FSM_Var.inHouse)
	{
		FSM_Var.steering.AutoOrientate = false;
		FSM_Var.steering.AngularVelocity = FSM_Var.turnspeed;
	}
	else
	{
		FSM_Var.steering.AutoOrientate = true;
	}
}

void Plugin::Healing(FSMVariables& FSM_Var)
{

}

void Plugin::Eating(FSMVariables& FSM_Var)
{

}

void Plugin::Return(FSMVariables& FSM_Var)
{
	FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.checkpointDirection) - FSM_Var.agentPosition;
	FSM_Var.steering.LinearVelocity.Normalize();
	FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}
}

void Plugin::LookForItem(FSMVariables& FSM_Var)
{
	if (FSM_Var.hasDestination)
	{
		if ((FSM_Var.destination - FSM_Var.agentPosition).Magnitude() < 5.0f)
		{
			FSM_Var.steering.LinearVelocity = { 0, 0 };
		}
		else
		{
			FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
			FSM_Var.steering.LinearVelocity.Normalize();
			FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;
		}
	}
	else
	{
		if (FSM_Var.houses.size() > 0)
		{
			FSM_Var.destination = FSM_Var.houses[0].Center;
			FSM_Var.hasDestination = true;
		}
		else
		{
			FSM_Var.hasDestination = false;
			
			cout << "No house in sight" << endl;
		}
	}

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}

	if (FSM_Var.inHouse)
	{
		FSM_Var.timer += FSM_Var.dt;
		FSM_Var.steering.AngularVelocity = FSM_Var.turnspeed;
		FSM_Var.steering.AutoOrientate = false;
	}
	else
	{
		FSM_Var.steering.AutoOrientate = true;
		FSM_Var.steering.AngularVelocity = 0;
	}
}

void Plugin::GetItem(FSMVariables& FSM_Var)
{
	bool toggle = false;

	if (FSM_Var.hasDestination)
	{
		FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
		FSM_Var.steering.LinearVelocity.Normalize();
		FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;	

		if (FSM_Var.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
			{
				if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location))
				{
					if ((FSM_Var.entities[i].Location - FSM_Var.agentPosition).Magnitude() < FSM_Var.grabRange)
					{
						ItemInfo item;

						if (FSM_Var.canGrab && FSM_Var.interface->Item_Grab(FSM_Var.entities[i], item))
						{
							AddItem(item, FSM_Var);
							toggle = true;

							break;
						}
						else
						{
							int a = 42;
						}

					}
					else
					{
						FSM_Var.destination = FSM_Var.entities[i].Location;
					}
				}
			}
		}
	}
	else
	{
		if (FSM_Var.entities.size() > 0)
		{
			for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
			{
				if (FSM_Var.entities[i].Type == eEntityType::ITEM && CheckNeeded(FSM_Var, FSM_Var.entities[i].Location))
				{
					FSM_Var.destination = FSM_Var.entities[i].Location;
					FSM_Var.hasDestination = true;
				}
			}
		}
		else
		{
			FSM_Var.hasDestination = false;

			//cout << "No item in sight" << endl;
		}
	}

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}

	bool itemSeen = false;
	if (FSM_Var.entities.size() > 0)
	{
		for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
		{
			if (FSM_Var.entities[i].Type == eEntityType::ITEM)
			{
				itemSeen = true;
			}
		}
	}
	if (!itemSeen)
	{
		FSM_Var.timer += FSM_Var.dt;
	}
	else
	{
		FSM_Var.timer = 0.0f;
	}

	if (toggle)
	{
		FSM_Var.canGrab = false;
	}
	else
	{
		FSM_Var.canGrab = true;
	}
}

void Plugin::FightAggressive(FSMVariables& FSM_Var)
{
	if (FSM_Var.entities.size() > 0)
	{
		for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
		{
			if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
			{
				FSM_Var.timer = 0.0f;
				//cout << "Gonna Shoot" << endl;

				FSM_Var.destination = FSM_Var.entities[i].Location;

				EnemyInfo enemy;
				FSM_Var.interface->Enemy_GetInfo(FSM_Var.entities[i], enemy);

				Shoot(enemy, FSM_Var);
			}
			else if (FSM_Var.entities[i].Type == eEntityType::ITEM && (FSM_Var.entities[i].Location - FSM_Var.agentPosition).Magnitude() < FSM_Var.grabRange)
			{
				//cout << "Item during combat" << endl;

				ItemInfo item;

				if (FSM_Var.interface->Item_Grab(FSM_Var.entities[i], item))
				{
					AddItem(item, FSM_Var);

					FSM_Var.entities = GetEntitiesInFOV(FSM_Var);

					break;
				}
				else
				{
					int a = 42;
				}
			}
		}


	}
	else
	{
		FSM_Var.timer += FSM_Var.dt;
		//cout << "No Entities" << endl;
	}
		FSM_Var.canRun = false;	

		FSM_Var.steering.LinearVelocity = FSM_Var.interface->NavMesh_GetClosestPathPoint(FSM_Var.destination) - FSM_Var.agentPosition;
		FSM_Var.steering.LinearVelocity.Normalize();
		FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;
}

void Plugin::FightPassive(FSMVariables& FSM_Var)
{
	if (FSM_Var.entities.size() > 0)
	{
		for (size_t i = 0; i < FSM_Var.entities.size(); ++i)
		{
			if (FSM_Var.entities[i].Type == eEntityType::ENEMY)
			{
				FSM_Var.steering.AngularVelocity = Clamp(GetAngleBetween(GetForward(FSM_Var), FSM_Var.entities[i].Location - FSM_Var.agentPosition, true) * 10.0f, -FSM_Var.turnspeed, FSM_Var.turnspeed);

				EnemyInfo enemy;
				FSM_Var.interface->Enemy_GetInfo(FSM_Var.entities[i], enemy);

				Shoot(enemy, FSM_Var);
			}
		}
	}
	else
	{
		cout << "Nothing here..." << endl;
		FSM_Var.steering.AngularVelocity = 0;
	}

	FSM_Var.steering.LinearVelocity = FSM_Var.checkpointDirection - FSM_Var.agentPosition;
	FSM_Var.steering.LinearVelocity.Normalize();
	FSM_Var.steering.LinearVelocity *= FSM_Var.moveSpeed;

	if (FSM_Var.agentStamina > FSM_Var.staminaThreshold)
	{
		FSM_Var.canRun = true;
	}
	else
	{
		FSM_Var.canRun = false;
	}
}
