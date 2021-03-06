
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         10

#define RESERV_INT_SIZE    4
#define RESERV_FP_SIZE     2
#define FU_INT_SIZE        2
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     4
#define FU_FP_LATENCY      9

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */

/* ECE552 Assignment 3 - BEGIN CODE */

bool pushToIFQ(instruction_t* instr)
{
    if (instr_queue_size < INSTR_QUEUE_SIZE)
    {
        instr_queue[instr_queue_size] = instr;
        instr_queue_size++;
        return true;
    }
    else
        return false;
}

instruction_t* popFromIFQ()
{
    if (instr_queue_size > 0)
    {
        instruction_t* first = instr_queue[0];
        int i;
        // move all the element to left
        for (i = 0; i < INSTR_QUEUE_SIZE-1; i++)
        {
            instr_queue[i] = instr_queue[i+1];
        }
        // set the last element to NULL
        instr_queue[INSTR_QUEUE_SIZE-1] = NULL;
	    instr_queue_size--;
        return first;
    }
    else
        return NULL;
}

instruction_t* getHeadInstrFromIFQ()
{
    return instr_queue[0];
}

void markRAWDependences(instruction_t* instr)
{
	int i;
	for (i = 0; i < 3; i++)
	{
        // check if input registers are not set or equal to r0 (r0 is constant value 0)
		if (instr->r_in[i] != DNA && instr->r_in[i] != 0)
        {
            // set the dependences based on the map table
            instr->Q[i] = map_table[instr->r_in[i]];
        }
	}
}
void setMapTable(instruction_t* instr)
{
    int i;
    for (i = 0; i < 2; i++)
    {
        // check if input registers are not set or equal to r0 (r0 is constant value 0)
		if (instr->r_out[i] != DNA && instr->r_out[i] != 0)
        {
            // set the map table entries based on the instruction output registers number
            map_table[instr->r_out[i]]= instr;
        }
    }
}

void clearMapTable(instruction_t* instr)
{
    int i;
    for (i = 0; i < 2; i++)
    {
        // check if input registers are not set or equal to r0 (r0 is constant value 0)
		if (instr->r_out[i] != DNA && instr->r_out[i] != 0)
        {
            // clear the map table entries based on the instruction output registers number
            // this function will be called when CDB is retired
            map_table[instr->r_out[i]]= NULL;
        }
    }
}

bool RAWResolved(instruction_t* instr, int current_cycle)
{
    int i;
    for (i = 0; i < 3; i++)
    {
        // check if the dependent register has broadcasted by CDB, 0 menas not in CDB yet, bigger or equal means that haven't reach the CDB broadcast yet
        if (instr->Q[i] != NULL && (instr->Q[i]->tom_cdb_cycle == 0 || instr->Q[i]->tom_cdb_cycle >= current_cycle))
            return false;
    }
    return true;
}

void removeFromRS(instruction_t* instr)
{
    int i;

    // compare the instruction with the ones stored inside RS
    if (USES_INT_FU(instr->op))
    {
        for (i = 0; i < RESERV_INT_SIZE; i++)
        {
            if(instr == reservINT[i])
                reservINT[i] = NULL;
        }
    }
    else if (USES_FP_FU(instr->op))
    {
        for (i = 0; i < RESERV_FP_SIZE; i++)
        {
            if(instr == reservFP[i])
                reservFP[i] = NULL;
        }
    }
}

void removeFromFU(instruction_t* instr)
{
    int i;

    // compare the instruction to the ones stored in FU
    if (USES_INT_FU(instr->op))
    {
        for (i = 0; i < FU_INT_SIZE; i++)
        {
            if(instr == fuINT[i])
                fuINT[i] = NULL;
        }
    }
    // since we only have one FP FU, no need a for loop
    else if (USES_FP_FU(instr->op))
    {
        if(instr == fuFP[0])
           fuFP[0] = NULL;

    }
}

/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    if (instr_queue_size > 0)
	    return false;    

    if (fetch_index <= sim_insn)
        return false;

    int i;

    for (i = 0; i < RESERV_INT_SIZE; i++)
    {
    	if (reservINT[i] != NULL)
    		return false;
    }

    for (i = 0; i < RESERV_FP_SIZE; i++)
    {    
        if (reservFP[i] != NULL)
    	    return false;
    }

    for (i = 0; i < FU_INT_SIZE; i++)
    {
        if (fuINT[i] != NULL)
            return false;
    }

    if (fuFP[0] != NULL)
        return false;

    for (i = 0; i < MD_TOTAL_REGS; i++) {
        if (map_table[i] != NULL)
            return false;
    }
    
    if (commonDataBus != NULL)
        return false;

    return true;
    /* ECE552 Assignment 3 - END CODE */
    //ECE552: you can change this as needed; we've added this so the code provided to you compiles
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    // check if we need to broadcast the CDB values, and make sure the cycle is ok
    if (commonDataBus && current_cycle - 1 >= commonDataBus->tom_cdb_cycle)
    {
        clearMapTable(commonDataBus);
        commonDataBus = NULL;
    }
    /* ECE552 Assignment 3 - END CODE */
}


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    instruction_t* old = NULL;

    int i;
    for (i = 0; i < FU_INT_SIZE; i++)
    {
        // unlike issue to execute, we don't need to check execute cycle and cdb cycle
        // because instructions in FU are definitely assigend the execute cycle, there's no need to check the execute cycle
        // for cdb cycle, it is only assigned when CDB is empty and instruction is moved to that stage
        if (fuINT[i] != NULL && current_cycle - FU_INT_LATENCY >= fuINT[i]->tom_execute_cycle)
        {
            // if no need to write to CDB, just remove from reservation station and INT FU
            if (!WRITES_CDB(fuINT[i]->op))
            {
                removeFromRS(fuINT[i]);
                fuINT[i] = NULL;
            }
            else if (old == NULL || old->index > fuINT[i]->index)
                old = fuINT[i];
        }
    }

    if (fuFP[0] != NULL && current_cycle - FU_FP_LATENCY >= fuFP[0]->tom_execute_cycle)
    {
        if (!WRITES_CDB(fuFP[0]->op))
        {
            removeFromRS(fuFP[0]);
            fuFP[0] = NULL;
        }
        else if (old == NULL || old->index > fuFP[0]->index)
            old = fuFP[0];
    }

    if (commonDataBus == NULL && old != NULL)
    {
        old->tom_cdb_cycle = current_cycle;
        commonDataBus = old;
        removeFromRS(old);
        removeFromFU(old);
    }
    /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    instruction_t* old_one = NULL;
    instruction_t* old_two = NULL;

    int i;
    for (i = 0; i < RESERV_INT_SIZE; i++)
    {
        // has to verify cycle number because we use reverse order call in run_tomasulo.
        // without checking execute cycle number, it may add instructions already in execut stage to execute stage again
        // same reason, we need to check issue cycle as well to make sure the instruction is already in issue stage
        if (reservINT[i] != NULL && reservINT[i]->tom_issue_cycle != 0 && reservINT[i]->tom_execute_cycle == 0 && RAWResolved(reservINT[i], current_cycle))
        {
            // get the oldest two instructions based on the index
            if (old_one == NULL)
                old_one = reservINT[i];
            else if (reservINT[i]->index < old_one->index)
            {
                old_two = old_one;
                old_one = reservINT[i];
            }
            else if (old_two == NULL || reservINT[i]->index < old_two->index)
                old_two = reservINT[i];
        }
    }

    for (i = 0; i < FU_INT_SIZE; i++)
    {
        // find empty FU slots
        if (fuINT[i] == NULL)
        {
            if (old_one != NULL)
            {
                old_one->tom_execute_cycle = current_cycle;
                fuINT[i] = old_one;
                old_one = NULL;
            }
            else if (old_two != NULL)
            {
                old_two->tom_execute_cycle = current_cycle;
                fuINT[i] = old_two;
            }
        }
    }

    // clear previous INT history in old_one, since we only have one FP FU
    old_one = NULL;

    for (i = 0; i < RESERV_FP_SIZE; i++)
    {
        if (reservFP[i] != NULL && reservFP[i]->tom_issue_cycle != 0 && reservFP[i]->tom_execute_cycle == 0 && RAWResolved(reservFP[i], current_cycle))
        {
            // get the oldest instruction based on the index
            if (old_one == NULL || reservFP[i]->index < old_one->index)
                old_one = reservFP[i];
        }
    }

    // we only have one FP fu, no need for a for loop
    if (fuFP[0] == NULL)
    {
        if (old_one != NULL)
        {
            old_one->tom_execute_cycle = current_cycle;
            fuFP[0] = old_one;
        }
    }
    /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    int i;
    for (i = 0; i < RESERV_INT_SIZE; i++)
    {
        // only set the issue cycle for the first time
        // no need to check dispatch cycle, since it will be assigned when added to RS
        if (reservINT[i] != NULL && reservINT[i]->tom_issue_cycle == 0)
            reservINT[i]->tom_issue_cycle = current_cycle;
    }

    for (i = 0; i < RESERV_FP_SIZE; i++)
    {
        // only set the issue cycle for the first time
        if (reservFP[i] != NULL && reservFP[i]->tom_issue_cycle == 0)
            reservFP[i]->tom_issue_cycle = current_cycle;
    }
    /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {

  /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    // keep adding instructions to queue until it is full
    if (instr_queue_size < INSTR_QUEUE_SIZE && fetch_index <= sim_num_insn)
    {
        instruction_t* current_instr = get_instr(trace, fetch_index);
		while (IS_TRAP(current_instr->op))
		{
			fetch_index++;
           current_instr = get_instr(trace, fetch_index);
	    }
        if (current_instr != NULL)
        {
            fetch_index++;
            pushToIFQ(current_instr);	
       }
    }
    /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    // fetch instruction and dispatch
    fetch(trace);
    instruction_t* instr = getHeadInstrFromIFQ();
    
    if (instr != NULL)
    {
        if (IS_UNCOND_CTRL(instr->op) || IS_COND_CTRL(instr->op))
        {
            instr->tom_dispatch_cycle = current_cycle;
            popFromIFQ();
        }
        // we found an op code 0 that is none of the operation category, so we treat it as INT operation
        else if (USES_INT_FU(instr->op))
        {
            int i;
            for (i = 0; i < RESERV_INT_SIZE; i++)
            {
                // empty slot for reservation station
                if(reservINT[i] == NULL)
                {
                    markRAWDependences(instr);
                    setMapTable(instr);
                    instr->tom_dispatch_cycle = current_cycle;
                    reservINT[i] = instr;
                    popFromIFQ();
					break;
                }
            }
        }
        else if (USES_FP_FU(instr->op))
        {
            int i;
            for (i = 0; i < RESERV_FP_SIZE; i++)
            {
                // empty slot for reservation station
                if(reservFP[i] == NULL)
                {
                    markRAWDependences(instr);
                    setMapTable(instr);
                    instr->tom_dispatch_cycle = current_cycle;
                    reservFP[i] = instr;
                    popFromIFQ();
					break;
                }
            }
        }
        else 
            popFromIFQ();
    }
    /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }
  
  int cycle = 1;
  while (true) {

     /* ECE552: YOUR CODE GOES HERE */
     /* ECE552 Assignment 3 - BEGIN CODE */
     // in reverse order in order to prevent multiple stage in one cycle
     CDB_To_retire(cycle);  
     execute_To_CDB(cycle);
     issue_To_execute(cycle);
     dispatch_To_issue(cycle);
     fetch_To_dispatch(trace, cycle);   
     

     cycle++;
    
     if (is_simulation_done(sim_num_insn))
        break;
    /* ECE552 Assignment 3 - END CODE */
  }
  
  return cycle;
}
