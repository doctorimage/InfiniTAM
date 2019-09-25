// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include <xtensor/xnpy.hpp>
#include "MyEngine.h"
#include <gl.h>


#include <cstring>
#include <xtensor/xarray.hpp>
#include <glm/gtc/type_ptr.hpp>


#ifdef __APPLE__
#include <GLUT/glut.h>
#else

#include <GL/glut.h>

#endif

#ifdef FREEGLUT

#include <GL/freeglut.h>

#else
#if (!defined USING_CMAKE) && (defined _MSC_VER)
#pragma comment(lib, "glut64")
#endif
#endif

#include <json/json.h>
#include "../../ITMLib/ITMLibDefines.h"
#include "../../ITMLib/Core/ITMBasicEngine.h"
#include "../../ITMLib/Core/ITMBasicSurfelEngine.h"
#include "../../ITMLib/Core/ITMMultiEngine.h"

#include "../../ORUtils/FileUtils.h"
#include "../../InputSource/FFMPEGWriter.h"

using namespace InfiniTAM::Engine;
using namespace InputSource;
using namespace ITMLib;

MyEngine *MyEngine::instance;

static void safe_glutBitmapString(void *font, const char *str) {
    size_t len = strlen(str);
    for (size_t x = 0; x < len; ++x) {
        glutBitmapCharacter(font, str[x]);
    }
}

static void initSH(float *sh) {
    sh[0] = 1.0 / sqrt(4 * M_PI);
    sh[1] = (2.0 * M_PI / 3.0) * sqrt(3.0 / (4 * M_PI));
    sh[2] = (2.0 * M_PI / 3.0) * sqrt(3.0 / (4 * M_PI));
    sh[3] = (2.0 * M_PI / 3.0) * sqrt(3.0 / (4 * M_PI));;
    sh[4] = M_PI / 8.0 * sqrt(5.0 / (4 * M_PI));
    sh[5] = (3.0 * M_PI / 4.0) * sqrt(5 / (12 * M_PI));
    sh[6] = (3.0 * M_PI / 4.0) * sqrt(5 / (12 * M_PI));
    sh[7] = (3.0 * M_PI / 4.0) * sqrt(5 / (12 * M_PI));
    sh[8] = (3.0 * M_PI / 8.0) * sqrt(5 / (12 * M_PI));
}

void MyEngine::glutDisplayFunction() {
    MyEngine *MyEngine = MyEngine::Instance();

    // do the actual drawing

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);

    bool shouldRender[NUM_WIN] = {};
    shouldRender[RGBIn] = true;
    if (MyEngine->currentColourMode == SH_SHADED) {
        shouldRender[SHOut] = true;
    } else if (MyEngine->currentColourMode == UV_REFERENCE) {
        shouldRender[RefOut] = true;
    }

    glViewport(0.0f, 0.0f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
        glLoadIdentity();
        glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        {
            glEnable(GL_TEXTURE_2D);
            for (int w = 0; w < NUM_WIN; w++) {// Draw each sub window
                if (shouldRender[w]) {
                    glBindTexture(GL_TEXTURE_2D, MyEngine->textureId[w]);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glBegin(GL_QUADS);
                    {
                        glTexCoord2f(0, 1);
                        glVertex2f(0, 0);
                        glTexCoord2f(1, 1);
                        glVertex2f(1, 0);
                        glTexCoord2f(1, 0);
                        glVertex2f(1, 1);
                        glTexCoord2f(0, 0);
                        glVertex2f(0, 1);
                    }
                    glEnd();
                }
            }
            glDisable(GL_TEXTURE_2D);
        }
        glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glRasterPos2f(0.85f, -0.962f);
    char str[200];
    sprintf(str, "%04.2lf", MyEngine->processedTime);
    safe_glutBitmapString(GLUT_BITMAP_HELVETICA_18, (const char *) str);


    glColor3f(1.0f, 0.0f, 0.0f);
    glRasterPos2f(-0.98f, -0.95f);
    safe_glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const char *) str);

    glutSwapBuffers();
    MyEngine->needsRefresh = false;
}

void MyEngine::glutIdleFunction() {
    MyEngine *MyEngine = MyEngine::Instance();

    if (MyEngine->mainLoopAction == MyEngine::PROCESS_VIDEO) {
        MyEngine->ProcessFrame();
        MyEngine->processedFrameNo++;
        MyEngine->needsRefresh = true;
    }

    if (MyEngine->needsRefresh) {
        glutPostRedisplay();
    }
}

void MyEngine::glutKeyUpFunction(unsigned char key, int x, int y) {
    MyEngine *MyEngine = MyEngine::Instance();

    switch (key) {
        case 'c':
            MyEngine->currentColourMode++;
            if (MyEngine->currentColourMode == MyEngine->numColorMode) {
                MyEngine->currentColourMode = 0;
            }
            MyEngine->needsRefresh = true;
            break;
        case 'b':
            if (MyEngine->mainLoopAction == MyEngine::PROCESS_PAUSED) {
                MyEngine->mainLoopAction = MyEngine::PROCESS_VIDEO;
            } else if (MyEngine->mainLoopAction == MyEngine::PROCESS_VIDEO) {
                MyEngine->mainLoopAction = MyEngine::PROCESS_PAUSED;
            }
        default:
            break;
    }
}

void MyEngine::Initialise(int &argc, char **argv, ImageSourceEngine *imageSource, const char *outFolder,
                          const std::string &referenceTexturePath, const std::string &shParamsPath,
                          const std::string &cameraJsonPath, const std::string &meshPath) {

    this->mainLoopAction = MyEngine::PROCESS_PAUSED;

    {
        this->refImage = new ORUtils::Image<ORUtils::Vector4<unsigned char> >(true, false);
        bool ok = ReadImageFromFile(this->refImage, referenceTexturePath.c_str());
        if (!ok) {
            std::cerr << "load reference texture failed: " << referenceTexturePath << std::endl;
            return;
        }
    }

    {
        initSH(this->shUniform);
        std::ifstream fin(shParamsPath);
        for (int i = 0; i < 9; i++) {
            float x;
            fin >> x;
            shUniform[i] *= x;
        }
    }

    {
        std::ifstream fin(cameraJsonPath);
        Json::Value root;
        fin >> root;
        rgbIntrinsics.imgSize.width = root["width"].asInt();
        rgbIntrinsics.imgSize.height = root["height"].asInt();
        rgbIntrinsics.projectionParamsSimple.fx = root["fx"].asFloat();
        rgbIntrinsics.projectionParamsSimple.fy = root["fy"].asFloat();
        rgbIntrinsics.projectionParamsSimple.px = root["px"].asFloat();
        rgbIntrinsics.projectionParamsSimple.py = root["py"].asFloat();
        pMat = getProjectionMatrix(rgbIntrinsics);
    }

    this->currentColourMode = 0;
    this->imageSource = imageSource;

    {
        size_t len = strlen(outFolder);
        this->outFolder = new char[len + 1];
        strcpy(this->outFolder, outFolder);
    }

    winSize.x = imageSource->getRGBImageSize().x;
    winSize.y = imageSource->getRGBImageSize().y;

    this->currentFrameNo = 0;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(winSize.x, winSize.y);
    glutCreateWindow("UVRender");

    glewInit();

    glGenTextures(NUM_WIN, textureId);

    {
        glBindTexture(GL_TEXTURE_2D, textureId[UVIn]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, refImage->noDims.x, refImage->noDims.y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     refImage->GetData(MEMORYDEVICE_CPU));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
    for (auto id:{UVOut, SHOut, RefOut}) {
        glBindTexture(GL_TEXTURE_2D, textureId[id]);
        unsigned internalFormat, type;
        if (id == UVOut) {
            internalFormat = GL_RG32F;
            type = GL_RG;
        } else {
            internalFormat = GL_RGBA32F;
            type = GL_RGBA;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, imageSource->getRGBImageSize().width,
                     imageSource->getRGBImageSize().height, 0, type, GL_FLOAT, nullptr);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + id, GL_TEXTURE_2D, textureId[id], 0);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

    glutDisplayFunc(MyEngine::glutDisplayFunction);
    glutKeyboardUpFunc(MyEngine::glutKeyUpFunction);
    glutIdleFunc(MyEngine::glutIdleFunction);


    {
        this->shader = std::make_unique<Shader>("./shaders/scene_vertex_shader.glsl",
                                                "./shaders/scene_fragment_shader.glsl");
        shader->use();
        shader->setFloatArray("sh", shUniform, 9);
        refTexIndex = 0;
        shader->setInt("corresTexture", refTexIndex);
    }

    glModel = std::make_unique<GLModel>(meshPath);

    inputRGBImage = new ITMUChar4Image(imageSource->getRGBImageSize(), true, false);
    inputRawDepthImage = new ITMShortImage(imageSource->getDepthImageSize(), true, false);

    needsRefresh = false;
    processedFrameNo = 0;
    processedTime = 0.0f;

    sdkCreateTimer(&timer_instant);
    sdkCreateTimer(&timer_average);

    sdkResetTimer(&timer_average);

    printf("initialised.\n");
}

void MyEngine::ProcessFrame() {
    if (!imageSource->hasMoreImages()) return;
    imageSource->getImages(inputRGBImage, inputRawDepthImage);

    sdkResetTimer(&timer_instant);
    sdkStartTimer(&timer_instant);
    sdkStartTimer(&timer_average);

    char str[120];
    sprintf(str, "%s/pose_%04d.npy", outFolder, currentFrameNo);
    xt::xtensor_fixed<float, xt::xshape<4, 4>, xt::layout_type::column_major> curPose = xt::load_npy<float>(str);
    auto mvMat = glm::make_mat4(curPose.data());
    auto mvpMat = pMat * mvMat;
    auto normalMat = glm::transpose(glm::inverse(mvMat));

    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0 + RefOut, GL_COLOR_ATTACHMENT0 + SHOut,
                        GL_COLOR_ATTACHMENT0 + UVOut};
    glDrawBuffers(3, buffers);

    glViewport(0, 0, winSize.width, winSize.height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->use();
    shader->setMat4("mvpMat", mvpMat);
    shader->setMat4("normalMat", normalMat);

    glActiveTexture(GL_TEXTURE0 + refTexIndex);
    glBindTexture(GL_TEXTURE_2D, textureId[UVIn]);
    glModel->draw();

    glDisable(GL_DEPTH_TEST);

    xt::xtensor<float, 3> uvImage({static_cast<unsigned long>(winSize.width),
                                   static_cast<unsigned long>(winSize.height), 2});
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId[UVOut]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, uvImage.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    sprintf(str, "%s/uv_%04d.npy", outFolder, currentFrameNo);
    xt::dump_npy(str, uvImage);


    {
        glBindTexture(GL_TEXTURE_2D, textureId[RGBIn]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, inputRGBImage->noDims.x, inputRGBImage->noDims.y, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, inputRGBImage->GetData(MEMORYDEVICE_CPU));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    sdkStopTimer(&timer_instant);
    sdkStopTimer(&timer_average);

    //processedTime = sdkGetTimerValue(&timer_instant);
    processedTime = sdkGetAverageTimerValue(&timer_average);

    currentFrameNo++;
}

void MyEngine::Run() { glutMainLoop(); }

void MyEngine::Shutdown() {
    sdkDeleteTimer(&timer_instant);
    sdkDeleteTimer(&timer_average);

    delete inputRGBImage;
    delete inputRawDepthImage;

    delete[] outFolder;
    delete refImage;
    delete instance;
    instance = nullptr;
}

glm::mat4 MyEngine::getProjectionMatrix(ITMLib::ITMIntrinsics &intrinsics) {
    auto fx = intrinsics.projectionParamsSimple.fx;
    auto fy = intrinsics.projectionParamsSimple.fy;
    auto px = intrinsics.projectionParamsSimple.px;
    auto py = intrinsics.projectionParamsSimple.py;
    auto width = intrinsics.imgSize.width;
    auto height = intrinsics.imgSize.height;

    auto P = glm::mat4(0.0);

    P[0][0] = 2.0f * fx / width;
    P[1][1] = 2.0f * fy / width;
    P[2][0] = 1.0f - 2.0f * px / (width - 1.0f);
    P[2][1] = 2.0f * py / (height - 1.0f) - 1.0f;
    P[2][3] = -1.0f;

    auto n = 0.05; // 5cm
    P[2][2] = -1.0f;
    P[3][2] = -2.0f * n;

    return P;
}


