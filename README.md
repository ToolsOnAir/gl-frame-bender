gl-frame-bender: An OpenGL 4.x video processing benchmark
=========================================================

Introduction 
------------

gl-frame-bender is a benchmark tool running a simple video rendering scenario
using OpenGL 4.x techniques. The basic process is to 

* Copy an input video frame from main memory to PBO-mapped memory
* Upload the frame to GPU memory
* Convert the video's luma/chroma encoding into linearly-coded RGB color space
* Overlay simple graphics
* Convert RGB back to the video's luma/chroma encoding
* Download the video frame to PBO memory
* Copy the PBO-mapped memory to a destination pointer in memory

These steps are implemented by a software pipeline pattern, where individual
stages execute tasks and transport data downstream (towards the output) and
upstream (towards the input). The algorithms of those stages, and their
threading is highly configurable by the user for each run. During execution of
a stage's task, the begin and end times are optionally recorded into a trace
file (using CPU timers and GPU timers). This file can later be used to analyze
the efficiency of the options chosen by the user. As an example: Using multiple
GL contexts for upload/render/download increases the performance on some GPU
architectures (e.g. NVIDIA Quadro cards), but shows worse performance on
others.

The video source and destination format that is currently supported is a 10-bit
Y'CbCr interleaved 4:2:2 format (aka V210). Decoder and encoders to/from
linearly-coded RGB are implemented in multiple variations (GLSL 3.3, 4.2 and
4.3 compute), as a showcase how modern GPU features can help to implement these
algorithms more efficiently.

*DISCLAIMER*: Please note that this code has emerged from an academic project
(see [further documentation][further documentation]), and has been used solely
for the purpose of testing and measuring the performance of certain OpenGL
video processing algorithms. The author does not recommend to use this
framework in a more generic video processing scenario. gl-frame-bender is
understood as a sandbox and as sample code for certain GL techniques.

Used OpenGL features
--------------------

The following OpenGL features are either used throughout the benchmark, or
available as a program option.

* OpenGL 4.3 core profile
* Compute shaders
* GLSL image/load store operations
* Immutable texture storage 
* ARB_buffer_storage (immutable buffer storage, and persistent PBO mapping)
* Native GLSL integer bitfield operations for 10-bit unpacking 
(vs. RGB10UI_A2UI)
* GL fences used client-side and server-side to synchronise GPU buffer 
access
* ARB_debug_output used to insert GL logging into application-side logging
* ARB_framebuffer_no_attachment

The V210 encoder/decoder that comes with the benchmark is implemented using in
various flavors of GLSL:
* "GLSL 330": Standard fragment shader where a single invocation writes a
  single pixel of the destination format.
* "GLSL 420": Using the GLSL image load/store path, a single shader invocation
  of the decoder reads 4 32-bit words of V210-encoded data, and writes 6 RGB
pixels (vice-versa for the encoder).
* "GLSL 430": Same as GLSL 420, but tries to cache intermediate steps into
  memory that is shared between work groups. Optionally, this step can be
ommitted, resulting effectively in a very similar path as the "GLSL 420"
variant.

Project structure 
-----------------

`./lib` contains the source files and headers of the gl-frame-bender library

`./gl-frame-bender` is an application linking against the gl-frame-bender
library

`./glsl contains` the GLSL shaders for format conversion and rendering. Note
that the framework will preprocess those shaders before they are actually being
loaded by OpenGL (i.e. using the fb_include tag).

`./resources` contains a bunch of preset configuration files, which you can
use via the "-c" command line argument with gl-frame-bender

`./scripts` has several scripts that are used for running batched benchmarking
runs and for visualizing the performance trace files

`./test_data` contains raw, uncompressed video frames test data that are used
mainly for testing. You [can acquire larger test sequences][larger test
sequences] as well for playback.

`./textures` has images that are used for the overlay rendering operation

`./utils` hosts C++ utility applications for preparing test data, and PSNR
measurement of V210 raw frames

Build instructions
------------------

CMake is used to create build files for Linux and Windows.

Commong build requirements: 

* CMake >= 3.0.0
* Boost >= 1.54
* Google prototbuf >= 2.6.0 
* GLFW >= 3
* GLM library
* DevIL library
* Intel IPP library (optional)

### Linux

* Clang or gcc with C++11 support is required
* All library dependencies need to be install by the user before compiling

### Windows

* Visual Studio 2013 is required
* 64-bit prebuilt libraries of all dependencies except Boost are provided. See
  below for instructions how to point CMake to use the right boost
installation.

Note that only 64-builds are supported. As such, it is mandatory to use the
"Visual Studio 12 2013 Win64" generator with CMake. No other Visual Studio
version / architectures are currently supported.

#### Configuring Boost paths

It is required to install Boost before compiling (it will be statically linked,
though, so Boost is not required to run the benchmark). Once installed, CMake
needs to know where to look for the Boost installation. This is done via the
`USER_BOOST_ROOT` CMake variable (exposed via cmake-gui config dialogue, or
passed via command-line arguments to cmake).

By default, `USER_BOOST_ROOT` points to `C:/local/boost_1_57_0`.

### Using the Intel IPP library

Host-side buffer copy operation can be accelerated by using
`ippiCopyManaged_8u_C1R` instead of a standard `memcpy`. This will use
non-temporary store operations on Intel CPUs and as such avoid thrashing
caches when copying large amounts of data. gl-frame-bender will use this managed
copy operation, if CMake is pointed to a local Intel IPP installation. This is
done by setting the following CMake variables:

`IPP_USER_INCLUDE_DIR`: Path to the IPP headers

`IPP_USER_LIBRARY_DIR`: Path containing the IPP library binaries

`USE_INTEL_IPP`: If active (i.e. set to true), the CMake build script will
assume to have Intel IPP installed, and use the above variables to configure
the generated build files.

Example Linux: 

```
IPP_USER_INCLUDE_DIR=/opt/intel/composer_xe_2015.2.164/ipp/include 
IPP_USER_LIBRARY_DIR=/opt/intel/composer_xe_2015.2.164/ipp/lib/intel64 
USE_INTEL_IPP=true
```
Example Windows:

```
IPP_USER_INCLUDE_DIR=C:/Program Files (x86)/Intel/Composer XE 2015/ipp/include
IPP_USER_LIBRARY_DIR=/opt/intel/composer_xe_2015.2.164/ipp/lib/intel64 
USE_INTEL_IPP=true

```

Note that on Windows, Intel IPP is linked statically, and gl-frame-bender uses
the new naming schema of Intel IPP libs (ippimt.a vs. ippi_l.a).

Running unit tests
------------------

After successful compilation, the folder `gl-frame-bender-tests` will contain
an executable `gl-frame-bender-tests` (on Windows it will further be moved into
a folder named after the active configure, e.g. `Release`). 

In order to run all unit tests, simply execute the executable. The unit test
runner is based on *Boost test*. Please read the *Boost test* documentation to
learn about possible command line arguments for test selection, filtering and
logging.

Running gl-frame-bender
-----------------------

The `gl-frame-bender` executable (built under `BUILD_FOLDER/gl-frame-bender`)
exposes many options to the user. Run `gl-frame-bender --help` for a full
listing and description of available parameters.

In addition to command-line arguments, a file with configuration parameters
presets can be loaded using the `-c` option. The build process copies two
simple preset configurations for convenience: 

* PlayerPreset.cfg: A preset which activates the player-path, i.e. blitting the
  rendered frames to a window with vsync-ed display. Since VSYNC is the only
pacing of the data flow, the test-sequence will be played back at 60hz.

* SimpleBenchmarkPreset.cfg: A preset that deactivates the player option, and
  activates the performance tracing paths.

### Running the player mode

Start gl-frame-bender using `gl-frame-bender -c PlayerPreset.cfg`.

The source repository only contains a limited number of test frames. You can
download a larger set of video frames from [here]
(http://files.hfink.eu/horse_v210_1920_1080p.tar.xz). Extract the archive to
your `test_data` folder, and launch gl-frame-bender like this:

`gl-frame-bender -c PlayerPreset.cfg --input.sequence.id=horse_v210_1920_1080p`

Notice that options explicitly set on the command line will always override
options in the configuration files.

A 4K test sequence is also available from
[here](http://files.hfink.eu/rain_fruits_v210_3820_2160p.tar.xz), in which case
the dimensions of the input source also have to be set explicitly: 

`gl-frame-bender -c PlayerPreset.cfg --input.sequence.id=rain_fruits_v210_3820_2160p --input.sequence.width=3840 --input.sequence.height=2160`

All test sequences were losslessly converted from the [EBU UHD-1 test sequences](https://tech.ebu.ch/testsequences/uhd-1).

### Creating performance traces 

To run a single simple benchmark run, execute `gl-frame-bender -c
SimpleBenchmarkPreset.cfg`. This will create a trace file in the same folder
named `trace.fbt` and print out the average throughput on standard output.

Visualizing performance traces 
------------------------------

The python script `scripts/trace_munch.py` can be used to generated PDF
diagrams from the trace.fbt file created above. Note that this might require to
install additional dependencies, like python-cairo, etc.

Creating performance traces in batches
--------------------------------------

See `scripts/run_analysis.py`.

TODO: Update parts about batched performance traces, CSV summaries, etc.

Further documentation 
---------------------

Presentation talk about gl-frame-bender at NVIDIA GTC 2015:

http://registration.gputechconf.com/quicklink/1X6eJp2

Heinrich Fink's master thesis, which describes most of the theory and
functionality of this framework: 

http://www.cg.tuwien.ac.at/research/publications/2013/fink-2013-gvp/

