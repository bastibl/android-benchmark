# GNU Radio Benchmarks for Android

This is an Android app that benchmarks GNU Radio. Similar to [our paper](https://www.bastibl.net/bib/bloessl2019benchmarking/), it looks into the scaling behavior of the runtime environment for a large number of blocks. Please see the paper for more information on the experiment setup.

Besides runtime performance, it also compares the throughput of OpenCL accelerated blocks with upstream GNU Radio blocks. These measurements are adapted from [gr-clenabled](https://github.com/ghostop14/gr-clenabled). Note that there is no Android OpenCL API, i.e., this requires some manual setup of the libraries (mainly copying OpenCL libraries from the phone into the toolchain on the PC, see the documentation of the [GNU Radio Android toolchain](https://github.com/bastibl/gnuradio-android/)).

Most of the GNU Radio blocks that are compared to OpenCL use Volk. So make sure to run [volk profile](https://github.com/bastibl/android-volk/) before running the measurements.

The results stored as CSVs on *External Storage* (which might be an SD card or some internal storage).

## Installation

Building the app requires the [GNU Radio Android toolchain](https://github.com/bastibl/gnuradio-android/). Please see this repository for further instructions on how to build the toolchain and apps that use it.

## Running the App

The app is minimalistic. There are only three buttons to start the three different measurements (runtime buffers, runtime async message, OpenCL vs CPU).

## Exemplary Results
=
Scaling of GNU Radio data streams on a OnePlus 5T smartphone vs a similar setup on a laptop with an i7.

![Buffer Scaling](doc/buf.png)

Throughput of OpenCL kernels with increasing buffer sizes.

![OpenCL vs CPU](doc/cl_vs_cpu.png)
