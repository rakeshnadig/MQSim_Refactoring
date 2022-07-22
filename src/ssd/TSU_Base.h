#ifndef TSU_H
#define TSU_H

#include <list>

#include "../sim/Sim_Defs.h"
#include "../sim/Sim_Object.h"
#include "../nvm_chip/flash_memory/Flash_Chip.h"
#include "../sim/Sim_Reporter.h"
#include "FTL.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include "Flash_Transaction_Queue.h"

namespace SSD_Components
{
enum class Flash_Scheduling_Type
{
	OUT_OF_ORDER,
	PRIORITY_OUT_OF_ORDER,
	FLIN
};
class FTL;
class TSU_Base : public MQSimEngine::Sim_Object
{
public:
	TSU_Base(const sim_object_id_type &id,
			FTL *ftl,
			NVM_PHY_ONFI_NVDDR2 *NVMController,
			Flash_Scheduling_Type Type,
			unsigned int Channel_no,
			unsigned int chip_no_per_channel,
			unsigned int DieNoPerChip,
			unsigned int PlaneNoPerDie,
			bool EraseSuspensionEnabled,
			bool ProgramSuspensionEnabled,
			sim_time_type WriteReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForRead,
			sim_time_type EraseReasonableSuspensionTimeForWrite);
	virtual ~TSU_Base();
	void SetupTriggers();

	/*When an MQSim needs to send a set of transactions for execution, the following 
		* three funcitons should be invoked in this order:
		* PrepareForTransactionSubmit()
		* SubmitTransaction(transaction)
		* .....
		* SubmitTransaction(transaction)
		* Schedule()
		*
		* The above mentioned mechanism helps to exploit die-level and plane-level parallelism.
		* More precisely, first the transactions are queued and then, when the Schedule function
		* is invoked, the TSU has that opportunity to schedule them together to exploit multiplane
		* and die-interleaved execution.
		*/
	void PrepareForTransactionSubmit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
		{
			return;
		}
		transaction_receive_slots.clear();
	}

	void SubmitTransaction(NVM_Transaction_Flash *transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	/* Shedules the transactions currently stored in inputTransactionSlots. The transactions could
		* be mixes of reads, writes, and erases.
		*/
	virtual void Schedule() = 0;
	virtual void ReportResultsInXML(std::string name_prefix, Utils::XmlWriter &xmlwriter);

protected:
	FTL *ftl;
	NVM_PHY_ONFI_NVDDR2 *_NVMController;
	Flash_Scheduling_Type type;
	unsigned int channel_count;
	unsigned int chip_no_per_channel;
	unsigned int die_no_per_chip;
	unsigned int plane_no_per_die;
	bool eraseSuspensionEnabled, programSuspensionEnabled;
	sim_time_type writeReasonableSuspensionTimeForRead;
	sim_time_type eraseReasonableSuspensionTimeForRead; // the time period
	sim_time_type eraseReasonableSuspensionTimeForWrite;
	flash_chip_ID_type *Round_robin_turn_of_channel; // Used for round-robin service of the chips in channels

	static TSU_Base *_my_instance;
	std::list<NVM_Transaction_Flash *> transaction_receive_slots; // Stores the transactions that are received for scheduling
	std::list<NVM_Transaction_Flash *> transaction_dispatch_slots; // Used to submit transactions to the channel controller
	virtual bool ServiceReadTransaction(NVM::FlashMemory::Flash_Chip *chip) = 0;
	virtual bool ServiceWriteTransaction(NVM::FlashMemory::Flash_Chip *chip) = 0;
	virtual bool ServiceEraseTransaction(NVM::FlashMemory::Flash_Chip *chip) = 0;
	bool IssueCommandToChip(Flash_Transaction_Queue *sourceQueue1,
			Flash_Transaction_Queue *sourceQueue2,
			Transaction_Type transactionType,
			bool suspensionRequired);
	static void HandleTransactionServicedSignalFromPHY(NVM_Transaction_Flash *transaction);
	static void HandleChannelIdleSignal(flash_channel_ID_type);
	static void HandleChipIdleSignal(NVM::FlashMemory::Flash_Chip *chip);
	int opened_scheduling_reqs;
	void ProcessChipRequests(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (!_my_instance->ServiceReadTransaction(chip)) {
			if (!_my_instance->ServiceWriteTransaction(chip)) {
				_my_instance->ServiceEraseTransaction(chip);
			}
		}
	}

private:
	bool TransactionIsReady(NVM_Transaction_Flash* transaction)
	{
		switch (transaction->Type)
		{
		case Transaction_Type::READ:
			return true;
		case Transaction_Type::WRITE:
			return static_cast<NVM_Transaction_Flash_WR*>(transaction)->RelatedRead == NULL;
		case Transaction_Type::ERASE:
			return static_cast<NVM_Transaction_Flash_ER*>(transaction)->Page_movement_activities.size() == 0;
		default:
			return true;
		}
	}
};
} // namespace SSD_Components

#endif //TSU_H
