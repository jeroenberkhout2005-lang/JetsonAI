#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime_api.h>

using namespace nvinfer1;


//Public logger to output to console and ignore low level warnings
class Logger : public ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cout << msg << std::endl;
    }
} logger; //it just outputs messages

//Load TensorRT engine file
std::vector<char> loadEngine(const std::string& filename){
    std::ifstream file(filename, std::ios::binary); //file must be binary
    file.seekg(0,file.end); //file pointer at end of file
    size_t size = file.tellg(); //size at end of file
    file.seekg(0, file.beg); //back to start

    std::vector<char> buffer(size); 
    file.read(buffer.data(), size); //reads all data into buffer
    return buffer;
}

int main(){
    const int H = 256; //height
    const int W = 256; //width

    //timing 
    auto totalStart = std::chrono::high_resolution_clock::now();
    int frameCount = 0;

    //load engine
    auto engineData = loadEngine("model.engine");
    if(engineData.empty()){
        return -1;
    }

    IRuntime* runTime = createInferRuntime(logger);
    //^
    ICudaEngine* engine = runTime->deserializeCudaEngine(engineData.data(), engineData.size());
    //^converts raw binary to usable data
    IExecutionContext* context = engine->createExecutionContext(); 

    //TensorRT setup -Allocates GPU memory ONCE
    void* buffers[2];
    int inputIndex = engine->getBindingIndex("input");
    int outputIndex = engine->getBindingIndex("output");

    std::vector<float> input(H*W);
    std::vector<float> output(H*W);

    cudaMalloc(&buffers[inputIndex], input.size()*sizeof(float));
    cudaMalloc(&buffers[outputIndex], output.size()*sizeof(float));
    cudaSetDevice(0);

// ---------------- Video ----------------
    cv::VideoCapture cap("yt_100.mp4");

    if (!cap.isOpened()) {
        std::cerr << "Error opening video\n";
        return -1;
    }

    // FPS fallback
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;

    cv::VideoWriter writer;

    // try H264 first, fallback MJPG if needed
    int fourcc = cv::VideoWriter::fourcc('a','v','c','1');
    writer.open("output.mp4", fourcc, fps, cv::Size(W, H));

    if (!writer.isOpened()) {
        std::cout << "H264 failed, switching to MJPG\n";
        fourcc = cv::VideoWriter::fourcc('M','J','P','G');
        writer.open("output.avi", fourcc, fps, cv::Size(W, H));
    }

    if (!writer.isOpened()) {
        std::cerr << "VideoWriter failed\n";
        return -1;
    }

    cv::Mat frame; //read next frame
    

    //LOOP Start:
    while(cap.read(frame)){
        if (frame.empty()) {
            std::cerr << "Empty frame!\n";
            break;
        }   

        auto frameStart = std::chrono::high_resolution_clock::now();

        //preprocess
        cv::Mat img;
        cv::cvtColor(frame, img, cv::COLOR_BGR2GRAY);
        cv::resize(img,img,cv::Size(W,H));
        img.convertTo(img,CV_32F,1.0/255.0);

        memcpy(input.data(), img.data, input.size()*sizeof(float));
    
        //Copy input to GPU
        cudaMemcpy(buffers[inputIndex], input.data(), input.size()*sizeof(float), cudaMemcpyHostToDevice);

        //Inference
        context->enqueueV2(buffers,0,nullptr);

        //Copy back
        cudaMemcpy(output.data(), buffers[outputIndex],output.size() * sizeof(float), cudaMemcpyDeviceToHost);
        
        //Postprocess
        // cv::Mat heatmap(H, W, CV_32F, output.data());

        // // convert safely to 8-bit first
        // cv::Mat heatmap8u;
        // // cv::normalize(heatmap, heatmap8u, 0, 255, cv::NORM_MINMAX);
        // // heatmap8u.convertTo(heatmap8u, CV_8U);
        // heatmap.convertTo(heatmap8u, CV_8U, 255.0);

        // // threshold - EDIT third variable to change sensitivity
        // cv::Mat thresh;
        // cv::threshold(heatmap8u, thresh, 5, 255, cv::THRESH_BINARY);
        cv::Mat heatmap(H, W, CV_32F, output.data());

        cv::Mat heatmap8u;
        heatmap.convertTo(heatmap8u, CV_8U, 255.0);

        // remove global bias
        cv::Scalar meanVal = cv::mean(heatmap8u);
        heatmap8u -= meanVal[0];

        // enhance local features
        cv::Laplacian(heatmap8u, heatmap8u, CV_8U);

        // threshold
        cv::Mat thresh;
        cv::threshold(heatmap8u, thresh, 10, 255, cv::THRESH_BINARY);


        // ensure single channel (important safety)
        if (thresh.channels() != 1) {
            cv::cvtColor(thresh, thresh, cv::COLOR_BGR2GRAY);
        }

        // contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        cv::Mat display;
        // cv::cvtColor(img, display, cv::COLOR_GRAY2BGR); //black screen for dots only
        cv::resize(frame, display, cv::Size(W, H)); //puts mask onto frame
        
        float scaleX = (float)display.cols / W;
        float scaleY = (float)display.rows / H;

        for(const auto& contour : contours){
            cv::Moments m = cv::moments(contour);
            if(m.m00 !=0){
                int cx = int((m.m10 / m.m00));
                int cy = int((m.m01 / m.m00));
                cv::circle(display, cv::Point(cx,cy),3,cv::Scalar(0,0,255),-1);
                    
            }
        }
        cv::Mat outputFrame;
        display.convertTo(outputFrame, CV_8U);
        writer.write(outputFrame);
        cv::imwrite("heatmap_debug.jpg", heatmap8u);
        
        // timing per frame
        auto frameEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

        frameCount++;
        std::cout << "Frame " << frameCount
                  << " | " << ms << " ms"
                  << " | FPS: " << 1000.0 / ms
                  << " | Contours: " << contours.size() <<  std::endl;

    }

    // ---------------- Cleanup ----------------
    auto totalEnd = std::chrono::high_resolution_clock::now();

    double totalSec =
        std::chrono::duration<double>(totalEnd - totalStart).count();

    std::cout << "\n--- Summary ---\n";
    std::cout << "Total frames: " << frameCount << "\n";
    std::cout << "Total time: " << totalSec << " s\n";
    std::cout << "Avg FPS: " << frameCount / totalSec << "\n";

    writer.release();
    cap.release();

    cudaFree(buffers[inputIndex]);
    cudaFree(buffers[outputIndex]);

    delete context;
    delete engine;
    delete runTime;

    return 0;
}