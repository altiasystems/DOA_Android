package com.jabra.doa;

import android.app.Application;
import android.util.Log;

import com.qualcomm.qti.snpe.FloatTensor;
import com.qualcomm.qti.snpe.NeuralNetwork;
import com.qualcomm.qti.snpe.SNPE;
//import com.qualcomm.qti.snpe.NeuralNetwork.Runtime;
import com.qualcomm.qti.snpe.Tensor;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class DOAInference {
    private static final String LOG_TAG = DOAInference.class.getSimpleName();

    private NeuralNetwork network;
    private String inputLayer;
    private String cacheDir;

    DOAInference(final Application application, String cacheDir_, String modelFileName) {
        try {
            cacheDir = cacheDir_;
            final SNPE.NeuralNetworkBuilder builder = new SNPE.NeuralNetworkBuilder(application)
                    // Allows selecting a runtime order for the network.
                    // In the example below use DSP and fall back, in order, to GPU then CPU
                    // depending on whether any of the runtimes is available.
                    .setRuntimeOrder(com.qualcomm.qti.snpe.NeuralNetwork.Runtime.GPU,
                            com.qualcomm.qti.snpe.NeuralNetwork.Runtime.DSP,
                            com.qualcomm.qti.snpe.NeuralNetwork.Runtime.CPU)
                    // Loads a model from DLC file
                    .setModel(new File(cacheDir, modelFileName));

            network = builder.build();

            Set<String> inputNames = network.getInputTensorsNames();
            if (inputNames.size() != 1) {
                throw new IllegalStateException("Invalid network input tensor.");
            }

            inputLayer = inputNames.iterator().next();

        } catch (IllegalStateException | IOException e) {
            Log.e(LOG_TAG, e.getMessage(), e);
        }
    }

    void runInference(String inputFileName) {
        try {
            final FloatTensor tensor = network.createFloatTensor(
                    network.getInputTensorsShapes().get(inputLayer));

            float[] input = loadInput(cacheDir, inputFileName);
            if (input == null) {
                throw new IOException("Cannot open input file");
            }
            tensor.write(input, 0, input.length);

            final Map<String, FloatTensor> inputs = new HashMap<>();
            inputs.put(inputLayer, tensor);

            final Map<String, FloatTensor> outputs = network.execute(inputs);

            for (Map.Entry<String, FloatTensor> output : outputs.entrySet()) {
                FloatTensor outputTensor = output.getValue();

                final float[] array = new float[outputTensor.getSize()];
                outputTensor.read(array, 0, array.length);
            }

            releaseTensors(inputs, outputs);
        } catch (IllegalStateException | IOException e) {
            Log.e(LOG_TAG, e.getMessage(), e);
        }
    }

    float[] loadInput(String cacheDir, String inputFileName) {
        try {
            File f = new File(cacheDir, inputFileName);
            Log.d(LOG_TAG, "inputFile "+inputFileName+" length "+f.length());
            float[] arr = new float[(int)f.length()/4];
            DataInputStream in = new DataInputStream(new FileInputStream(f));
            int k = 0;
            while (in.available() > 0) {
                arr[k++] = in.readFloat();
            }
            return arr;
        } catch (IOException ex) {
            Log.e(LOG_TAG, ex.getMessage(), ex);
            return null;
        }
    }

    @SafeVarargs
    private final void releaseTensors(Map<String, ? extends Tensor>... tensorMaps) {
        for (Map<String, ? extends Tensor> tensorMap: tensorMaps) {
            for (Tensor tensor: tensorMap.values()) {
                tensor.release();
            }
        }
    }
}
