//
// Copyright 2013 Francisco Jerez
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef CLOVER_CORE_PLATFORM_HPP
#define CLOVER_CORE_PLATFORM_HPP

#ifdef ENABLE_COMP_BRIDGE
#include <map>
#include <string>
#include <utility>
#include <memory>
#endif
#include <vector>
#ifdef ENABLE_COMP_BRIDGE
#include <CLRX/utils/Utilities.h>
#include <CLRX/utils/GPUId.h>
#include "CL/cl.h"
#endif

#include "core/object.hpp"
#include "core/device.hpp"
#include "util/range.hpp"

namespace clover {
#ifdef ENABLE_COMP_BRIDGE
   namespace cl_funcs {
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clGetPlatformIDs)(cl_uint          /* num_entries */,
                     cl_platform_id * /* platforms */,
                     cl_uint *        /* num_platforms */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clGetDeviceInfo)(cl_device_id    /* device */,
                     cl_device_info  /* param_name */, 
                     size_t          /* param_value_size */, 
                     void *          /* param_value */,
                     size_t *        /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_context CL_API_CALL
      (*tp_clCreateContextFromType)(const cl_context_properties * /* properties */,
                        cl_device_type          /* device_type */,
                        void (CL_CALLBACK *     /* pfn_notify*/ )(const char *, const void *, size_t, void *),
                        void *                  /* user_data */,
                        cl_int *                /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clGetContextInfo)(cl_context         /* context */, 
                 cl_context_info    /* param_name */, 
                 size_t             /* param_value_size */, 
                 void *             /* param_value */, 
                 size_t *           /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clReleaseContext)(cl_context /* context */) CL_API_SUFFIX__VERSION_1_0;
      
      typedef CL_API_ENTRY cl_program CL_API_CALL
      (*tp_clCreateProgramWithSource)(cl_context        /* context */,
                          cl_uint           /* count */,
                          const char **     /* strings */,
                          const size_t *    /* lengths */,
                          cl_int *          /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clReleaseProgram)(cl_program /* program */) CL_API_SUFFIX__VERSION_1_0;
      
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clBuildProgram)(cl_program           /* program */,
               cl_uint              /* num_devices */,
               const cl_device_id * /* device_list */,
               const char *         /* options */, 
               void (CL_CALLBACK *  /* pfn_notify */)(cl_program /* program */, void * /* user_data */),
               void *               /* user_data */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clGetProgramBuildInfo)(cl_program            /* program */,
                      cl_device_id          /* device */,
                      cl_program_build_info /* param_name */,
                      size_t                /* param_value_size */,
                      void *                /* param_value */,
                      size_t *              /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;
      typedef CL_API_ENTRY cl_int CL_API_CALL
      (*tp_clGetProgramInfo)(cl_program         /* program */,
                 cl_program_info    /* param_name */,
                 size_t             /* param_value_size */,
                 void *             /* param_value */,
                 size_t *           /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;
   };
#endif
   
   class platform : public _cl_platform_id,
                    public adaptor_range<
      evals, std::vector<intrusive_ref<device>> &> {
   public:
      platform();

      platform(const platform &platform) = delete;
      platform &
      operator=(const platform &platform) = delete;

   protected:
      std::vector<intrusive_ref<device>> devs;
#ifdef ENABLE_COMP_BRIDGE
   public:
      union amdocl2_funcs_struct {
         struct {
            cl_funcs::tp_clGetPlatformIDs fn_clGetPlatformIDs;
            cl_funcs::tp_clGetDeviceInfo fn_clGetDeviceInfo;
            cl_funcs::tp_clCreateContextFromType fn_clCreateContextFromType;
            cl_funcs::tp_clGetContextInfo fn_clGetContextInfo;
            cl_funcs::tp_clReleaseContext fn_clReleaseContext;
            cl_funcs::tp_clCreateProgramWithSource fn_clCreateProgramWithSource;
            cl_funcs::tp_clGetProgramInfo fn_clGetProgramInfo;
            cl_funcs::tp_clGetProgramBuildInfo fn_clGetProgramBuildInfo;
            cl_funcs::tp_clBuildProgram fn_clBuildProgram;
            cl_funcs::tp_clReleaseProgram fn_clReleaseProgram;
         };
         void* funcs[10];
      };
      
      cl_context get_amdocl2_context() const
      { return amdocl2_context; }
      const amdocl2_funcs_struct* get_amdocl2_handlers() const
      { return amdocl2_funcs.get(); }
   private:
      void load_config_from_file(const char* filename);
      void load_config();
      
      void load_amdocl2();
      
      std::string amdocl2_path;
      int amdocl2_version;
      std::map<CLRX::GPUArchitecture, comp_bridge> arch_bridge_map;
      std::map<CLRX::GPUDeviceType, comp_bridge> dev_bridge_map;
      
      bool amdocl2_dynlib_load_tried;
      CLRX::DynLibrary amdocl2_dynlib;
      std::unique_ptr<amdocl2_funcs_struct> amdocl2_funcs;
      cl_context amdocl2_context;
      std::map<CLRX::GPUDeviceType, cl_device_id> amdocl2_device_map;
#endif
   };
}

#endif
