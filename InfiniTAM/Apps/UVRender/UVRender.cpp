//
// Created by gerw on 9/24/19.
//

#include "MyEngine.h"
#include <memory>

using InfiniTAM::Engine::MyEngine;
using namespace InputSource;

int main(int argc, char *argv[]) {
    // load reference texture
    ImageMaskPathGenerator pathGenerator("result/%04u.ppm", "result/%04u.pgm");
    auto imageSource = std::make_unique<ImageFileReader<ImageMaskPathGenerator>>("", pathGenerator, 0);
    MyEngine::Instance()->Initialise(argc, argv, imageSource.get(), "./result", "./resource/bremm.png",
                                     "./resource/light_render.txt", "./result/camera_intrinsic_color.json",
                                     "./result/mesh.obj");
    MyEngine::Run();
    MyEngine::Instance()->Shutdown();
    return 0;
}

