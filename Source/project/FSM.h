#ifndef FSM_H
#define FSM_H

#pragma region ***DELEGATE - CONDITION***
//-----------------------------------------------------------------
// DELEGATE CONTAINER (BASE)
//----------------------------------------------------------------
template<typename ret, typename... args>
struct FSMDelegateContainer
{
	std::function<ret(args...)> func;
	std::tuple< args... > arguments;

	explicit FSMDelegateContainer(std::function<ret(args...)> f, args... argumentList):
		func(f), arguments(std::tuple<args...>(argumentList...))
	{}
};

//-----------------------------------------------------------------
// BASES (...)
//----------------------------------------------------------------
class FSMDelegateBase
{
public:
	virtual ~FSMDelegateBase()
	{}
	virtual void Invoke() {};
};
class FSMConditionBase
{
public:
	virtual ~FSMConditionBase()
	{}
	virtual bool Invoke() { return false; };
};

template<typename ret, typename... args>
class FSMDelegateDataBase
{
protected:
	std::vector < FSMDelegateContainer<ret, args...>> m_functions;

	template<std::size_t... Is>
	ret CallFunction(std::function<ret(args...)>& func, const std::tuple<args...>& tuple, std::index_sequence<Is...>)
	{
		return func(std::get<Is>(tuple)...);
	}
};

//-----------------------------------------------------------------
// DELEGATE CLASS (DELEGATE BASE)
//----------------------------------------------------------------
template<typename... args>
class FSMDelegate : public FSMDelegateBase, private FSMDelegateDataBase<void, args...>
{
public:
	virtual ~FSMDelegate()
	{}
	explicit FSMDelegate(std::vector<FSMDelegateContainer<void, args...>> dc)
	{
		for (auto c : dc)
			FSMDelegateDataBase<void, args...>::m_functions.push_back(c);
	}
	explicit FSMDelegate()
	{}

	void Invoke() override
	{
		for (FSMDelegateContainer<void, args...> f : FSMDelegateDataBase<void, args...>::m_functions)
		{
			CallFunction(f.func, f.arguments, std::index_sequence_for<args...>());
		}
	}

	auto Assign(FSMDelegateContainer<void, args...> dc) -> void
	{
		FSMDelegateDataBase<void, args...>::m_functions.push_back(dc);
	}
};

//-----------------------------------------------------------------
// CONDITION CLASS (DELEGATE)
//-----------------------------------------------------------------
template<typename... args>
class FSMCondition : public FSMConditionBase, private FSMDelegateDataBase<bool, args...>
{
public:
	explicit FSMCondition(std::vector<FSMDelegateContainer<bool, args...>> dc)
	{
		for(auto c : dc)
			FSMDelegateDataBase<bool, args...>::m_functions.push_back(c);
	}
	explicit FSMCondition()
	{}
	virtual ~FSMCondition() 
	{};

	bool Invoke() override
	{
		bool returnValue = 0;
		for (FSMDelegateContainer<bool, args...> f : FSMDelegateDataBase<bool, args...>::m_functions)
		{
			returnValue |= CallFunction(f.func, f.arguments, std::index_sequence_for<args...>());
		}
		return returnValue;
	}

	auto Assign(FSMDelegateContainer<bool, args...> dc) -> void
	{
		FSMDelegateDataBase<bool, args...>::m_functions.push_back(dc);
	}
};
#pragma endregion

//-----------------------------------------------------------------
//TYPEDEF'S
//-----------------------------------------------------------------
typedef FSMDelegateBase* FSMDelegateBasePtr;
typedef FSMConditionBase* FSMConditionBasePtr;

//-----------------------------------------------------------------
// STATE CLASS
//-----------------------------------------------------------------
class FSMTransition;
class FSMState
{
public:
	FSMState(std::vector<FSMDelegateBasePtr> entryActions, std::vector<FSMDelegateBasePtr> actions,
		std::vector<FSMDelegateBasePtr> exitActions, std::vector<FSMTransition> transitions):
		m_EntryActions(entryActions), m_Actions(actions), m_ExitActions(exitActions), m_Transitions(transitions)
	{}
	FSMState()
	{}
	virtual ~FSMState()
	{
		m_EntryActions.clear();
		m_Actions.clear();
		m_ExitActions.clear();
		m_Transitions.clear();
	}

	//Run actions
	void RunActions() const { for (auto a : m_Actions) a->Invoke(); }; //The actions that should run when active
	void RunEntryActions() const { for (auto a : m_EntryActions) a->Invoke(); }; //The action that should run when entering this state
	void RunExitActions() const { for (auto a : m_ExitActions) a->Invoke(); }; //The action that should run when exiting this state

	//Setters
	void SetEntryActions(std::vector<FSMDelegateBasePtr> entryActions)
	{ m_EntryActions = entryActions; }

	void SetActions(std::vector<FSMDelegateBasePtr> actions)
	{ m_Actions = actions; }

	void SetExitActions(std::vector<FSMDelegateBasePtr> exitActions)
	{ m_ExitActions = exitActions; }

	void SetTransitions(std::vector<FSMTransition> transitions)
	{ m_Transitions = transitions; }

	//Getters
	std::vector<FSMTransition> GetTransitions() { return m_Transitions; };

private:
	std::vector<FSMDelegateBasePtr> m_EntryActions = {};
	std::vector<FSMDelegateBasePtr> m_Actions = {};
	std::vector<FSMDelegateBasePtr> m_ExitActions = {};
	std::vector<FSMTransition> m_Transitions = {};
};

//-----------------------------------------------------------------
// TRANSITION CLASS
//-----------------------------------------------------------------
class FSMTransition
{
public:
	FSMTransition(std::vector<FSMConditionBasePtr> conditions, std::vector<FSMDelegateBasePtr> actions, FSMState* targetState):
		m_TargetState(targetState), m_Conditions(conditions), m_Actions(actions)
	{}
	FSMTransition()
	{
		m_Conditions = {};
		m_Actions = {};
	}

	//Functions
	bool IsTriggered() const //Triggered when one of the given conditions is true
	{
		auto r = false;
		if (m_Conditions.size() > 0)
		{
			for (auto c : m_Conditions)
				r |= c->Invoke();
		}
		else
			r = true;
		return r;
	};
	void RunActions() const { for (auto a : m_Actions) a->Invoke(); };

	//Getters
	FSMState* GetTargetState() const { return m_TargetState; };

	//Setters
	void SetTargetState(FSMState* state)
	{ m_TargetState = state; }
	void SetConditions(std::vector<FSMConditionBasePtr> conditions)
	{ m_Conditions = conditions; }
	void SetActions(std::vector<FSMDelegateBasePtr> actions)
	{ m_Actions = actions; }

private:
	FSMState* m_TargetState = {};
	std::vector<FSMConditionBasePtr> m_Conditions = {};
	std::vector<FSMDelegateBasePtr> m_Actions = {};
};

//-----------------------------------------------------------------
// FINITE STATE MACHINE CLASS
//-----------------------------------------------------------------
class FSM
{
public:
	FSM(std::vector<FSMState*> states, FSMState* initialState):
		m_States(states), m_InitialState(initialState), m_CurrentState(initialState)
	{}
	virtual ~FSM()
	{
		m_States.clear();
	}

	void Start();
	void Update();

private:
	//Datamembers
	std::vector<FSMState*> m_States = {};
	FSMState* m_InitialState = {};
	FSMState* m_CurrentState = {};
};
#endif