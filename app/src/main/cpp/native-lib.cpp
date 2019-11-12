#include <jni.h>
#include <android/log.h>

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <boost/algorithm/string.hpp>

#include <gnuradio/logger.h>
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/null_source.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/head.h>
#include <gnuradio/blocks/copy.h>
#include <gnuradio/blocks/nlog10_ff.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/multiply.h>
#include <gnuradio/blocks/complex_to_arg.h>
#include <gnuradio/blocks/complex_to_mag.h>

#include <sched/msg_forward.h>

#include <clenabled/GRCLBase.h>
#include <clenabled/clSComplex.h>
#include "clMathConst_impl.h"
#include "clMathOp_impl.h"
#include <clenabled/clMathOpTypes.h>
#include "clLog_impl.h"
#include "clComplexToMag_impl.h"
#include "clComplexToArg_impl.h"

extern "C"
JNIEXPORT jstring JNICALL
Java_net_bastibl_benchmark_MainActivity_runMsg(JNIEnv *env, jobject thiz,
        jstring tmpDir, int pipes, int stages, int burst_size, int pdu_size,
        int repetitions, int run) {

    static std::string ret_str;

    const char *tmp_c;
    tmp_c = env->GetStringUTFChars(tmpDir, nullptr);
    setenv("TMP", tmp_c, 1);

    setenv("VOLK_CONFIGPATH", getenv("EXTERNAL_STORAGE"), 1);

    gr::top_block_sptr  tb = gr::make_top_block("fg");

    gr::sched::msg_forward::sptr src = gr::sched::msg_forward::make();

    gr::sched::msg_forward::sptr prev;

    for(int pipe = 0; pipe < pipes; pipe++) {
        prev = gr::sched::msg_forward::make();
        tb->msg_connect(src, "out", prev, "in");

        for(int stage = 1; stage < stages; stage++) {
            gr::sched::msg_forward::sptr block = gr::sched::msg_forward::make();
            tb->msg_connect(prev, "out", block, "in");
            prev = block;
        }
    }

    std::stringstream ss;

    for(int repetition = 0; repetition < repetitions; repetition++) {

        for(int p = 0; p < burst_size; p++) {

            pmt::pmt_t msg = pmt::cons(pmt::PMT_NIL, pmt::make_u8vector((size_t)pdu_size, 0x42));
            src->post(pmt::mp("in"), msg);
        }
        pmt::pmt_t msg = pmt::cons(pmt::intern("done"), pmt::from_long(1));
        src->post(pmt::mp("system"), msg);

        auto start = std::chrono::high_resolution_clock::now();
        tb->run();
        auto finish = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count()/1e9;

        ss << boost::format("%1$4d, %2$4d, %3$4d,  %4$4d,  %5$4d,  %6$4d,  %7$20.12f") % run % pipes % stages % repetition % burst_size % pdu_size % time << std::endl;
    }

    ret_str = ss.str();

    return env->NewStringUTF(ret_str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_bastibl_benchmark_MainActivity_runBuf(JNIEnv *env, jobject thiz,
                                                jstring tmpDir, int pipes, int stages, int samples, int run) {

    static std::string ret_str;

    const char *tmp_c;
    tmp_c = env->GetStringUTFChars(tmpDir, nullptr);
    setenv("TMP", tmp_c, 1);

    setenv("VOLK_CONFIGPATH", getenv("EXTERNAL_STORAGE"), 1);

    gr::top_block_sptr  tb = gr::make_top_block("fg");

    auto src = gr::blocks::null_source::make(sizeof(float));
    auto head = gr::blocks::head::make(sizeof(float), (uint64_t)samples);

    tb->connect(src, 0, head, 0);

    std::vector<gr::blocks::copy::sptr> blocks;

    for(int pipe = 0; pipe < pipes; pipe++) {
        auto block = gr::blocks::copy::make(sizeof(float));
        tb->connect(head, 0, block, 0);
        blocks.push_back(block);

        for(int stage = 1; stage < stages; stage++) {
            block = gr::blocks::copy::make(sizeof(float));
            tb->connect(blocks.back(), 0, block, 0);
            blocks.push_back(block);
        }

        auto sink = gr::blocks::null_sink::make(sizeof(float));
        tb->connect(blocks.back(), 0, sink, 0);

    }

    nice(-200);

    auto start = std::chrono::high_resolution_clock::now();
    tb->run();
    auto finish = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count()/1e9;

    std::stringstream ss;
    ss << boost::format("%1$4d, %2$4d,  %3$4d,   %4$15d,   %5$20.15f") %
                          run % pipes % stages % samples % time << std::endl;

    ret_str = ss.str();

    return env->NewStringUTF(ret_str.c_str());
}

int opencltype=OCLTYPE_GPU;
int selectorType=OCLDEVICESELECTOR_FIRST;
int platformId=0;
int devId=0;
int d_vlen=1;
int iterations = 100;

std::string testMultiplyConst(int largeBlockSize, int repetition, int nreptitions) {

    GR_INFO("cl", "testing MultiplyConst " << largeBlockSize);
    std::stringstream ss;

    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    std::chrono::duration<double> elapsed;

    // Testing no-action kernel (return only) constant operation to measure OpenCL overhead"
    // This value represent the 'floor' on the selected platform.  Any CPU operations have to be
    // slower than this to even be worthy of OpenCL consideration unless you're just looking to offload.
    gr::clenabled::clMathConst_impl *test=NULL;
    try {
        test = new gr::clenabled::clMathConst_impl(DTYPE_COMPLEX, sizeof(SComplex), opencltype,
                selectorType, platformId, devId, 2.0, MATHOP_EMPTY, false);
    }
    catch (...) {
        GR_INFO("cl", "ERROR: error setting up OpenCL environment.");
        if (test != NULL) {
            delete test;
        }
        return "ERROR: error setting up OpenCL environment.";
    }

    test->setBufferLength(largeBlockSize);

    std::vector<int> ninitems;

    std::vector<gr_complex> inputItems;
    std::vector<gr_complex> outputItems;

    std::vector<const void *> inputPointers;
    std::vector<void*> outputPointers;

    for (int i = 0; i < largeBlockSize; i++) {
        inputItems.push_back(gr_complex((i+1.0f), 0.5f));
        outputItems.push_back(gr_complex(0.0, 0.0));
    }

    inputPointers.push_back((const void *)&inputItems[0]);
    outputPointers.push_back((void *)&outputItems[0]);

    test->set_k(0);
    test->testOpenCL(largeBlockSize, ninitems, inputPointers, outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end-start;

        ss << "empty,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    delete test;

    test = new gr::clenabled::clMathConst_impl(DTYPE_COMPLEX, sizeof(SComplex), opencltype,
            selectorType, platformId, devId, 2.0, MATHOP_EMPTY_W_COPY, false);
    test->setBufferLength(largeBlockSize);
    test->set_k(2.0);

    test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize, ninitems, inputPointers,
                                            outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "copy,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    delete test;

    test = new gr::clenabled::clMathConst_impl(DTYPE_COMPLEX, sizeof(SComplex), opencltype,
            selectorType, platformId, devId, 2.0, MATHOP_MULTIPLY, false);
    test->setBufferLength(largeBlockSize);
    test->set_k(2.0);

    test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize, ninitems, inputPointers,
                                            outputPointers);
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "multiply_const,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    delete test;

    gr::blocks::multiply_const_cc::sptr multConst = gr::blocks::multiply_const_cc::make(gr_complex(2,0), 1);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            multConst->work(largeBlockSize, inputPointers, outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "multiply_const,cpu, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    // ------------------------- LOG10 ----------------------------
    std::vector<float> inputFloats;
    std::vector<float> outputFloats;
    std::vector<const void*> inputFloatPointers;
    std::vector<void*> outputFloatPointers;

    for (int i = 0; i < largeBlockSize; i++) {
        inputFloats.push_back((i + 1.0f));
        outputFloats.push_back(0.0);
    }

    inputFloatPointers.push_back((const void *)&inputFloats[0]);
    outputFloatPointers.push_back((void *)&outputFloats[0]);

    gr::clenabled::clLog_impl *testLog = NULL;
    testLog = new gr::clenabled::clLog_impl(opencltype, selectorType, platformId,
            devId, 20.0, 0, false);

    testLog->setBufferLength(largeBlockSize);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            testLog->testOpenCL(largeBlockSize, ninitems, inputFloatPointers,
                                               outputFloatPointers);
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "log10,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    delete testLog;

    gr::blocks::nlog10_ff::sptr log10 = gr::blocks::nlog10_ff::make(1, 1, 0);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            log10->work(largeBlockSize, inputFloatPointers, outputFloatPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "log10,cpu, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }


    return ss.str();
}

std::string testMultiply(int largeBlockSize, int repetition, int nreptitions) {

    GR_INFO("cl", "testing Multiply " << largeBlockSize);

    std::stringstream ss;

    gr::clenabled::clMathOp_impl *test=NULL;
    try {
        test = new gr::clenabled::clMathOp_impl(DTYPE_COMPLEX,sizeof(SComplex),opencltype,selectorType,platformId,devId,MATHOP_MULTIPLY,false);
    }
    catch (...) {
        GR_INFO("cl", "ERROR: error setting up OpenCL environment.");

        if (test != NULL) {
            delete test;
        }

        return "error";
    }

    test->setBufferLength(largeBlockSize);

    std::vector<gr_complex> inputItems1;
    std::vector<gr_complex> inputItems2;
    std::vector<gr_complex> outputItems;
    std::vector<int> ninitems;

    std::vector< const void *> inputPointers;
    std::vector<void *> outputPointers;

    for (int i = 1; i <= largeBlockSize; i++) {
        inputItems1.push_back(gr_complex(1.0f,0.5f));
        inputItems2.push_back(gr_complex(1.0f,0.5f));
        outputItems.push_back(gr_complex(0.0,0.0));
    }

    inputPointers.push_back((const void *)&inputItems1[0]);
    inputPointers.push_back((const void *)&inputItems2[0]);
    outputPointers.push_back((void *)&outputItems[0]);

    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::chrono::duration<double> elapsed;

    test->testOpenCL(largeBlockSize,ninitems, inputPointers,outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize, ninitems, inputPointers,
                                            outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "multiply,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    delete test;

    gr::blocks::multiply_cc::sptr multiply = gr::blocks::multiply_cc::make(1);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            multiply->work(largeBlockSize,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "multiply,cpu, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }

    return ss.str();
}

std::string testComplexToArg(int largeBlockSize, int repetition, int nreptitions) {
    GR_INFO("cl", "Testing Complex to Arg " << largeBlockSize);

    std::stringstream ss;

    gr::clenabled::clComplexToArg_impl *test=NULL;
    try {
        test = new gr::clenabled::clComplexToArg_impl(opencltype,selectorType,platformId,devId,false);
    }
    catch (...) {
        GR_INFO("cl", "ERROR: error setting up OpenCL environment.");

        if (test != NULL) {
            delete test;
        }
        return "ERROR";
    }

    test->setBufferLength(largeBlockSize);

    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    std::chrono::duration<double> elapsed;

    std::vector<int> ninitems;
    std::vector<gr_complex> inputItems;
    std::vector<float> outputItems;
    std::vector<const void *> inputPointers;
    std::vector<void *> outputPointers;

    for (int i = 0; i < largeBlockSize; i++) {
        inputItems.push_back(gr_complex(1.0f,0.5f));
        outputItems.push_back(0.0);
    }

    inputPointers.push_back((const void *)&inputItems[0]);
    outputPointers.push_back((void *)&outputItems[0]);

    test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "complexToArg,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;

    }

    delete test;

    gr::blocks::complex_to_arg::sptr ctoa = gr::blocks::complex_to_arg::make(1);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            ctoa->work(largeBlockSize,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "complexToArg,cpu, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;
    }
    end = std::chrono::steady_clock::now();

    return ss.str();
}

std::string testComplexToMag(int largeBlockSize, int repetition, int nreptitions) {
    GR_INFO("cl", "Testing Complex to mag " << largeBlockSize);

    std::stringstream ss;

    gr::clenabled::clComplexToMag_impl *test=NULL;
    try {
        test = new gr::clenabled::clComplexToMag_impl(opencltype,selectorType,platformId,devId,true);
    }
    catch (...) {
        GR_INFO("cl", "ERROR: error setting up OpenCL environment.");

        if (test != NULL) {
            delete test;
        }
        return "ERROR";
    }

    test->setBufferLength(largeBlockSize);

    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    std::chrono::duration<double> elapsed;

    std::vector<int> ninitems;
    std::vector<gr_complex> inputItems;
    std::vector<float> outputItems;
    std::vector<const void *> inputPointers;
    std::vector<void *> outputPointers;

    for (int i = 0; i < largeBlockSize; i++) {
        inputItems.push_back(gr_complex(1.0f,0.5f));
        outputItems.push_back(0.0);
    }

    inputPointers.push_back((const void *)&inputItems[0]);
    outputPointers.push_back((void *)&outputItems[0]);

    // Get a test run out of the way.
    test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            test->testOpenCL(largeBlockSize,ninitems,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "complexToMag,cl, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;

    }

    delete test;

    gr::blocks::complex_to_mag::sptr ctom = gr::blocks::complex_to_mag::make(1);

    for(int rep = repetition; rep < repetition+nreptitions; rep++) {
        start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < iterations; it++) {
            ctom->work(largeBlockSize,inputPointers,outputPointers);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        ss << "complexToMag,cpu, " << largeBlockSize << ", " << rep << ", " << elapsed.count() << std::endl;

    }

    return ss.str();
}


extern "C"
JNIEXPORT jstring JNICALL
Java_net_bastibl_benchmark_MainActivity_runCl(JNIEnv *env, jobject /*this*/, jstring tmpDir) {

    cl::Context *context=NULL;
    std::vector<cl::Device> devices;

    cl_device_type contextType;

    std::string platformName="";
    std::string platformVendor="";
    std::string deviceName;
    std::string deviceType;

    std::vector<cl::Platform> platformList;

    // Pick platform
    try {
        __android_log_write(ANDROID_LOG_DEBUG, "cl", "Getting platform list.");
        cl::Platform::get(&platformList);
    }
    catch(...) {
        __android_log_write(ANDROID_LOG_DEBUG, "cl", "OpenCL Error: Unable to get platform list.");
        return env->NewStringUTF("foo");
    }

    __android_log_write(ANDROID_LOG_DEBUG, "cl", boost::str(boost::format("Found platforms: %1%") % platformList.size()).c_str());

    context = NULL;

    cl_context_properties cprops[] = {
            CL_CONTEXT_PLATFORM, (cl_context_properties)(platformList[0])(), 0};

    std::string kernelCode="";
    kernelCode += "kernel void test(__constant int * a, const int multiplier, __global int * restrict c) {\n";
    kernelCode += "return;\n";
    kernelCode += "}\n";

    std::vector<cl::Device> curDev;

    // Pick first platform
    for (int i=0;i<platformList.size();i++) {
        try {
            // Find the first platform that has devices of the type we want.
            cl_context_properties cprops[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(platformList[i])(), 0};
            platformName=platformList[i].getInfo<CL_PLATFORM_NAME>();
            platformVendor=platformList[i].getInfo<CL_PLATFORM_VENDOR>();
            context= new cl::Context(CL_DEVICE_TYPE_ALL, cprops);

            devices = context->getInfo<CL_CONTEXT_DEVICES>();

            for (int j=0;j<devices.size();j++) {
                deviceName = devices[j].getInfo<CL_DEVICE_NAME>();

                boost::trim(deviceName);

                switch (devices[j].getInfo<CL_DEVICE_TYPE>()) {
                    case CL_DEVICE_TYPE_GPU:
                        deviceType = "GPU";
                        break;
                    case CL_DEVICE_TYPE_CPU:
                        deviceType = "CPU";
                        break;
                    case CL_DEVICE_TYPE_ACCELERATOR:
                        deviceType = "Accelerator";
                        break;
                    case CL_DEVICE_TYPE_ALL:
                        deviceType = "ALL";
                        break;
                   case CL_DEVICE_TYPE_CUSTOM:
                        deviceType = "CUSTOM";
                        break;
                }

                cl_int err;
                cl_device_svm_capabilities caps;

                err = clGetDeviceInfo(devices[j](),CL_DEVICE_SVM_CAPABILITIES,
                                      sizeof(cl_device_svm_capabilities),&caps,0);
                bool hasSharedVirtualMemory = (err == CL_SUCCESS);
                bool hasSVMFineGrained = (err == CL_SUCCESS && (caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER));
                cl_int maxConstMemSize = devices[j].getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>();
                cl_int localMemSize = devices[j].getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
                cl_int memInK=(int)((float)maxConstMemSize / 1024.0);
                cl_int localMemInK=(int)((float)localMemSize / 1024.0);

                std::string Ocl2Caps="[OpenCL 2.0 Capabilities]";

                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Platform Id: %d", i);
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Device Id: %d", j);
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Platform Name: %s", platformName.c_str());
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Device Name: %s", deviceName.c_str());
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Device Type: %s", deviceType.c_str());
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Constant Memory: %dK (%d floats)", memInK, (maxConstMemSize/4));
                __android_log_print(ANDROID_LOG_DEBUG, "cl", "Local Memory: %dK (%d floats)", localMemInK, (localMemSize/4));


                // ---- now check if we support double precision
                size_t retSize;
                bool hasDoublePrecisionSupport = false;
                bool hasDoubleFMASupport = false;
                err = clGetDeviceInfo(devices[j](),CL_DEVICE_EXTENSIONS, 0, NULL, &retSize);
                if (err == CL_SUCCESS) {
                    char extensions[retSize];
                    // returns a char[] string of extension names
                    // https://www.khronos.org/registry/OpenCL/sdk/1.0/docs/man/xhtml/clGetDeviceInfo.html
                    err = clGetDeviceInfo(devices[j](),CL_DEVICE_EXTENSIONS, retSize, extensions, &retSize);

                    std::string dev_extensions(extensions);

                    if (dev_extensions.find("cl_khr_fp64") != std::string::npos) {
                        hasDoublePrecisionSupport = true;

                        // Query if we support FMA in double
                        err = clGetDeviceInfo(devices[j](),CL_DEVICE_EXTENSIONS, 0, NULL, &retSize);

                        if (err == CL_SUCCESS) {
                            cl_device_fp_config config_properties;

                            err = clGetDeviceInfo(devices[j](),CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(cl_device_fp_config),&config_properties,NULL);

                            if (config_properties & CL_FP_FMA)
                                hasDoubleFMASupport = true;
                        }

                    }
                }

                if (hasDoublePrecisionSupport) {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Double Precision Math Support: Yes");
                    if (hasDoubleFMASupport) {
                        __android_log_print(ANDROID_LOG_DEBUG, "cl", "Double Precision Fused Multiply/Add [FMA] Support: Yes");
                    }
                    else {
                        __android_log_print(ANDROID_LOG_DEBUG, "cl", "Double Precision Fused Multiply/Add [FMA] Support: No");
                    }
                }
                else {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Double Precision Math Support: No  [WARNING THIS WILL NEGATIVELY IMPACT TRIG FUNCTIONS]");
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Double Precision Fused Multiply/Add [FMA] Support: No");
                }

                // ----- now check if we support fused multiply / add
                bool hasSingleFMASupport = false;

                err = clGetDeviceInfo(devices[j](),CL_DEVICE_EXTENSIONS, 0, NULL, &retSize);

                if (err == CL_SUCCESS) {
                    cl_device_fp_config config_properties;

                    err = clGetDeviceInfo(devices[j](),CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config),&config_properties,NULL);

                    if (config_properties & CL_FP_FMA)
                        hasSingleFMASupport = true;
                }

                if (hasSingleFMASupport) {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Single Precision Fused Multiply/Add [FMA] Support: Yes");
                }
                else {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Single Precision Fused Multiply/Add [FMA] Support: No");
                }

                // OpenCL 2.0 Capabilities
                std::cout << "OpenCL 2.0 Capabilities:" << std::endl;
                if (hasSharedVirtualMemory) {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Shared Virtual Memory (SVM): Yes");
                }
                else {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Shared Virtual Memory (SVM): No");
                }

                if (hasSVMFineGrained) {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Fine-grained SVM: Yes");
                }
                else {
                    __android_log_print(ANDROID_LOG_DEBUG, "cl", "Fine-grained SVM: No");
                }
            }

        }
        catch (...) {
            __android_log_print(ANDROID_LOG_DEBUG, "cl", "OpenCL Error: Unable to get platform list.");
            return env->NewStringUTF("foo");
        }
    }

    nice(-200);
    setenv("VOLK_CONFIGPATH", getenv("EXTERNAL_STORAGE"), 1);

    static std::string ret_str;
    ret_str = "";
    int n = 5;

    for(int rep = 0; rep < 20; rep+=n) {
        for(int i = 0; i < 16; i++) {
            ret_str += testMultiply(4096 * (i + 1), rep, n);
            ret_str += testMultiplyConst(4096 * (i + 1), rep, n);
            ret_str += testComplexToMag(4096 * (i + 1), rep, n);
            ret_str += testComplexToArg(4096 * (i + 1), rep, n);
        }
    }

    return env->NewStringUTF(ret_str.c_str());
}