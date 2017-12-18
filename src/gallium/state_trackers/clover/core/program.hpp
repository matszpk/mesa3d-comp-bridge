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

#ifndef CLOVER_CORE_PROGRAM_HPP
#define CLOVER_CORE_PROGRAM_HPP

#include <map>
#ifdef ENABLE_COMP_BRIDGE
#include <memory>
#include <CLRX/utils/GPUId.h>
#include <CLRX/amdbin/AmdCL2Binaries.h>
#endif

#include "core/object.hpp"
#include "core/context.hpp"
#include "core/module.hpp"

namespace clover {
   typedef std::vector<std::pair<std::string, std::string>> header_map;

   class program : public ref_counter, public _cl_program {
   private:
      typedef adaptor_range<
         evals, const std::vector<intrusive_ref<device>> &> device_range;

   public:
      program(clover::context &ctx,
              const std::string &source);
      program(clover::context &ctx,
              const ref_vector<device> &devs = {},
              const std::vector<module> &binaries = {});

      program(const program &prog) = delete;
      program &
      operator=(const program &prog) = delete;


#ifdef ENABLE_COMP_BRIDGE
      void compile(const ref_vector<device> &devs, const std::string &opts,
                   const header_map &headers = {}, bool at_build = false);
      void link(const ref_vector<device> &devs, const std::string &opts,
                const ref_vector<program> &progs, bool at_build = false);
      void build_amdocl2(const ref_vector<device> &devs, const std::string &opts);
#else
      void compile(const ref_vector<device> &devs, const std::string &opts,
                   const header_map &headers = {});
      void link(const ref_vector<device> &devs, const std::string &opts,
                const ref_vector<program> &progs);
#endif

      const bool has_source;
      const std::string &source() const;

      device_range devices() const;

      struct build {
         build(const module &m = {}, const std::string &opts = {},
               const std::string &log = {}) : binary(m), opts(opts), log(log) {}
#ifdef ENABLE_COMP_BRIDGE
         build(std::unique_ptr<CLRX::AmdCL2MainGPUBinary64>& m, CLRX::GPUDeviceType devtype,
               const std::string &opts = {}, const std::string &log = {}) :
                  opts(opts), log(log), amdocl2_binary(nullptr) {
            binary = module::create_from_amdocl2_binary(m.get(), devtype);
            amdocl2_code.reset(m->getBinaryCode());
            amdocl2_binary.reset(m.release());
         }
#endif

         cl_build_status status() const;
         cl_program_binary_type binary_type() const;

         module binary;
         std::string opts;
         std::string log;
#ifdef ENABLE_COMP_BRIDGE
         std::unique_ptr<cxbyte[]> amdocl2_code;
         std::unique_ptr<CLRX::AmdCL2MainGPUBinary64> amdocl2_binary;
#endif
      };
      
#ifdef ENABLE_COMP_BRIDGE
      bool is_amdocl2_binary(const device &dev);
#endif

      const build &build(const device &dev) const;

      const std::vector<module::symbol> &symbols() const;

      unsigned kernel_ref_count() const;

      const intrusive_ref<clover::context> context;

      friend class kernel;

   private:
      std::vector<intrusive_ref<device>> _devices;
      std::map<const device *, struct build> _builds;
      std::string _source;
      ref_counter _kernel_ref_counter;
   };
}

#endif
