## Mesa3D Clover Compiler's bridge

This version of the Mesa3D has the special feature: Compiler bridge that allow to use
AMDGPU-PRO OpenCL compiler (AMDOCL) under Clover OpenCL driver.
Moreover, that feature allow to use
this compiler to build OpenCL programs for the Radeon RX VEGA device.
This feature allow to run the OpenCL applications that will be designed especially
for the AMD OpenCL platform (like, the miners, some the BOINC applications) without changes in
the OpenCL code.

This feature requires a working AMDGPU-PRO driver to run code from AMDGPU-PRO OpenCL driver.
This feature will be not working under same open-source AMDGPU driver or RadeonSI driver.
This code should be working under 64-bit x86-64 Linux systems. The 32-bit x86 architecture
is not supported.

This feature has been tested under OpenSUSE 14.2 and a AMDGPU-PRO 17.40 drivers.

### Building

I recommend to read the documentation about the installation (in the `docs` direcrory).

Before building this Mesa3D, you should install CLRadeonExtender 0.1.7svn or later.
Links to CLRadeonExtender: [http://clrx.nativeboinc.org](http://clrx.nativeboinc.org) or
[https://github.com/CLRX/CLRX-mirror](https://github.com/CLRX/CLRX-mirror).
Code of this feature need that package.

After that, you should create configure scripts by using command `./autogen.sh`. You can use
NOCONFIGURE variable to skip configuration.

I recommend to build Mesa3D in child directory like `build` and install in directory other
than system (not in `/usr`) like in `/opt/mymesa`.

```
mkdir build
cd build
../configure ....
make
...
```

While entering 'configure' command you should include '--enable-comp-bridge' option to enable
this a Compiler's bridge. Ofcourse, you should add gallium drivers: radeonsi and DRI drivers
drivers: radeon.

Example configuration command:

```
./configure --prefix=/opt/mymesa \
    --enable-opencl --enable-opencl-icd --enable-gbm --with-llvm-prefix=/opt/mymesa \
    --with-platforms=drm,x11 --with-dri-drivers=swrast,radeon,i915,i965 \
    --with-gallium-drivers=svga,r600,radeonsi --enable-glx-tls \
    --enable-comp-bridge
```

If you install other components (LLVM, libclc) in directory other than `/usr` then you remember
about setting the PKG_CONFIG_PATH variable before entering `configure` command.

After configuration, you can just build Mesa3D by command: `make`

Install it by using command: `make install` (you should be root if you install in
the system directory).

### Configuration

Now you can use built Mesa3D Clover with this feature. If you enable OpenCL ICD (
configure option `--enable-opencl-icd`) you should add OpenCL vendor to `/etc/OpenCL/vendors`.
Just write full path to libMesaOpenCL library to some ICD file in a `/etc/OpenCL/vendors`:

```
echo /opt/mymesa/lib64/libMesaOpenCL.so > /etc/OpenCL/vendors/mymesa.icd
```

Otherwise, you must set LD_LIBRARY_PATH to path of a libMesaOpenCL directory if you install
this Mesa3D in directory other than `/usr`:

```
export LD_LIBRARY_PATH="/opt/mymesa/lib64:$LD_LIBRARY_PATH"
```

By default, a COMP_BRIDGE feature is disabled. The simple configuration file allow to
enable this feature to particular devices. You can put this file as `/etc/clover_compbridge` or
`~/.clover_compbridge` (`.clover_compbridge` in your home directory).
The syntax of this file is simple:

```
archcomp_gcn1.1=amdocl2
archcomp_gcn1.2=amdocl2
archcomp_gcn1.4=amdocl2
allow_amdocl2_for_gcn14=1
```

or

```
devcomp_hawaii=amdocl2
devcomp_baffin=amdocl2
devcomp_gfx900=amdocl2
allow_amdocl2_for_gcn14=1
```

The `archomp_gcn1.X` enables the AMDGPU-PRO compiler for the GCN1.X devices.
The `devcomp_XXXX` enables the AMDGPU-PRO compiler for particular device, where XXX is
a name of the GPU device. The value of these parameters is compiler bridge.
Currently, only 'amdocl2' is working.

The list of the GPU architectures:

* gcn1.1 - GCN 2 (Radeon 2XX)
* gcn1.2 - GCN 3/4 (Radeon 3XX/Fury/4XX)
* gcn1.4 - GCN 5 (Radeon VEGA)

The list of the GPU devices:

* bonaire - Radeon R7 260/360
* spectre
* spooky
* kalindi
* mullins
* iceland
* hawaii - Radeon R9 290/390
* carrizo - Carrizo GPU
* tonga - Radeon R9 285/385
* fiji - Radeon Fury/Fury X
* baffin - Radeon 460/560
* ellesmere - Radeon RX 470/480/570/580
* gfx804 - Radeon 550
* gfx900 - Radeon RX VEGA
* gfx901 - Ryzen APU (???)

If you want to enable an AMDGPU-PRO compiler for devices of a GCN 5 (gcn1.4) architecture,
you shall to add line `allow_amdocl2_for_gcn14=1` or `allow_amdocl2_for_gcn14=true`.

By default this Mesa3D Clover version automatically find needed `libamdocl64.so` library.
If your needed `libamdocl64.so` library is in different place in filesystem you can add
path to that configuration file:

````
amdocl2_path=/yourpathtoamdocl64.so
````

Finally, your configuration file can appear like that:

```
archcomp_gcn1.1=amdocl2
archcomp_gcn1.2=amdocl2
archcomp_gcn1.4=amdocl2
allow_amdocl2_for_gcn14=1
amdocl2_path=/opt/myamdgpupro/lib64/libamdocl64.so
```

### The limitations

This feature only allow to use an AMDGPU-PRO compiler under Clover control. This feature
does not solve many limitations (slower performance,
slower transfer between the host and a device, a limited functionality, etc).

Currently, only a compiler from libamdocl64.so (no ROCm OpenCL compiler)
library can be used, except legacy compiler (enabled by `-legacy` build option). Just,
you can not use AMDOCL compiler for the first GCN generation devices (Radeon HD 7XXX), because
this code implements bridge for new AMDCL2 compiler, but not older legacy AMDCL1.2 compiler.

The OpenCL code build by using this AMDOCL compiler bridge can use only OpenCL 1.1 features,
except an images handling (because Clover doesn't support for RadeonSI devices) and
the OpenCL 2.0 features (likes, pipes, kernel enqueueing, etc). An OpenCL code can use
the special AMD extensions likes amd_bitalign and others special functions.

### Radeon RX VEGA notes

Because, an AMDOCL2 compiler for the Linux doesn't supports Radeon RX VEGA (only ROCm compiler support that devices), hence COMP_BRIDGE build code for GCN 3 architecture and uses and run
the generated code under RX VEGA. A GCN 5 architecture is compatible with GCN 3/4 in
99 percents, however some less important difference are exists. Some code which uses
half float point loading/store can be incorrectly working under RX VEGA.
A code that uses the V_MOVRELXX instructions (removed in GCN 5) will be
working incorrectly too.

### Building the OpenCL programs from binaries

This feature substitutes default Clover compiler by AMDGPU-PRO compiler.
It does not disable ability to build programs from Clover OpenCL binaries. A code this feature
same recognizes the type of a binary and choose suitable path to prepare that binary.
