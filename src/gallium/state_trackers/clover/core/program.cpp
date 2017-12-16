//
// Copyright 2012 Francisco Jerez
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

#ifdef ENABLE_COMP_BRIDGE
#include <vector>
#include "core/platform.hpp"
#endif
#include "core/program.hpp"
#include "llvm/invocation.hpp"
#include "tgsi/invocation.hpp"

using namespace clover;
#ifdef ENABLE_COMP_BRIDGE
using namespace CLRX;
#endif

program::program(clover::context &ctx, const std::string &source) :
   has_source(true), context(ctx), _source(source), _kernel_ref_counter(0) {
}

program::program(clover::context &ctx,
                 const ref_vector<device> &devs,
                 const std::vector<module> &binaries) :
   has_source(false), context(ctx),
   _devices(devs), _kernel_ref_counter(0) {
   for_each([&](device &dev, const module &bin) {
         _builds[&dev] = { bin };
      },
      devs, binaries);
}

void
program::compile(const ref_vector<device> &devs, const std::string &opts,
                 const header_map &headers
#ifdef ENABLE_COMP_BRIDGE
                 , bool at_build
#endif
                ) {
   if (has_source) {
      _devices = devs;

      for (auto &dev : devs) {
#ifdef ENABLE_COMP_BRIDGE
         if (dev.get_comp_bridge() != comp_bridge::none) {
            if (at_build)
               continue;
            continue;
         }
#endif
         
         std::string log;

         try {
            const module m = (dev.ir_format() == PIPE_SHADER_IR_TGSI ?
                              tgsi::compile_program(_source, log) :
                              llvm::compile_program(_source, headers,
                                                    dev.ir_target(), opts, log));
            _builds[&dev] = { m, opts, log };
         } catch (...) {
            _builds[&dev] = { module(), opts, log };
            throw;
         }
      }
   }
}

void
program::link(const ref_vector<device> &devs, const std::string &opts,
              const ref_vector<program> &progs
#ifdef ENABLE_COMP_BRIDGE
                 , bool at_build
#endif
             ) {
   _devices = devs;

   for (auto &dev : devs) {
#ifdef ENABLE_COMP_BRIDGE
      if (dev.get_comp_bridge() != comp_bridge::none) {
         if (at_build)
            continue;
         continue;
      }
#endif
      
      const std::vector<module> ms = map([&](const program &prog) {
         return prog.build(dev).binary;
         }, progs);
      std::string log = _builds[&dev].log;

      try {
         const module m = (dev.ir_format() == PIPE_SHADER_IR_TGSI ?
                           tgsi::link_program(ms) :
                           llvm::link_program(ms, dev.ir_format(),
                                              dev.ir_target(), opts, log));
         _builds[&dev] = { m, opts, log };
      } catch (...) {
         _builds[&dev] = { module(), opts, log };
         throw;
      }
   }
}

#ifdef ENABLE_COMP_BRIDGE
void program::build_amdocl2(const ref_vector<device> &devs, const std::string &opts) {
   if (has_source) {
      _devices = devs;
      for (auto &dev : devs) {
         if (dev.get_comp_bridge() == comp_bridge::none)
            continue;
         const platform& platform = dev.platform;
         const auto amdocl2_funcs = platform.get_amdocl2_handlers();
         cl_device_id amdocl2_device = dev.get_amdocl2_device();
         cl_context amdocl2_context = platform.get_amdocl2_context();
         // build program
         cl_int errcode = CL_SUCCESS;
         const char* prog_source = _source.c_str();
         cl_program amdocl2_prog = amdocl2_funcs->fn_clCreateProgramWithSource(
                  amdocl2_context, 1, &prog_source, nullptr, &errcode);
         if (amdocl2_prog == nullptr) {
            _builds[&dev] = { module(), opts, "Can't create AMDOCL2 program" };
            throw build_error("Can't create AMDOCL2 program");
         }
         try {
            cl_int build_errcode = amdocl2_funcs->fn_clBuildProgram(amdocl2_prog,
                           1, &amdocl2_device, opts.c_str(), nullptr, nullptr);
            size_t size = 0;
            errcode = amdocl2_funcs->fn_clGetProgramBuildInfo(amdocl2_prog, amdocl2_device,
                        CL_PROGRAM_BUILD_LOG, 0, nullptr, &size);
            if (errcode != CL_SUCCESS) {
               _builds[&dev] = { module(), opts, "Can't get AMDOCL2 Build Log" };
               throw build_error("Can't get AMDOCL2 Build Log");
            }
            std::vector<char> logvec(size);
            errcode = amdocl2_funcs->fn_clGetProgramBuildInfo(amdocl2_prog, amdocl2_device,
                        CL_PROGRAM_BUILD_LOG, size, logvec.data(), nullptr);
            if (errcode != CL_SUCCESS) {
               _builds[&dev] = { module(), opts, "Can't get AMDOCL2 Build Log" };
               throw build_error("Can't get AMDOCL2 Build Log");
            }
            
            if (build_errcode != CL_SUCCESS) {
               _builds[&dev] = { module(), opts, logvec.data() };
               throw build_error("Build failed");
            }
            // get build program binaries
            errcode = amdocl2_funcs->fn_clGetProgramInfo(amdocl2_prog, 
                       CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, nullptr);
            if (errcode != CL_SUCCESS) {
               _builds[&dev] = { module(), opts, "Can't get AMDOCL2 Binary" };
               throw build_error("Can't get AMDOCL2 Binary");
            }
            std::unique_ptr<cxbyte[]> binary(new cxbyte[size]);
            unsigned char* binary_ptr = binary.get();
            errcode = amdocl2_funcs->fn_clGetProgramInfo(amdocl2_prog, 
                       CL_PROGRAM_BINARIES, sizeof(unsigned char*), &binary_ptr, nullptr);
            if (errcode != CL_SUCCESS) {
               _builds[&dev] = { module(), opts, "Can't get AMDOCL2 Binary" };
               throw build_error("Can't get AMDOCL2 Binary");
            }
            
            std::unique_ptr<AmdCL2MainGPUBinary64> amdocl2_binary(
                        new AmdCL2MainGPUBinary64(size, binary_ptr));
            _builds[&dev] = { amdocl2_binary, opts, logvec.data() };
            binary.release();
            
         } catch(const std::exception& ex) {
            _builds[&dev] = { module(), opts, ex.what() };
            amdocl2_funcs->fn_clReleaseProgram(amdocl2_prog);
            throw build_error(ex.what());
         } catch(...) {
            amdocl2_funcs->fn_clReleaseProgram(amdocl2_prog);
            throw;
         }
         amdocl2_funcs->fn_clReleaseProgram(amdocl2_prog);
      }
   }
}
#endif

const std::string &
program::source() const {
   return _source;
}

program::device_range
program::devices() const {
   return map(evals(), _devices);
}

cl_build_status
program::build::status() const {
   if (!binary.secs.empty())
      return CL_BUILD_SUCCESS;
   else if (log.size())
      return CL_BUILD_ERROR;
   else
      return CL_BUILD_NONE;
}

cl_program_binary_type
program::build::binary_type() const {
   if (any_of(type_equals(module::section::text_intermediate), binary.secs))
      return CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
   else if (any_of(type_equals(module::section::text_library), binary.secs))
      return CL_PROGRAM_BINARY_TYPE_LIBRARY;
   else if (any_of(type_equals(module::section::text_executable), binary.secs))
      return CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
   else
      return CL_PROGRAM_BINARY_TYPE_NONE;
}

const struct program::build &
program::build(const device &dev) const {
   static const struct build null;
   return _builds.count(&dev) ? _builds.find(&dev)->second : null;
}

const std::vector<module::symbol> &
program::symbols() const {
   if (_builds.empty())
      throw error(CL_INVALID_PROGRAM_EXECUTABLE);

   return _builds.begin()->second.binary.syms;
}

unsigned
program::kernel_ref_count() const {
   return _kernel_ref_counter.ref_count();
}
