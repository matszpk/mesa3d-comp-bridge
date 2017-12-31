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

#include <type_traits>
#include <iostream>
#ifdef ENABLE_COMP_BRIDGE
#include <algorithm>
#include <string>
#include <iterator>
#include <vector>
#include <map>
#include <cstring>
#include <CLRX/amdbin/AmdCL2Binaries.h>
#include <CLRX/amdbin/Commons.h>
#include <CLRX/amdbin/GalliumBinaries.h>
#include <CLRX/utils/InputOutput.h>
#endif

#include "core/module.hpp"

using namespace clover;
#ifdef ENABLE_COMP_BRIDGE
using namespace CLRX;
#endif

namespace {
   template<typename T, typename = void>
   struct _serializer;

   /// Serialize the specified object.
   template<typename T>
   void
   _proc(std::ostream &os, const T &x) {
      _serializer<T>::proc(os, x);
   }

   /// Deserialize the specified object.
   template<typename T>
   void
   _proc(std::istream &is, T &x) {
      _serializer<T>::proc(is, x);
   }

   template<typename T>
   T
   _proc(std::istream &is) {
      T x;
      _serializer<T>::proc(is, x);
      return x;
   }

   /// Calculate the size of the specified object.
   template<typename T>
   void
   _proc(module::size_t &sz, const T &x) {
      _serializer<T>::proc(sz, x);
   }

   /// (De)serialize a scalar value.
   template<typename T>
   struct _serializer<T, typename std::enable_if<
                            std::is_scalar<T>::value>::type> {
      static void
      proc(std::ostream &os, const T &x) {
         os.write(reinterpret_cast<const char *>(&x), sizeof(x));
      }

      static void
      proc(std::istream &is, T &x) {
         is.read(reinterpret_cast<char *>(&x), sizeof(x));
      }

      static void
      proc(module::size_t &sz, const T &x) {
         sz += sizeof(x);
      }
   };

   /// (De)serialize a vector.
   template<typename T>
   struct _serializer<std::vector<T>,
                      typename std::enable_if<
                         !std::is_scalar<T>::value>::type> {
      static void
      proc(std::ostream &os, const std::vector<T> &v) {
         _proc<uint32_t>(os, v.size());

         for (size_t i = 0; i < v.size(); i++)
            _proc<T>(os, v[i]);
      }

      static void
      proc(std::istream &is, std::vector<T> &v) {
         v.resize(_proc<uint32_t>(is));

         for (size_t i = 0; i < v.size(); i++)
            new(&v[i]) T(_proc<T>(is));
      }

      static void
      proc(module::size_t &sz, const std::vector<T> &v) {
         sz += sizeof(uint32_t);

         for (size_t i = 0; i < v.size(); i++)
            _proc<T>(sz, v[i]);
      }
   };

   template<typename T>
   struct _serializer<std::vector<T>,
                      typename std::enable_if<
                         std::is_scalar<T>::value>::type> {
      static void
      proc(std::ostream &os, const std::vector<T> &v) {
         _proc<uint32_t>(os, v.size());
         os.write(reinterpret_cast<const char *>(&v[0]),
                  v.size() * sizeof(T));
      }

      static void
      proc(std::istream &is, std::vector<T> &v) {
         v.resize(_proc<uint32_t>(is));
         is.read(reinterpret_cast<char *>(&v[0]),
                 v.size() * sizeof(T));
      }

      static void
      proc(module::size_t &sz, const std::vector<T> &v) {
         sz += sizeof(uint32_t) + sizeof(T) * v.size();
      }
   };

   /// (De)serialize a string.
   template<>
   struct _serializer<std::string> {
      static void
      proc(std::ostream &os, const std::string &s) {
         _proc<uint32_t>(os, s.size());
         os.write(&s[0], s.size() * sizeof(std::string::value_type));
      }

      static void
      proc(std::istream &is, std::string &s) {
         s.resize(_proc<uint32_t>(is));
         is.read(&s[0], s.size() * sizeof(std::string::value_type));
      }

      static void
      proc(module::size_t &sz, const std::string &s) {
         sz += sizeof(uint32_t) + sizeof(std::string::value_type) * s.size();
      }
   };

   /// (De)serialize a module::section.
   template<>
   struct _serializer<module::section> {
      template<typename S, typename QT>
      static void
      proc(S &s, QT &x) {
         _proc(s, x.id);
         _proc(s, x.type);
         _proc(s, x.size);
         _proc(s, x.data);
      }
   };

   /// (De)serialize a module::argument.
   template<>
   struct _serializer<module::argument> {
      template<typename S, typename QT>
      static void
      proc(S &s, QT &x) {
         _proc(s, x.type);
         _proc(s, x.size);
         _proc(s, x.target_size);
         _proc(s, x.target_align);
         _proc(s, x.ext_type);
         _proc(s, x.semantic);
      }
   };

   /// (De)serialize a module::symbol.
   template<>
   struct _serializer<module::symbol> {
      template<typename S, typename QT>
      static void
      proc(S &s, QT &x) {
         _proc(s, x.name);
         _proc(s, x.section);
         _proc(s, x.offset);
         _proc(s, x.args);
      }
   };

   /// (De)serialize a module.
   template<>
   struct _serializer<module> {
      template<typename S, typename QT>
      static void
      proc(S &s, QT &x) {
         _proc(s, x.syms);
         _proc(s, x.secs);
      }
   };
};

#ifdef ENABLE_COMP_BRIDGE
struct gallium_argtype_info {
   GalliumArgType type;
   bool sign_extended;
   uint32_t size;
   uint32_t align;
};

static const gallium_argtype_info gallium_to_amdocl2_argtype_table[] = {
   { GalliumArgType::SCALAR, false, 1, 4 }, // VOID
   { GalliumArgType::SCALAR, false, 1, 1 }, // UCHAR
   { GalliumArgType::SCALAR, false, 1, 1 }, // CHAR
   { GalliumArgType::SCALAR, false, 2, 2 }, // USHORT
   { GalliumArgType::SCALAR, false, 2, 2 }, // SHORT
   { GalliumArgType::SCALAR, false, 4, 4 }, // UINT
   { GalliumArgType::SCALAR, false, 4, 4 }, // INT
   { GalliumArgType::SCALAR, false, 8, 8 }, // ULONG
   { GalliumArgType::SCALAR, false, 8, 8 }, // LONG
   { GalliumArgType::SCALAR, false, 4, 4 }, // FLOAT
   { GalliumArgType::SCALAR, false, 8, 8 }, // DOUBLE
   { GalliumArgType::GLOBAL, false, 8, 8 }, // POINTER
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE1D
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE1D_ARRAY
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE1D_BUFFER
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE2D
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE2D_ARRAY
   { GalliumArgType::SCALAR, false, 0, 0 }, // IMAGE3D
   { GalliumArgType::SCALAR, false, 2, 1 }, // UCHAR2
   { GalliumArgType::SCALAR, false, 4, 1 }, // UCHAR3
   { GalliumArgType::SCALAR, false, 4, 1 }, // UCHAR4
   { GalliumArgType::SCALAR, false, 8, 1 }, // UCHAR8
   { GalliumArgType::SCALAR, false, 16, 1 }, // UCHAR16
   { GalliumArgType::SCALAR, false, 2, 1 }, // CHAR2
   { GalliumArgType::SCALAR, false, 4, 1 }, // CHAR3
   { GalliumArgType::SCALAR, false, 4, 1 }, // CHAR4
   { GalliumArgType::SCALAR, false, 8, 1 }, // CHAR8
   { GalliumArgType::SCALAR, false, 16, 1 }, // CHAR16
   { GalliumArgType::SCALAR, false, 4, 2 }, // USHORT2
   { GalliumArgType::SCALAR, false, 8, 2 }, // USHORT3
   { GalliumArgType::SCALAR, false, 8, 2 }, // USHORT4
   { GalliumArgType::SCALAR, false, 16, 2 }, // USHORT8
   { GalliumArgType::SCALAR, false, 32, 2 }, // USHORT16
   { GalliumArgType::SCALAR, false, 4, 2 }, // SHORT2
   { GalliumArgType::SCALAR, false, 8, 2 }, // SHORT3
   { GalliumArgType::SCALAR, false, 8, 2 }, // SHORT4
   { GalliumArgType::SCALAR, false, 16, 2 }, // SHORT8
   { GalliumArgType::SCALAR, false, 32, 2 }, // SHORT16
   { GalliumArgType::SCALAR, false, 8, 4 }, // UINT2
   { GalliumArgType::SCALAR, false, 16, 4 }, // UINT3
   { GalliumArgType::SCALAR, false, 16, 4 }, // UINT4
   { GalliumArgType::SCALAR, false, 32, 4 }, // UINT8
   { GalliumArgType::SCALAR, false, 64, 4 }, // UINT16
   { GalliumArgType::SCALAR, false, 8, 4 }, // INT2
   { GalliumArgType::SCALAR, false, 16, 4 }, // INT3
   { GalliumArgType::SCALAR, false, 16, 4 }, // INT4
   { GalliumArgType::SCALAR, false, 32, 4 }, // INT8
   { GalliumArgType::SCALAR, false, 64, 4 }, // INT16
   { GalliumArgType::SCALAR, false, 16, 8 }, // ULONG2
   { GalliumArgType::SCALAR, false, 32, 8 }, // ULONG3
   { GalliumArgType::SCALAR, false, 32, 8 }, // ULONG4
   { GalliumArgType::SCALAR, false, 64, 8 }, // ULONG8
   { GalliumArgType::SCALAR, false, 128, 8 }, // ULONG16
   { GalliumArgType::SCALAR, false, 16, 8 }, // LONG2
   { GalliumArgType::SCALAR, false, 32, 8 }, // LONG3
   { GalliumArgType::SCALAR, false, 32, 8 }, // LONG4
   { GalliumArgType::SCALAR, false, 64, 8 }, // LONG8
   { GalliumArgType::SCALAR, false, 128, 8 }, // LONG16
   { GalliumArgType::SCALAR, false, 8, 4 }, // FLOAT2
   { GalliumArgType::SCALAR, false, 16, 4 }, // FLOAT3
   { GalliumArgType::SCALAR, false, 16, 4 }, // FLOAT4
   { GalliumArgType::SCALAR, false, 32, 4 }, // FLOAT8
   { GalliumArgType::SCALAR, false, 64, 4 }, // FLOAT16
   { GalliumArgType::SCALAR, false, 16, 8 }, // DOUBLE2
   { GalliumArgType::SCALAR, false, 32, 8 }, // DOUBLE3
   { GalliumArgType::SCALAR, false, 32, 8 }, // DOUBLE4
   { GalliumArgType::SCALAR, false, 64, 8 }, // DOUBLE8
   { GalliumArgType::SCALAR, false, 128, 8 }, // DOUBLE16
   { GalliumArgType::SCALAR, false, 0, 0 }, // SAMPLER
   { GalliumArgType::GLOBAL, false, 8, 8 }, // STRUCTURE
   { GalliumArgType::SCALAR, false, 0, 0 }, // COUNTER32
   { GalliumArgType::SCALAR, false, 0, 0 }, // COUNTER64
   { GalliumArgType::SCALAR, false, 0, 0 }, // PIPE
   { GalliumArgType::SCALAR, false, 0, 0 }, // CMDQUEUE
   { GalliumArgType::SCALAR, false, 0, 0 } // CLKEVENT
};

static GalliumArgInfo
convert_amdocl2_arginfo_to_gallium_arginfo(const AmdKernelArg& arg_info) {
   if (arg_info.argType == KernelArgType::STRUCTURE)
      // this is not real structure (will be modifier to real later after deserialization)
      return { GalliumArgType::GLOBAL, false, GalliumArgSemantic::GENERAL,
            0, 0, 256 };
   
   const auto& gtype = gallium_to_amdocl2_argtype_table[cxint(arg_info.argType)];
   if (gtype.size == 0)
      throw Exception("Unsupported type");
   GalliumArgInfo garg { gtype.type, gtype.sign_extended, GalliumArgSemantic::GENERAL,
            gtype.size, gtype.size, gtype.align };
   if (arg_info.ptrSpace == KernelPtrSpace::LOCAL)
      garg.type = GalliumArgType::LOCAL;
   return garg;
}
#endif

namespace clover {
   void
   module::serialize(std::ostream &os) const {
      _proc(os, *this);
   }

   module
   module::deserialize(std::istream &is) {
      return _proc<module>(is);
   }

   module::size_t
   module::size() const {
      size_t sz = 0;
      _proc(sz, *this);
      return sz;
   }
   
#ifdef ENABLE_COMP_BRIDGE
   module
   module::create_from_amdocl2_binary(const AmdCL2MainGPUBinary64* binary,
               GPUDeviceType devtype) {
      const GPUArchitecture arch = getGPUArchitectureFromDeviceType(devtype);
      const AmdCL2InnerGPUBinary& inner = binary->getInnerBinary();
      const Elf64_Shdr& shdr = inner.getSectionHeader(".hsatext");
      size_t hsatext_size = ULEV(shdr.sh_size);
      const cxbyte* hsatext_orig = inner.getBinaryCode() + ULEV(shdr.sh_offset);
      std::unique_ptr<cxbyte[]> hsatext(new cxbyte[hsatext_size]);
      ::memcpy(hsatext.get(), hsatext_orig, hsatext_size);
      
      std::map<CString, std::vector<size_t>> structsMap;
      
      GalliumInput ginput{};
      ginput.is64BitElf = true;
      ginput.isLLVM390 = true;
      ginput.isMesa170 = true;
      ginput.deviceType = devtype;
      ginput.codeSize = hsatext_size;
      ginput.code = hsatext.get();
      ginput.globalDataSize = inner.getGlobalDataSize();
      ginput.globalData = inner.getGlobalData();
      //std::cout << "globalDataSize: " << ginput.globalDataSize << std::endl;
      for (size_t i = 0; i < binary->getKernelInfosNum(); i++) {
         const auto& kinfo = binary->getKernelInfo(i);
         const auto& bkernel = inner.getKernelData(i);
         GalliumKernelInput gkernel{ kinfo.kernelName };
         gkernel.offset = bkernel.setup - hsatext_orig;
         gkernel.useConfig = false;
         AmdHsaKernelConfig& hsaConfig = *(AmdHsaKernelConfig*)
                  (hsatext.get() + gkernel.offset);
         const size_t localSize = ULEV(hsaConfig.workgroupGroupSegmentSize);
         // fix compute pgmrsrc2
         SULEV(hsaConfig.computePgmRsrc2, ULEV(hsaConfig.computePgmRsrc2) |
               calculatePgmRSrc2(arch, false, 0, false, 0, 0, false, localSize, false));
         gkernel.progInfo[0].address = ULEV(0x0000b848U);
         gkernel.progInfo[0].value = hsaConfig.computePgmRsrc1;
         gkernel.progInfo[1].address = ULEV(0x0000b84cU);
         gkernel.progInfo[1].value = hsaConfig.computePgmRsrc2;
         gkernel.progInfo[2].address = ULEV(0x0000b860U);
         gkernel.progInfo[2].value = ULEV(hsaConfig.workitemPrivateSegmentSize)<<12;
         gkernel.progInfo[3].address = ULEV(0x00000004U);
         gkernel.progInfo[3].value = 0; // ?
         gkernel.progInfo[4].address = ULEV(0x00000008U);
         gkernel.progInfo[4].value = 0; // ?
         
         {  // get argument structure size
            const cxbyte* metadata = binary->getMetadata(i);

            const AmdCL2GPUMetadataHeader64* mdHdr =
                     reinterpret_cast<const AmdCL2GPUMetadataHeader64*>(metadata);
            size_t headerSize = ULEV(mdHdr->size);
            size_t argOffset = headerSize + ULEV(mdHdr->firstNameLength) + 
                     ULEV(mdHdr->secondNameLength)+2;
            if (ULEV(*(const uint32_t*)(metadata+argOffset)) ==
                        (sizeof(AmdCL2GPUKernelArgEntry64)<<8))
               argOffset++;    // fix for AMD GPUPRO driver (2036.03) */
            const AmdCL2GPUKernelArgEntry64* argPtr = reinterpret_cast<
                     const AmdCL2GPUKernelArgEntry64*>(metadata + argOffset);
            
            // mark all structure arguments
            structsMap[kinfo.kernelName].resize(kinfo.argInfos.size()-6);
            for (size_t ka = 6; ka < kinfo.argInfos.size(); ka++)
                  structsMap[kinfo.kernelName][ka-6] =
                        (kinfo.argInfos[ka].argType == KernelArgType::STRUCTURE) ?
                              ULEV(argPtr[ka].structSize) : 0;
            
         }
         // convert arguments
         std::transform(kinfo.argInfos.begin()+6, kinfo.argInfos.end(), 
               std::back_inserter(gkernel.argInfos),
                        convert_amdocl2_arginfo_to_gallium_arginfo);
         
         gkernel.argInfos.push_back({GalliumArgType::SCALAR, false,
                  GalliumArgSemantic::GRID_OFFSET, 8, 8, 8 });
         
         ginput.kernels.push_back(gkernel);
      }
      
      module gmod;
      {
         GalliumBinGenerator bingen(&ginput);
         Array<cxbyte> out;
         bingen.generate(out);
         {
            ArrayIStream istream(out.size(), (char*)out.data());
            gmod = module::deserialize(istream);
         }
      }
      // modify all structure arguments
      for (auto& sym: gmod.syms) {
         auto it = structsMap.find(sym.name);
         if (it == structsMap.end()) continue;
         for (size_t j = 0; j < it->second.size(); j++)
            if (it->second[j]) {
               sym.args[j].type = module::argument::structure;
               sym.args[j].size = sym.args[j].target_size = it->second[j];
            }
      }
      
      /* put constant (global data relocs) */
      // getting optional sections in inner binary
      uint16_t gDataSectionIdx = SHN_UNDEF;
      try
      { gDataSectionIdx = inner.getSectionIndex(".hsadata_readonly_agent"); }
      catch(const Exception& ex)
      { }
      for (size_t i = 0; i < inner.getTextRelaEntriesNum(); i++) {
         const Elf64_Rela& rela = inner.getTextRelaEntry(i);
         uint32_t symIndex = ELF64_R_SYM(ULEV(rela.r_info));
         int64_t addend = ULEV(rela.r_addend);
         const Elf64_Sym& sym = inner.getSymbol(symIndex);
         uint16_t symShndx = ULEV(sym.st_shndx);
         if (symShndx!=gDataSectionIdx)
            throw Exception("Symbol is not placed in global data");
         addend += ULEV(sym.st_value);
         typename module::reloc::type reloc_type;
         uint32_t rtype = ELF64_R_TYPE(ULEV(rela.r_info));
         if (rtype==1)
            reloc_type = module::reloc::low_32bit;
         else if (rtype==2)
            reloc_type = module::reloc::high_32bit;
         else
            throw Exception("Unknown relocation type");
         
         /*std::cout << "reloc: " << reloc_type << ", " << ULEV(rela.r_offset) <<
                     ", " << addend << "\n";*/
         gmod.relocs.push_back({reloc_type, ULEV(rela.r_offset), addend});
      }
      
      /*for (const auto& sym: gmod.syms) {
         std::cout << "sym: " << sym.name << "\n";
         for (const auto& arg: sym.args)
            std::cout << "  arg: sem=" << int(arg.semantic) << ", "
                  "type=" << int(arg.type) << ", ext=" << int(arg.ext_type) << ", " <<
                  arg.size << ", " << arg.target_size << ", " <<
                  arg.target_align << "\n";
      }
      std::cout.flush();*/
      return gmod;
   }
#endif
}
