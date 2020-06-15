/********************************************************************************
 *
 * Copyright (c) 2018 ROCm Developer Tools
 *
 * MIT LICENSE:
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

extern "C" {
#include <pci/pci.h>
#include <linux/pci.h>
}

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "inc/pci_caps.h"

#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"

#include <chrono>

using std::string;
using std::vector;


struct AgentInformation {
    //! HSA agent handle
    hsa_agent_t                   agent;
    //! agent name
    string                        agent_name;
    //! device type, can be "GPU" or "CPU"
    string                        agent_device_type;
    //! NUMA node this agent belongs to
    uint32_t                      node;
    //! system memory pool
    hsa_amd_memory_pool_t         sys_pool;
    /** vector of memory pool HSA handles as reported during mem pool
    * enumeration
    **/
    vector<hsa_amd_memory_pool_t> mem_pool_list;
    //! vecor of mem pools max sizes (index alligned with mem_pool_list)
    vector<size_t>                max_size_list;
};

//! array of all found HSA agents
vector<AgentInformation> agent_list;

/**
 * @brief computes the difference (in milliseconds) between 2 points in time
 * @param t_end second point in time
 * @param t_start first point in time
 * @return time difference in milliseconds
 */
uint64_t time_diff(
                std::chrono::time_point<std::chrono::system_clock> t_end,
                std::chrono::time_point<std::chrono::system_clock> t_start) {
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            t_end - t_start);
    return milliseconds.count();
}


/**
 * @brief Fetch time needed to copy data between two memory pools
 *
 * Uses time obtained from corresponding hsa_signal objects
 *
 * @param bidirectional 'true' for bidirectional transfer
 * @param signal_fwd signal used for direct transfer
 * @param signal_rev signal used for reverse transfer
 * @return time in seconds
 *
 * */
double GetCopyTime( hsa_signal_t signal_fwd) {
  // Obtain time taken for forward copy
  hsa_amd_profiling_async_copy_time_t async_time_fwd {0};

  if (HSA_STATUS_SUCCESS != hsa_amd_profiling_get_async_copy_time(signal_fwd, &async_time_fwd)){
      std::cout << "\n Copy time failed"; 
      return -1;
  }

  return(async_time_fwd.end - async_time_fwd.start);
}


/**
 * @brief Process individual hsa_agent
 *
 * Functionality:
 *
 * Process a single CPUs or GPUs hsa_agent_t
 *
 * @return hsa_status_t
 *
 * */
hsa_status_t ProcessAgent(hsa_agent_t agent, void* data) {
  string log_msg, log_agent_name;
  hsa_device_type_t device_type;
  AgentInformation agent_info;
  char agent_name[64];
  uint32_t node;

  // get agent list
  vector<AgentInformation>* agent_l =
  reinterpret_cast<vector<AgentInformation>*>(data);

  // Get the name of the agent
  if (HSA_STATUS_SUCCESS != hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, agent_name)) {
	  std::cout << "\n HSA get agent name failed";
    return HSA_STATUS_ERROR; 
  }

  // Get device type
  if (HSA_STATUS_SUCCESS != hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type)) {
	  std::cout <<"\n HSA get device type failed";
    return HSA_STATUS_ERROR; 
  }

  if (HSA_STATUS_SUCCESS != hsa_agent_get_info(agent, HSA_AGENT_INFO_NODE, &node)) {
    std::cout << "\n HSA get node failed";
    return HSA_STATUS_ERROR; 
  }

  agent_info.node = node;

  switch (device_type) {
    case HSA_DEVICE_TYPE_CPU : {
	    std::cout <<"\n HSA device type CPU detected";
      agent_info.agent_device_type = "CPU";
      break;
    };
    case HSA_DEVICE_TYPE_GPU : {
	    std::cout <<"\n HSA device type GPU detected";
      agent_info.agent_device_type = "GPU";
      break;
    };
    case HSA_DEVICE_TYPE_DSP : {
	    std::cout <<"\n HSA device type DSP detected";
      agent_info.agent_device_type = "DSP";
      break;
    };
    default:
	    std::cout <<"\n HSA device unknown device detected" << device_type;
     break;
  }

  // add agent to list
  agent_info.agent = agent;
  agent_info.agent_name = log_agent_name;
  agent_l->push_back(agent_info);

  return HSA_STATUS_SUCCESS;
}

/**
 * @brief Process hsa_agent memory pool
 *
 * Functionality:
 *
 * Process agents memory pools
 *
 * @return hsa_status_t
 *
 * */
hsa_status_t ProcessMemPool(hsa_amd_memory_pool_t pool, void* data) {
  hsa_status_t status;

  // get current agents memory pools
  AgentInformation* agent_info = reinterpret_cast<AgentInformation*>(data);

  // Query pools' segment, report only pools from global segment
  hsa_amd_segment_t segment;
  if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_get_info(pool,
                                        HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                        &segment)))
    std::cout << "\n hsa_amd_memory_pool_get_info()";
  if (HSA_AMD_SEGMENT_GLOBAL != segment) {
    return HSA_STATUS_SUCCESS;
  }

  // Determine if allocation is allowed in this pool
  // Report only pools that allow an alloction by user
  bool alloc = false;
  if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_get_info(pool,
                            HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED,
                            &alloc)))
    std::cout << "\n HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED";
  if (alloc != true) {
    return HSA_STATUS_SUCCESS;
  }

  // Query the max allocatable size
  size_t max_size = 0;
  if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_get_info(pool,
                                        HSA_AMD_MEMORY_POOL_INFO_SIZE,
                                        &max_size)))
    std::cout << "\n HSA_AMD_MEMORY_POOL_INFO_SIZE";

  agent_info->max_size_list.push_back(max_size);

  // Determine if the pools is accessible to all agents
  bool access_to_all = false;
  if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_get_info(pool,
                                HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL,
                                &access_to_all)))
    std::cout << "\n HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL";

  // Determine type of access to owner agent
  hsa_amd_memory_pool_access_t owner_access;
  hsa_agent_t agent = agent_info->agent;
  if (HSA_STATUS_SUCCESS !=
     (status = hsa_amd_agent_memory_pool_get_info(agent, pool,
                                      HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS,
                                      &owner_access)))
    std::cout << "\n Status " << status;

  // Determine if the pool is fine-grained or coarse-grained
  uint32_t flag = 0;
  if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_get_info(pool,
                                        HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                        &flag)))
    std::cout << "\n HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS";

  bool is_kernarg = (HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT & flag);

  if (is_kernarg) {
    agent_info->sys_pool = pool;
    std::cout << " \n [RVSHSA] Found system memory region";

  } else if (owner_access != HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
	  std::cout << " \n [RVSHSA] Found regular memory region";
  }
  agent_info->mem_pool_list.push_back(pool);

  return HSA_STATUS_SUCCESS;
}


int Allocate(int SrcAgent, int DstAgent, size_t Size,
                     hsa_amd_memory_pool_t* pSrcPool, void** SrcBuff,
                     hsa_amd_memory_pool_t* pDstPool, void** DstBuff) {
  hsa_status_t status;
  void* srcbuff = nullptr;
  void* dstbuff = nullptr;

  std::cout << "\n Src agent Index : " << SrcAgent << " Device Type : " << agent_list[SrcAgent].agent_device_type;
  std::cout << "\n Dst agent Index : " << DstAgent << " Device Type : " << agent_list[DstAgent].agent_device_type;

  // iterate over src pools
  for (size_t i = 0; i < agent_list[SrcAgent].mem_pool_list.size(); i++) {
    // size too small, continue
    if (Size > agent_list[SrcAgent].max_size_list[i]) {
      continue;
    }

    // try allocating source buffer
    if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_allocate(
                agent_list[SrcAgent].mem_pool_list[i], Size, 0, &srcbuff))) {
	    std::cout << "\n Allocation failed for source agent " << SrcAgent << " size is " << Size;
	    std::cout << "\n Device Type : " << agent_list[SrcAgent].agent_device_type;
      	    continue;
    }

    // iterate over dst pools
    for (size_t j = 0; j < agent_list[DstAgent].mem_pool_list.size(); j++) {
      // size too small, continue
      if (Size > agent_list[DstAgent].max_size_list[j]) {
        continue;
      }

      // check if src agent has access to this dst agent's pool
      hsa_amd_memory_pool_access_t access = HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED;

      if (agent_list[SrcAgent].agent_device_type == "CPU") {
        if (HSA_STATUS_SUCCESS != hsa_amd_agent_memory_pool_get_info( agent_list[DstAgent].agent,
        							agent_list[SrcAgent].mem_pool_list[j],
       								 HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS,
        							&access)) {
          std:: cout <<"\n HSA pool access failed";
          return -1;
        }
      } else { 
        if (HSA_STATUS_SUCCESS != (status = hsa_amd_agent_memory_pool_get_info( agent_list[SrcAgent].agent,
        									agent_list[DstAgent].mem_pool_list[j],
        									HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS,
        									&access))) {
          std::cout <<"\n HSA pool access failed";
          return -1;
        }
      }

      if (access == HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
        continue;
      }

      // try allocating destination buffer
      if (HSA_STATUS_SUCCESS != (status = hsa_amd_memory_pool_allocate(
        agent_list[DstAgent].mem_pool_list[j], Size, 0, &dstbuff))) {
           std::cout <<"\n HSA pool alloc failed";
        continue;
      }

      // destination buffer allocated,
      // give access to agents

      // determine which one is a cpu and allow access on the other agent
      if (agent_list[SrcAgent].agent_device_type == "CPU") {
        status = hsa_amd_agents_allow_access(1,
                                            &agent_list[DstAgent].agent,
                                            NULL,
                                            srcbuff);
      } else {
        status = hsa_amd_agents_allow_access(1,
                                            &agent_list[SrcAgent].agent,
                                            NULL,
                                            dstbuff);
      }

      if (status != HSA_STATUS_SUCCESS) {
	      std::cout << "\n Access grant issues ";
        // do cleanup
        hsa_amd_memory_pool_free(dstbuff);
        dstbuff = nullptr;
        continue;
      }

      // all OK, set output parameters:
      *pSrcPool = agent_list[SrcAgent].mem_pool_list[i];
      *pDstPool = agent_list[DstAgent].mem_pool_list[j];
      *SrcBuff = srcbuff;
      *DstBuff = dstbuff;

      return 0;
    }  // end of dst agent pool loop

    // suitable destination buffer not foud, deallocate src buff and exit
    hsa_amd_memory_pool_free(srcbuff);
  }  // end of src agent pool loop

  return -1;
}


void InitAgents() {
  hsa_status_t status;

  // Initialize Roc Runtime
  if (HSA_STATUS_SUCCESS != (status = hsa_init())) {
      std::cout << "\n HSA Init failed ";       
      return;
  }

  // Initialize profiling
  if (HSA_STATUS_SUCCESS != hsa_amd_profiling_async_copy_enable(true)) {
     std::cout << "\n HSA asyn copy enable failed";
     return;
  }

  // Populate the lists of agents
  if (HSA_STATUS_SUCCESS != hsa_iterate_agents(ProcessAgent, &agent_list)) {
     std::cout << "\n Populating agents failed";
     return;
  }

  for (uint32_t i = 0; i < agent_list.size(); i++) {
       // Populate the list of memory pools
       if (HSA_STATUS_SUCCESS != hsa_amd_agent_iterate_memory_pools(
                  agent_list[i].agent, ProcessMemPool, &agent_list[i])) {
	       std::cout << "\n Processing memory pool failed";
         return;
       }
       std::cout << " \n Device Type : " << agent_list[i].agent_device_type << "  Index : "<< i;
  }
}


int test(int reps)
{

  hsa_amd_memory_pool_t src_pool_fwd;
  hsa_amd_memory_pool_t dst_pool_fwd;
  hsa_signal_t          signal_fwd;
  double                Duration;

  void* src_ptr_fwd = nullptr;
  void* dst_ptr_fwd = nullptr;
  int32_t src_ix_fwd;
  int32_t dst_ix_fwd;
  int sts;

  src_ix_fwd = 2;
  dst_ix_fwd = 9;
  int Size = 1024;
  
  // allocate buffers and grant permissions for forward transfer
  sts = Allocate(src_ix_fwd, dst_ix_fwd, Size,
           &src_pool_fwd, &src_ptr_fwd,
           &dst_pool_fwd, &dst_ptr_fwd);
  if (sts) {
    return -1;
  }

  // Create a signal to wait on copy operation
  if (HSA_STATUS_SUCCESS != hsa_signal_create(1, 0, NULL, &signal_fwd)) {
      std::cout << "\n Signal creation failed";
      hsa_amd_memory_pool_free(src_ptr_fwd);
      hsa_amd_memory_pool_free(dst_ptr_fwd);
      return -1;
  }

  // initiate forward transfer
  hsa_signal_store_relaxed(signal_fwd, 1);

  if (HSA_STATUS_SUCCESS != hsa_amd_memory_async_copy(
                dst_ptr_fwd, agent_list[dst_ix_fwd].agent,
                src_ptr_fwd, agent_list[src_ix_fwd].agent,
                Size,
                0, NULL, signal_fwd)) {
      std::cout << "\n Async copy failed";
      return -1;
  }

  // wait for transfer to complete
  hsa_signal_wait_acquire(signal_fwd, HSA_SIGNAL_CONDITION_LT, 1, uint64_t(-1), HSA_WAIT_STATE_ACTIVE);

  // wait for transfer to complete
  while (hsa_signal_wait_acquire(signal_fwd, HSA_SIGNAL_CONDITION_LT,
    1, uint64_t(-1), HSA_WAIT_STATE_ACTIVE)) {}

  // get transfer duration
  Duration = GetCopyTime(signal_fwd)/1000000000;

  std::cout << " \n Time taken to transfer : " << Duration;

  hsa_amd_memory_pool_free(src_ptr_fwd);
  hsa_amd_memory_pool_free(dst_ptr_fwd);
  hsa_signal_destroy(signal_fwd);

  return 0;
}


int main(int argc, char *argv[])
{
    int reps = 10;

    InitAgents();

    test(reps);
}




