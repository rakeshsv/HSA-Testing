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
#ifndef INCLUDE_AMAT_INC_H_
#define INCLUDE_AMAT_INC_H_

#include <string>

struct AgentInformation {
    //! HSA agent handle
    hsa_agent_t                   agent;
    //! agent name
    std::string                   agent_name;
    //! device type, can be "GPU" or "CPU"
    std::string                        agent_device_type;
    //! NUMA node this agent belongs to
    uint32_t                      node;
    //! system memory pool
    hsa_amd_memory_pool_t         sys_pool;
    /** vector of memory pool HSA handles as reported during mem pool
    * enumeration
    **/
    std::vector<hsa_amd_memory_pool_t> mem_pool_list;
    //! vecor of mem pools max sizes (index alligned with mem_pool_list)
    std::vector<size_t>                max_size_list;
};


#define FPGA_MEM_SIZE 2560 * 1620/8


#endif  // INCLUDE_PCI_CAPS_H_
