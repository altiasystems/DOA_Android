///////////
//
//  Draft app that will run DOA CNN through QCOMM SNPE API
//  
//  TO DO:
//  - Implement Rx power calculation function (VAD)
//  - Replace network input with UserBuffer instead of ITensor
//  - Retrieve Tx data directily through aDSP shared memory OR through QCOMM AudioHAL
//  - Investigate opportunity for computing Tx FFT in aDSP
//
///////////

#include "doa.h"

// C++ libs
#include <iostream>
#include <getopt.h>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <string>
#include <iterator>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <unistd.h>
#include <chrono>

// SNPE libs
#include "CheckRuntime.hpp"
#include "LoadContainer.hpp"
#include "GNSetBuilderOptions.hpp"
#include "LoadInputTensor.hpp"
#include "SaveOutputTensor.hpp"
#include "Util.hpp"
#include "SNPE/SNPE.hpp"
#include "DiagLog/IDiagLog.hpp"

// CMSIS libs
#include "arm_const_structs.h"


DOA::DOA(std::string dlc_, std::string inputFile_, std::string outputDir_, zdl::DlSystem::Runtime_t runtime_) {

    for (int i = 0; i < NUM_CHANNELS; i++) {
        arm_cfft_init_f32(&varInstCfftF32[i], FFT_LEN);
    }
    TxThreadRunning = false;
    RxThreadRunning = false;
    DoaThreadRunning = false;
    runtime = runtime_;
    //runtime = checkRuntime(runtime);
    dlc = dlc_;
    outputDir = outputDir_;
    inputFile = inputFile_;
    RxEnergyLow = false;
}

int DOA::startDOA() {

    RxThreadRunning = true;
    TxThreadRunning = true;

    RxThread = std::thread(&DOA::RxProcessing, this);
    TxThread = std::thread(&DOA::TxProcessing, this);

    //  Open DLC
    std::cout << "Trying to load dlc: " << dlc << std::endl;
    std::ifstream dlcFile(dlc);
    if (!dlcFile) {
        std::cerr << "Input dlc file not valid." << std::endl;
        return -1;
    }
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(dlc);
    if (container == nullptr) {
        std::cerr << "Error while opening the container file." << std::endl;
        return -1;
    }

    //  BUILD SNPE OBJECT
    bool useUserSuppliedBuffers = false;
    bool usingInitCaching = false;

    zdl::DlSystem::PlatformConfig platformConfig;
    std::unique_ptr<zdl::SNPE::SNPE> snpe;

    std::cout << "Building SNPE runtime object" << std::endl;
    snpe = GNsetBuilderOptions(container, runtime, useUserSuppliedBuffers, platformConfig,
            usingInitCaching);
    if (snpe == nullptr) {
        std::cerr << "Error while building SNPE object." << std::endl;
        return -1;
    }

    if (usingInitCaching) {
        if (container->save(dlc)) {
            std::cout << "Saved container into archive successfully" << std::endl;
        } else {
            std::cout << "Failed to save container into archive" << std::endl;
        }
    }

    std::ifstream inputList(inputFile);
    if (!inputList) {
        std::cerr
                << "Input list not valid. Please ensure that you have provided a valid input list and dlc for processing"
                << std::endl;
        return -1;
    }

    // Configure logging output and start logging. The snpe-diagview
    // executable can be used to read the content of this diagnostics file
    auto logger_opt = snpe->getDiagLogInterface();
    if (!logger_opt) {
        std::cerr << "SNPE failed to obtain logging interface" << std::endl;
        return -1;
    }
    auto logger = *logger_opt;
    auto opts = logger->getOptions();
    opts.LogFileDirectory = outputDir;
    if (!logger->setOptions(opts)) {
        std::cerr << "Failed to set options" << std::endl;
        return -1;
    }
    if (!logger->start()) {
        std::cerr << "Failed to start logger" << std::endl;
        return -1;
    }

    std::cout << "Loading DNN Input: " << inputFile << "\n";
    std::vector<float> inputVec = loadFloatDataFile(inputFile);

    const auto &strList_opt = snpe->getInputTensorNames();
    if (!strList_opt) throw std::runtime_error("Error obtaining Input tensor names");
    const auto &strList = *strList_opt;
    // Make sure the network requires only a single input
    //assert (strList.size() == 1);

    /* Create an input tensor that is correctly sized to hold the input of the network. Dimensions that have no fixed size will be represented with a value of 0. */
    const auto &inputDims_opt = snpe->getInputDimensions(strList.at(0));
    const auto &inputShape = *inputDims_opt;

    /* Calculate the total number of elements that can be stored in the tensor so that we can check that the input contains the expected number of elements.
       With the input dimensions computed create a tensor to convey the input into the network. */
    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = zdl::SNPE::SNPEFactory::getTensorFactory().createTensor(
            inputShape);
    //Padding the input vector so as to make the size of the vector to equal to an integer multiple of the batch size
    //zdl::DlSystem::TensorShape tensorShape;
    //tensorShape = snpe->getInputDimensions();
    //size_t batchSize = tensorShape.getDimensions()[0];

    if (inputTensor->getSize() != inputVec.size()) {
        std::cerr << "Size of input does not match network.\n"
                << "Expecting: " << inputTensor->getSize() << "\n"
                << "Got: " << inputVec.size() << "\n";
        return EXIT_FAILURE;
    }

    /* Copy the loaded input file contents into the networks input tensor. SNPE's ITensor supports C++ STL functions like std::copy() */
    std::copy(inputVec.begin(), inputVec.end(), inputTensor->begin());

    DoaThreadRunning = true;

    // Relinquish ownership of snpe and input tensor to thread
    DoaThread = std::thread(&DOA::DoaProcessing, this, std::move(snpe), std::move(inputTensor));

    return 0;

}

void DOA::stopDOA() {

    // Allow threads to exit
    TxThreadRunning = false;
    RxThreadRunning = false;
    DoaThreadRunning = false;

    RxThread.join();
    std::cout << "Rx thread joined" << std::endl;
    TxThread.join();
    std::cout << "Tx thread joined" << std::endl;
    DoaThread.join();
    std::cout << "DOA thread joined" << std::endl;

    return;
}

// Thread for calculating FFT on Tx signals
void DOA::TxProcessing()
{
    int WrPtr = 0;
    printf("TX Thread started\n");
    while(TxThreadRunning)
    {
        // Compute FFT pr channel
        for(int i = 0; i<NUM_CHANNELS; i++)
        {
            arm_cfft_f32(&varInstCfftF32[i],&RX_CFFT_OUT[WrPtr][i], 1,1);
        }
        // Increment pointer
        WrPtr += FFT_LEN*2;
        if(WrPtr >= (RX_CFFT_BUF_SIZE - FFT_LEN*2))
        {
            WrPtr = 0;
        }

        usleep(4000); // 4 ms
    }
    printf("TX Thread ending\n");
}

// Thread for calculating Rx power to determine if DOA should execute or not
void DOA::RxProcessing()
{
    printf("RX Thread started\n");
    while(RxThreadRunning)
    {
        RxEnergyLow = true;

        /*
        // Compute Rx energy here - negate flag for now to simulate activity
        RxEnergyLow = !RxEnergyLow;
        if(RxEnergyLow)
        std::cout << "RxEnergyLow is TRUE" << std::endl;
        else
        {
            std::cout << "RxEnergyLow is FALSE" << std::endl;
        }
        */

        usleep(1000000); // 1000 ms
    }
    RxEnergyLow = false;
    printf("RX Thread ending\n");
}

// Thread for executing and saving DOA
void DOA::DoaProcessing(std::unique_ptr<zdl::SNPE::SNPE> snpe, std::unique_ptr<zdl::DlSystem::ITensor> inputTensor)
{
    printf("DOA Thread started\n");
    zdl::DlSystem::TensorMap outputTensorMap; // A tensor map for SNPE execution outputs

    while(DoaThreadRunning)
    {
        if(snpe != NULL && inputTensor != NULL)
        {
            while(RxEnergyLow)
            {
                auto start = std::chrono::high_resolution_clock::now();

                bool execStatus = snpe->execute(inputTensor.get(), outputTensorMap);
                // Save the execution results if execution successful
                if (execStatus == true)
                {
                    if(!saveOutput(outputTensorMap, outputDir, 0, 1))
                    {
                        std::cerr << "Error while saving the output" << std::endl;
                    }

                    auto stop = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
                    std::string time = std::to_string((float)duration.count() / 1000);
                    std::cout << "DOA Execution done after:" << time << " seconds" << std::endl;
                }
                else
                {
                    std::cerr << "Error while executing the network." << std::endl;
                }
            }
        }

    }
    // Freeing of snpe object
    snpe.reset();
    printf("DOA Thread ending\n");
}


