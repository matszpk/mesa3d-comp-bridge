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
#include <cstdio>
#include <iostream>
#include <fstream>
#include <exception>
#include <CLRX/utils/Utilities.h>
#include <CLRX/utils/GPUId.h>
#endif
#include "core/platform.hpp"

using namespace clover;
#ifdef ENABLE_COMP_BRIDGE
using namespace CLRX;
#endif

platform::platform() : adaptor_range(evals(), devs) {
#ifdef ENABLE_COMP_BRIDGE
   allow_amdocl2_for_gcn14 = false;
   amdocl2_context = nullptr;
   amdocl2_dynlib_load_tried = false;
   load_config();
#endif
   int n = pipe_loader_probe(NULL, 0);
   std::vector<pipe_loader_device *> ldevs(n);

   pipe_loader_probe(&ldevs.front(), n);

   for (pipe_loader_device *ldev : ldevs) {
      try {
#ifdef ENABLE_COMP_BRIDGE
         if (ldev) {
            devs.push_back(create<device>(*this, ldev));
            auto& dev = devs.back();
            auto dname = dev().device_name();
            GPUDeviceType devtype = dev().get_device_type();
            GPUDeviceType real_devtype = dev().get_real_device_type();
            // check whether to load amdocl2 library
            GPUArchitecture arch = getGPUArchitectureFromDeviceType(real_devtype);
            auto ait = arch_bridge_map.find(arch);
            auto dit = dev_bridge_map.find(real_devtype);
            if ((ait != arch_bridge_map.end() && ait->second==comp_bridge::amdocl2) ||
                (dit != dev_bridge_map.end() && dit->second==comp_bridge::amdocl2)) {
               if (!amdocl2_dynlib_load_tried) {
                  // try load amdocl2
                  try {
                     load_amdocl2();
                  } catch(const std::exception& ex) {
                     std::cerr << "Can't load AMDOCL2 library: " << ex.what() << std::endl;
                  } catch(...) {
                     std::cerr << "Can't load AMDOCL2 library" << std::endl;
                  }
                  amdocl2_dynlib_load_tried = true;
               }
               auto devit = amdocl2_device_map.find(devtype);
               if (devit != amdocl2_device_map.end())
                  dev().set_comp_bridge(comp_bridge::amdocl2, devit->second);
            }
         }
#else
         if (ldev)
            devs.push_back(create<device>(*this, ldev));
#endif
      } catch (error &) {
         pipe_loader_release(&ldev, 1);
      }
   }
}

#ifdef ENABLE_COMP_BRIDGE

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static const char* amdocl2_funcs_names[] = { "clGetPlatformIDs", "clGetDeviceInfo",
   "clCreateContextFromType", "clGetContextInfo", "clReleaseContext",
   "clCreateProgramWithSource", "clGetProgramInfo", "clGetProgramBuildInfo",
   "clBuildProgram", "clReleaseProgram"
};

#ifndef CL_CONTEXT_OFFLINE_DEVICES_AMD
#define CL_CONTEXT_OFFLINE_DEVICES_AMD              0x403F
#endif


static std::string
trimSpaces(const std::string& s) {
   std::string::size_type pos = s.find_first_not_of(" \n\t\r\v\f");
   if (pos == std::string::npos)
      return "";
   std::string::size_type endPos = s.find_last_not_of(" \n\t\r\v\f");
   return s.substr(pos, endPos+1-pos);
}

static std::string
trimLeftSpaces(const std::string& s) {
   std::string::size_type pos = s.find_first_not_of(" \n\t\r\v\f");
   if (pos == std::string::npos)
      return "";
   return s.substr(pos);
}

void
platform::load_amdocl2() {
   std::string amdocl2_cur_path = amdocl2_path;
   if (amdocl2_cur_path.empty()) {
      // just find
      amdocl2_cur_path = findAmdOCL();
   }
   if (amdocl2_cur_path.empty())
      throw Exception("Can't find AMDOCL2");
   
   amdocl2_dynlib.load(amdocl2_cur_path.c_str(), DYNLIB_NOW);
   
   amdocl2_funcs.reset(new amdocl2_funcs_struct);
   for(size_t i = 0; i < sizeof(amdocl2_funcs_struct)/sizeof(void*); i++)
      amdocl2_funcs->funcs[i] = amdocl2_dynlib.getSymbol(amdocl2_funcs_names[i]);
   
   cl_platform_id platform;
   cl_int errcode = CL_SUCCESS;
   errcode = amdocl2_funcs->fn_clGetPlatformIDs(1, &platform, nullptr);
   if (errcode != CL_SUCCESS)
      return;
   
   // create context
   cl_context_properties clprops[5] =
            { CL_CONTEXT_PLATFORM, (cl_context_properties)platform, CL_CONTEXT_OFFLINE_DEVICES_AMD, 1, 0 };
   amdocl2_context = amdocl2_funcs->fn_clCreateContextFromType(clprops, CL_DEVICE_TYPE_GPU,
                                 nullptr, nullptr, &errcode);
   if (amdocl2_context == nullptr)
      return;
   size_t size;
   if (amdocl2_funcs->fn_clGetContextInfo(
            amdocl2_context, CL_CONTEXT_DEVICES, 0, nullptr, &size) != CL_SUCCESS)
      return;
   std::vector<cl_device_id> devices(size / sizeof(cl_device_id));
   if (amdocl2_funcs->fn_clGetContextInfo( amdocl2_context,
            CL_CONTEXT_DEVICES, size, devices.data(), nullptr) != CL_SUCCESS)
      return;
   
   // get devices and put device types
   for (cl_device_id dev: devices) {
      if (amdocl2_funcs->fn_clGetDeviceInfo(dev, CL_DEVICE_NAME,
                  0, nullptr, &size) != CL_SUCCESS)
         continue;
      std::string dname;
      std::vector<char> dnamebuf(size);
      if (amdocl2_funcs->fn_clGetDeviceInfo(dev, CL_DEVICE_NAME,
                  size, dnamebuf.data(), nullptr) != CL_SUCCESS)
         continue;
      
      dname.assign(dnamebuf.begin(), dnamebuf.end());
      dname = trimSpaces(dname);
      
      const GPUDeviceType devtype = getGPUDeviceTypeFromName(dname.c_str());
      amdocl2_device_map[devtype] = dev;
   }
}

template<typename T>
static void
parse_bridge_value(std::map<T, comp_bridge>& map,
                               T param, const std::string& invalue,
                               int line_no)
{
   auto value = trimSpaces(invalue);
   comp_bridge bridge = comp_bridge::none;
   if (value=="rocm")
      bridge = comp_bridge::rocm;
   else if (value=="amdocl2")
      bridge = comp_bridge::amdocl2;
   else
      throw ParseException(line_no, "Wrong bridge name");
   map[param] = bridge;
}

void
platform::load_config_from_file(const char* filename) try {
   std::ifstream ifs(filename);
   int line_no = 1;
   while (ifs) {
      try {
         std::string line;
         std::getline(ifs, line);
         if (line.empty())
            continue;
         size_t inequal = line.find('=');
         if (inequal==std::string::npos)
            throw ParseException(line_no, "No param line");
         
         std::string param = trimSpaces(line.substr(0, inequal));
         std::string value = line.substr(inequal+1);
         value = trimLeftSpaces(value);
         if (value.empty())
            throw ParseException(line_no, "No value in param line");
         
         if (param == "amdocl2_path") {
            amdocl2_path = value;
         } if (param == "allow_amdocl2_for_gcn14") {
            value = trimSpaces(value);
            allow_amdocl2_for_gcn14 = (value=="true" || value=="1");
         } else if (param.compare(0, 9, "archcomp_")==0) {
            GPUArchitecture arch = GPUArchitecture::GCN1_1;
            try {
               arch = getGPUArchitectureFromName(param.substr(9).c_str());
            } catch (const GPUIdException& ex) {
               throw ParseException(line_no, "Wrong GPU architecture name");
            }
            parse_bridge_value(arch_bridge_map, arch, value, line_no);
         } else if (param.compare(0, 8, "devcomp_")==0) {
            GPUDeviceType devtype = GPUDeviceType::BONAIRE;
            try {
               devtype = getGPUDeviceTypeFromName(param.substr(8).c_str());
            } catch (const GPUIdException& ex) {
               throw ParseException(line_no, "Wrong GPU device type name");
            }
            parse_bridge_value(dev_bridge_map, devtype, value, line_no);
         }
         line_no++;
      } catch(const ParseException& ex) {
         std::cerr << "Parse Error while loading " << filename <<
               " comp bridge config: " << ex.what() << std::endl;
      }
   }
} catch (const std::exception& ex) {
   // just ignore
   std::cerr << "Error while loading " << filename << " comp bridge config: " << 
   ex.what() << std::endl;
   return;
}

void
platform::load_config() {
   std::string configfilename(SYSCONFDIR);
   configfilename += "/clover_compbridge";
   load_config_from_file(configfilename.c_str());
   configfilename = getHomeDir();
   configfilename += "/.clover_compbridge";
   load_config_from_file(configfilename.c_str());
   const char* in_amdocl2_path = getenv("CLOVER_AMDOCL2_PATH");
   if (in_amdocl2_path != nullptr)
      amdocl2_path = in_amdocl2_path;
}
#endif
