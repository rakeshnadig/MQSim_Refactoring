#ifndef EVENT_TREE_H
#define EVENT_TREE_H

#include "Sim_Defs.h"
#include "Sim_Event.h"

namespace MQSimEngine
{

	class EventTree
	{
	public:
		EventTree();
		~EventTree();

		void Add(sim_time_type key, Sim_Event* data);
		Sim_Event* GetData(sim_time_type key);
		void Insert_sim_event(Sim_Event* data);
		int Size();
		sim_time_type Get_min_key();
		void Remove(sim_time_type key);
		void Clear();
	};
}

#endif // !EVENT_TREE_H
