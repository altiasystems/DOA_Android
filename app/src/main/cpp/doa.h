//
// Created by Ram Natarajan on 2020-05-12.
//

#ifndef DOA_DOA_H
#define DOA_DOA_H

#include <string>
#include <thread>
#include "SNPE/SNPE.hpp"
#include "SNPE/SNPEFactory.hpp"
#include "arm_math.h"

// Tx FFT parameters
#define NUM_CHANNELS 8
#define FRAME_SIZE 128
#define FRAME_PR_SEC (16000 / FRAME_SIZE)
#define FFT_LEN 128
#define RX_CFFT_BUF_SIZE (FFT_LEN*2 * FRAME_PR_SEC)

class DOA {
public:
    DOA(std::string dlc_, std::string inputFile_, std::string outputDir_,
            zdl::DlSystem::Runtime_t runtime_);

    int startDOA();

    void stopDOA();

private:
    void TxProcessing();

    void RxProcessing();

    void DoaProcessing(std::unique_ptr <zdl::SNPE::SNPE> snpe,
            std::unique_ptr <zdl::DlSystem::ITensor> inputTensor);

private:
    bool TxThreadRunning;
    bool RxThreadRunning;
    bool DoaThreadRunning;
    zdl::DlSystem::Runtime_t runtime;
    std::string dlc;
    std::string outputDir;
    std::string inputFile;
    std::thread RxThread;
    std::thread TxThread;
    std::thread DoaThread;
    arm_cfft_instance_f32 varInstCfftF32[NUM_CHANNELS];
    float RX_CFFT_OUT[RX_CFFT_BUF_SIZE][NUM_CHANNELS];
    bool RxEnergyLow;
};
#endif //DOA_DOA_H
