// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include <vector>
#include <memory>
#include <xtensor/xarray.hpp>

#include <glad/glad.h>
#include "../../InputSource/ImageSourceEngine.h"
#include "../../InputSource/IMUSourceEngine.h"
#include "../../InputSource/FFMPEGWriter.h"
#include "../../ITMLib/Core/ITMMainEngine.h"
#include "../../ITMLib/Utils/ITMLibSettings.h"
#include "../../ORUtils/FileUtils.h"
#include "../../ORUtils/NVTimer.h"
#include "shader.h"
#include "GLModel.h"


namespace InfiniTAM {
    namespace Engine {
        class MyEngine {

            static MyEngine *instance;

            enum MainLoopAction {
                PROCESS_PAUSED, PROCESS_VIDEO
            } mainLoopAction;

            enum UIColourMode {
                SH_SHADED, UV_REFERENCE
            };
            int currentColourMode;
            const int numColorMode = 2;

            InputSource::ImageSourceEngine *imageSource;

            StopWatchInterface *timer_instant;
            StopWatchInterface *timer_average;

        private: // For UI layout
            static const int NUM_WIN = 5; //
            enum TextureIndex {
                UVIn, RGBIn, UVOut, SHOut, RefOut
            };
            Vector2i winSize;
            uint textureId[NUM_WIN];

            ITMUChar4Image *inputRGBImage;
            ITMShortImage *inputRawDepthImage;

            ITMLib::ITMIntrinsics rgbIntrinsics;
            glm::mat4 pMat;

            ORUtils::Image<ORUtils::Vector4<unsigned char> > *refImage;
            int refTexIndex;
            xt::xarray<float> normRefImage;

            float shUniform[9];

            int currentFrameNo;

            std::unique_ptr<Shader> shader;

            std::unique_ptr<GLModel> glModel;
        public:
            static MyEngine *Instance(void) {
                if (instance == NULL) instance = new MyEngine();
                return instance;
            }

            static void glutDisplayFunction();

            static void glutIdleFunction();

            static void glutKeyUpFunction(unsigned char key, int x, int y);

            float processedTime;
            int processedFrameNo;
            char *outFolder;
            bool needsRefresh;

            unsigned fbo;

            void Initialise(int &argc, char **argv, InputSource::ImageSourceEngine *imageSource, const char *outFolder,
                            const std::string &referenceTexturePath, const std::string &shParamsPath,
                            const std::string &cameraJsonPath, const std::string &meshPath);

            void Shutdown();

            static void Run();

            void ProcessFrame();

            static glm::mat4 getProjectionMatrix(ITMLib::ITMIntrinsics &intrinsics);
        };
    }
}
