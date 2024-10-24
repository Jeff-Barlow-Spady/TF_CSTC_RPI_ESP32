#pragma GCC diagnostic push
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <fstream>
#include <iostream>
#include <cmath>
#include <regex>
#include <experimental/filesystem>
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow/lite/model.h"
#include "BYTETracker.h"

#include <string>
#include <vector>
#include <type_traits>
#include <set>

#include <filesystem>


namespace fs = std::filesystem;


using namespace cv;
using namespace std;

const size_t width = 300;
const size_t height = 300;

std::vector<std::string> Labels;
std::unique_ptr<tflite::Interpreter> interpreter;

static bool getFileContent(std::string fileName)
{

	// Open the File
	std::ifstream in(fileName.c_str());
	// Check if object is valid
	if(!in.is_open()) return false;

	std::string str;
	// Read the next line from File untill it reaches the end.
	while (std::getline(in, str))
	{
		// Line contains string of length > 0 then save it in vector
		if(str.size()>0) Labels.push_back(str);
	}
	// Close The File
	in.close();
	return true;
}

int dummy_error_handler(int status
    , char const* func_name
    , char const* err_msg
    , char const* file_name
    , int line
    , void* userdata)
{
    //Do nothing -- will suppress console output
    return 0;   //Return value is not used
}

void set_dummy_error_handler()
{
    cv::redirectError(dummy_error_handler);
}

void detect_from_video(Mat &src,BYTETracker &track)
{
    Mat image;
    int cam_width =src.cols;
    int cam_height=src.rows;

    // copy image to input as input tensor
    //cv::resize(src, image, Size(448,448));
    cv::resize(src, image, Size(300,300));

    memcpy(interpreter->typed_input_tensor<uchar>(0), image.data, image.total() * image.elemSize());

    interpreter->SetAllowFp16PrecisionForFp32(true);
    interpreter->SetNumThreads(4);      //quad core

    interpreter->Invoke();      // run your model

    const float* detection_locations = interpreter->tensor(interpreter->outputs()[0])->data.f;
    const float* detection_classes=interpreter->tensor(interpreter->outputs()[1])->data.f;
    const float* detection_scores = interpreter->tensor(interpreter->outputs()[2])->data.f;
    const int    num_detections = *interpreter->tensor(interpreter->outputs()[3])->data.f;

    std::vector<Object> objects;
    const float confidence_threshold = 0.4;
    for(int i = 0; i < num_detections; i++){
        if(detection_scores[i] > confidence_threshold){
            Object Obj;
            Obj.label=(int)detection_classes[i]+1;
            Obj.prob =detection_scores[i];
            float y1=detection_locations[4*i  ]*cam_height;
            float x1=detection_locations[4*i+1]*cam_width;
            float y2=detection_locations[4*i+2]*cam_height;
            float x2=detection_locations[4*i+3]*cam_width;

            Obj.rect.x     = (int)x1;
            Obj.rect.y     = (int)y1;
            Obj.rect.width = (int)(x2 - x1);
            Obj.rect.height= (int)(y2 - y1);

            objects.push_back(Obj);
        }
    }

    vector<STrack> output_stracks = track.update(objects);

    for(size_t i = 0; i < output_stracks.size(); i++){
        vector<float> tlwh = output_stracks[i].tlwh;
       // bool vertical = tlwh[2] / tlwh[3] > 1.6;
        //if (tlwh[2] * tlwh[3] > 20 && !vertical){
            Scalar s = track.get_color(output_stracks[i].track_id);
            putText(src, format("%d", output_stracks[i].track_id), Point(tlwh[0], tlwh[1] - 5),
                    0, 0.6, Scalar(0, 0, 255), 2, LINE_AA);
            rectangle(src, Rect(tlwh[0], tlwh[1], tlwh[2], tlwh[3]), s, 2);
        //}
    }

}

int main(int argc,char ** argv)
{
    float f;
    float FPS[16];
    int i, Fcnt=0;
    Mat frame;
    chrono::steady_clock::time_point Tbegin, Tend;

    for(i=0;i<16;i++) FPS[i]=0.0;

    // Load model
    std::unique_ptr<tflite::FlatBufferModel> model = tflite::FlatBufferModel::BuildFromFile("detect.tflite");
    //std::unique_ptr<tflite::FlatBufferModel> model = tflite::FlatBufferModel::BuildFromFile("pednet_20200517.tflite");

    // Build the interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder(*model.get(), resolver)(&interpreter);

    interpreter->AllocateTensors();

	// Get the names
	bool result = getFileContent("COCO_labels.txt");
    //bool result = getFileContent("pednet_labels.txt");

	if(!result)
	{
        cout << "loading labels failed";
        exit(-1);
	}


    //int fps = cap.get(CAP_PROP_FPS);
    int fps = 10;
    BYTETracker tracker(fps, 30);
    set_dummy_error_handler();

    //cout << "Start grabbing, press ESC on Live window to terminate" << endl;
    try{

	while(1){

        std::string pathToData("../../current_imgs/file.jpeg"); //file_%05d.jpeg // UNCOMMENT
        //std::string pathToData("file_%05d.jpeg"); //file_%05d.jpeg

        cv::VideoCapture cap(pathToData);
        //cout<<std::filesystem::remove("/home/capstone/Downloads/TensorFlow_Lite-Tracking-RPi_64-bit-mainIntegrate/test/file.jpeg")<<endl;
        //VideoCapture cap("highway_01.mp4");
        if (!cap.isOpened()) {
            //cerr << "File aint there?" << endl;
            //return 0;
        }
        int iAhmed = 0;
        while(1) {
            //cout<<"huh"<<iAhmed<<endl;
            iAhmed++;
            cap >> frame;
            cap.release(); //UNCOMMENT
            if (!cap.isOpened()) {
               // cout << "File aint there2?" << endl;
            //return 0;
            }
            cout<<std::filesystem::remove("../../current_imgs/file.jpeg")<<endl;//UNCOMMENT
            if (frame.empty()) {
               // cout << "HELLOOOOOOOOOOOOOOOOOOODIS" << endl;
                break;
            }

            Tbegin = chrono::steady_clock::now();

            detect_from_video(frame, tracker);

            Tend = chrono::steady_clock::now();
            //calculate frame rate
            f = chrono::duration_cast <chrono::milliseconds> (Tend - Tbegin).count();
            if(f>0.0) FPS[((Fcnt++)&0x0F)]=1000.0/f;
            for(f=0.0, i=0;i<16;i++){ f+=FPS[i]; }
            putText(frame, format("FPS %0.2f", f/16),Point(10,20),FONT_HERSHEY_SIMPLEX,0.6, Scalar(0, 0, 255));

            //show output
            imshow("RPi 4 - 1,9 GHz - 2 Mb RAM", frame);
            char esc = waitKey(5);
            if(esc == 27) break;


        }


    }
    } catch(int e) {
    }

  cout << "Closing the camera" << endl;
  destroyAllWindows();
  cout << "Bye!" << endl;

  return 0;
}
