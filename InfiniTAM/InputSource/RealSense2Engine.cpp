// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "RealSense2Engine.h"

#include "../ORUtils/FileUtils.h"

#include <cstdio>
#include <stdexcept>

#include <iostream>
#include <iomanip>
#include <cstring>

#ifdef COMPILE_WITH_RealSense2
#include "librealsense2/rs.hpp"

using namespace InputSource;
using namespace ITMLib;

static void print_device_information(const rs2::device& dev)
{
	// Each device provides some information on itself
	// The different types of available information are represented using the "RS2_CAMERA_INFO_*" enum
	
	std::cout << "Device information: " << std::endl;
	//The following code shows how to enumerate all of the RS2_CAMERA_INFO
	//Note that all enum types in the SDK start with the value of zero and end at the "*_COUNT" value
	for (int i = 0; i < static_cast<int>(RS2_CAMERA_INFO_COUNT); i++)
	{
		rs2_camera_info info_type = static_cast<rs2_camera_info>(i);
		//SDK enum types can be streamed to get a string that represents them
		std::cout << "  " << std::left << std::setw(20) << info_type << " : ";
		
		//A device might not support all types of RS2_CAMERA_INFO.
		//To prevent throwing exceptions from the "get_info" method we first check if the device supports this type of info
		if (dev.supports(info_type))
			std::cout << dev.get_info(info_type) << std::endl;
		else
			std::cout << "N/A" << std::endl;
	}
}


RealSense2Engine::RealSense2Engine(const char *calibFilename, bool alignColourWithDepth,
								   Vector2i requested_imageSize_rgb, Vector2i requested_imageSize_d)
: BaseImageSourceEngine(calibFilename)
{
	this->calib.disparityCalib.SetStandard();
	this->calib.trafo_rgb_to_depth = ITMExtrinsics();
	this->calib.intrinsics_d = this->calib.intrinsics_rgb;
	
	this->imageSize_d = requested_imageSize_d;
	this->imageSize_rgb = requested_imageSize_rgb;
	
	this->ctx = std::unique_ptr<rs2::context>(new rs2::context());
	
	rs2::device_list availableDevices = ctx->query_devices();
	
	printf("There are %d connected RealSense devices.\n", availableDevices.size());
	if (availableDevices.size() == 0) {
		dataAvailable = false;
		ctx.reset();
		return;
	}
	
	this->device = std::unique_ptr<rs2::device>(new rs2::device(availableDevices.front()));
	
	print_device_information(*device);
	
	this->pipe = std::unique_ptr<rs2::pipeline>(new rs2::pipeline(*ctx));
	
	rs2::config config;
	config.enable_stream(RS2_STREAM_DEPTH, imageSize_d.x, imageSize_d.y, RS2_FORMAT_Z16, 30);
	config.enable_stream(RS2_STREAM_COLOR, imageSize_rgb.x, imageSize_rgb.y, RS2_FORMAT_RGBA8, 30);
	
	auto availableSensors = device->query_sensors();
	std::cout << "Device consists of " << availableSensors.size() << " sensors:" << std::endl;
	for (rs2::sensor sensor : availableSensors) {
		//print_sensor_information(sensor);
		
		if (rs2::depth_sensor dpt_sensor = sensor.as<rs2::depth_sensor>()) {
			float scale = dpt_sensor.get_depth_scale();
			std::cout << "Scale factor for depth sensor is: " << scale << std::endl;
			this->calib.disparityCalib.SetFrom(scale, 0, ITMLib::ITMDisparityCalib::TRAFO_AFFINE);
		}
	}
	
	rs2::pipeline_profile pipeline_profile = pipe->start(config);
	
	rs2::video_stream_profile depth_stream_profile = pipeline_profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
	rs2::video_stream_profile color_stream_profile = pipeline_profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
	
	// - intrinsics
	rs2_intrinsics intrinsics_depth = depth_stream_profile.get_intrinsics();
	rs2_intrinsics intrinsics_rgb = color_stream_profile.get_intrinsics();
	
	this->calib.intrinsics_d.projectionParamsSimple.fx = intrinsics_depth.fx;
	this->calib.intrinsics_d.projectionParamsSimple.fy = intrinsics_depth.fy;
	this->calib.intrinsics_d.projectionParamsSimple.px = intrinsics_depth.ppx;
	this->calib.intrinsics_d.projectionParamsSimple.py = intrinsics_depth.ppy;
    this->calib.intrinsics_d.projectionParamsSimple.all =
            *((Vector4f *) &this->calib.intrinsics_d.projectionParamsSimple.fx);
    this->calib.intrinsics_d.imgSize.width = depth_stream_profile.width();
    this->calib.intrinsics_d.imgSize.height = depth_stream_profile.height();
	
	this->calib.intrinsics_rgb.projectionParamsSimple.fx = intrinsics_rgb.fx;
	this->calib.intrinsics_rgb.projectionParamsSimple.fy = intrinsics_rgb.fy;
	this->calib.intrinsics_rgb.projectionParamsSimple.px = intrinsics_rgb.ppx;
	this->calib.intrinsics_rgb.projectionParamsSimple.py = intrinsics_rgb.ppy;
    this->calib.intrinsics_rgb.projectionParamsSimple.all =
            *((Vector4f *) &this->calib.intrinsics_rgb.projectionParamsSimple.fx);
    this->calib.intrinsics_rgb.imgSize.width = color_stream_profile.width();
    this->calib.intrinsics_rgb.imgSize.height = color_stream_profile.height();
	
	// - extrinsics
	rs2_extrinsics rs_extrinsics = color_stream_profile.get_extrinsics_to(depth_stream_profile);
	
	Matrix4f extrinsics;
	extrinsics.m00 = rs_extrinsics.rotation[0]; extrinsics.m10 = rs_extrinsics.rotation[1]; extrinsics.m20 = rs_extrinsics.rotation[2];
	extrinsics.m01 = rs_extrinsics.rotation[3]; extrinsics.m11 = rs_extrinsics.rotation[4]; extrinsics.m21 = rs_extrinsics.rotation[5];
	extrinsics.m02 = rs_extrinsics.rotation[6]; extrinsics.m12 = rs_extrinsics.rotation[7]; extrinsics.m22 = rs_extrinsics.rotation[8];
	extrinsics.m30 = rs_extrinsics.translation[0];
	extrinsics.m31 = rs_extrinsics.translation[1];
	extrinsics.m32 = rs_extrinsics.translation[2];
	
	extrinsics.m33 = 1.0f;
	extrinsics.m03 = 0.0f; extrinsics.m13 = 0.0f; extrinsics.m23 = 0.0f;
	
	this->calib.trafo_rgb_to_depth.SetFrom(extrinsics);
}

RealSense2Engine::~RealSense2Engine()
{
	if (pipe) {
		pipe->stop();
	}
}

void RealSense2Engine::getImages(ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage)
{
	dataAvailable = false;
	
	// get frames
	rs2::frameset frames = pipe->wait_for_frames();
	
	rs2::depth_frame depth = frames.get_depth_frame();
	rs2::video_frame color = frames.get_color_frame();

	constexpr size_t rgb_pixel_size = sizeof(Vector4u);
	static_assert(4 == rgb_pixel_size, "sizeof(rgb pixel) must equal 4");
	const Vector4u * color_frame = reinterpret_cast<const Vector4u*>(color.get_data());
	
	constexpr size_t depth_pixel_size = sizeof(uint16_t);
	static_assert(2 == depth_pixel_size, "sizeof(depth pixel) must equal 2");
	auto depth_frame = reinterpret_cast<const uint16_t *>(depth.get_data());
	
	// setup infinitam frames
	short *rawDepth = rawDepthImage->GetData(MEMORYDEVICE_CPU);
	Vector4u *rgb = rgbImage->GetData(MEMORYDEVICE_CPU);
	
	// Let's just memcpy the data instead of using loops
	std::memcpy(rgb, color_frame, rgb_pixel_size * rgbImage->noDims.x*rgbImage->noDims.y);
	std::memcpy(rawDepth, depth_frame, depth_pixel_size * rawDepthImage->noDims.x * rawDepthImage->noDims.y);
	
	dataAvailable = true;
}

bool RealSense2Engine::hasMoreImages(void) const {
	return pipe != nullptr;
}

Vector2i RealSense2Engine::getDepthImageSize(void) const {
	return pipe ? imageSize_d : Vector2i(0,0);
}

Vector2i RealSense2Engine::getRGBImageSize(void) const {
	return pipe ? imageSize_rgb : Vector2i(0,0);
}

#else

using namespace InputSource;

RealSense2Engine::RealSense2Engine(const char *calibFilename, bool alignColourWithDepth,
								   Vector2i requested_imageSize_rgb, Vector2i requested_imageSize_d)
: BaseImageSourceEngine(calibFilename)
{
	printf("compiled without RealSense SDK 2.X support\n");
}
RealSense2Engine::~RealSense2Engine()
{}
void RealSense2Engine::getImages(ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage)
{ return; }
bool RealSense2Engine::hasMoreImages(void) const
{ return false; }
Vector2i RealSense2Engine::getDepthImageSize(void) const
{ return Vector2i(0,0); }
Vector2i RealSense2Engine::getRGBImageSize(void) const
{ return Vector2i(0,0); }

#endif

RealSense2FileEngine::RealSense2FileEngine(const char *inputFileName) {
    this->calib.disparityCalib.SetStandard();
    this->calib.trafo_rgb_to_depth = ITMExtrinsics();
    this->calib.intrinsics_d = this->calib.intrinsics_rgb;


    this->ctx = std::unique_ptr<rs2::context>(new rs2::context());

    rs2::config cfg;
    cfg.enable_device_from_file(inputFileName, false);

    this->pipe = std::unique_ptr<rs2::pipeline>(new rs2::pipeline(*ctx));
    printf("Loading RealSense file from %s.\n", inputFileName);
    rs2::pipeline_profile pipeline_profile = pipe->start(cfg);

    this->device = std::unique_ptr<rs2::device>(new rs2::device(pipe->get_active_profile().get_device()));
    this->device.get()->as<rs2::playback>().set_real_time(false);

    print_device_information(*device);


    auto availableSensors = device->query_sensors();
    std::cout << "Device consists of " << availableSensors.size() << " sensors:" << std::endl;
    for (rs2::sensor sensor : availableSensors) {
        //print_sensor_information(sensor);

        if (rs2::depth_sensor dpt_sensor = sensor.as<rs2::depth_sensor>()) {
            float scale = dpt_sensor.get_depth_scale();
            std::cout << "Scale factor for depth sensor is: " << scale << std::endl;
            this->calib.disparityCalib.SetFrom(scale, 0, ITMLib::ITMDisparityCalib::TRAFO_AFFINE);
        }
    }


    rs2::video_stream_profile depth_stream_profile = pipeline_profile.get_stream(
            RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
    rs2::video_stream_profile color_stream_profile = pipeline_profile.get_stream(
            RS2_STREAM_COLOR).as<rs2::video_stream_profile>();

    this->imageSize_d = Vector2i(depth_stream_profile.width(),depth_stream_profile.height());
    this->imageSize_rgb = Vector2i(color_stream_profile.width(),color_stream_profile.height());
    this->depth_format = depth_stream_profile.format();
    this->color_format = color_stream_profile.format();

    // - intrinsics
    rs2_intrinsics intrinsics_depth = depth_stream_profile.get_intrinsics();
    rs2_intrinsics intrinsics_rgb = color_stream_profile.get_intrinsics();

    this->calib.intrinsics_d.projectionParamsSimple.fx = intrinsics_depth.fx;
    this->calib.intrinsics_d.projectionParamsSimple.fy = intrinsics_depth.fy;
    this->calib.intrinsics_d.projectionParamsSimple.px = intrinsics_depth.ppx;
    this->calib.intrinsics_d.projectionParamsSimple.py = intrinsics_depth.ppy;
    this->calib.intrinsics_d.imgSize.width = depth_stream_profile.width();
    this->calib.intrinsics_d.imgSize.height = depth_stream_profile.height();

    this->calib.intrinsics_rgb.projectionParamsSimple.fx = intrinsics_rgb.fx;
    this->calib.intrinsics_rgb.projectionParamsSimple.fy = intrinsics_rgb.fy;
    this->calib.intrinsics_rgb.projectionParamsSimple.px = intrinsics_rgb.ppx;
    this->calib.intrinsics_rgb.projectionParamsSimple.py = intrinsics_rgb.ppy;
    this->calib.intrinsics_rgb.imgSize.width = color_stream_profile.width();
    this->calib.intrinsics_rgb.imgSize.height = color_stream_profile.height();

    // - extrinsics
    rs2_extrinsics rs_extrinsics = color_stream_profile.get_extrinsics_to(depth_stream_profile);

    Matrix4f extrinsics;
    extrinsics.m00 = rs_extrinsics.rotation[0];
    extrinsics.m10 = rs_extrinsics.rotation[1];
    extrinsics.m20 = rs_extrinsics.rotation[2];
    extrinsics.m01 = rs_extrinsics.rotation[3];
    extrinsics.m11 = rs_extrinsics.rotation[4];
    extrinsics.m21 = rs_extrinsics.rotation[5];
    extrinsics.m02 = rs_extrinsics.rotation[6];
    extrinsics.m12 = rs_extrinsics.rotation[7];
    extrinsics.m22 = rs_extrinsics.rotation[8];
    extrinsics.m30 = rs_extrinsics.translation[0];
    extrinsics.m31 = rs_extrinsics.translation[1];
    extrinsics.m32 = rs_extrinsics.translation[2];

    extrinsics.m33 = 1.0f;
    extrinsics.m03 = 0.0f;
    extrinsics.m13 = 0.0f;
    extrinsics.m23 = 0.0f;

    this->calib.trafo_rgb_to_depth.SetFrom(extrinsics);
}

void RealSense2FileEngine::getImages(ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage) {

    dataAvailable = false;

    // get frames
    rs2::frameset frames;
    bool okay = pipe->try_wait_for_frames(&frames);
    if (!okay) {
        moreFrameFlag = false;
        return;
    }

    rs2::depth_frame depth = frames.get_depth_frame();
    rs2::video_frame color = frames.get_color_frame();

    if (depth_format == RS2_FORMAT_Z16) {
        constexpr size_t depth_pixel_size = sizeof(uint16_t);
        static_assert(2 == depth_pixel_size, "sizeof(depth pixel) must equal 2");
        auto depth_frame = reinterpret_cast<const uint16_t *>(depth.get_data());

        // setup infinitam frames
        short *rawDepth = rawDepthImage->GetData(MEMORYDEVICE_CPU);
        std::memcpy(rawDepth, depth_frame, depth_pixel_size * rawDepthImage->noDims.x * rawDepthImage->noDims.y);
    } else {
        std::cerr << "unsupported depth format " << rs2_format_to_string(depth_format) << std::endl;
    }

    if (color_format == RS2_FORMAT_RGBA8) {

        constexpr size_t rgb_pixel_size = sizeof(Vector4u);
        static_assert(4 == rgb_pixel_size, "sizeof(rgb pixel) must equal 4"); // TODO here is the issue
        const auto color_frame = reinterpret_cast<const Vector4u *>(color.get_data());

        Vector4u *rgb = rgbImage->GetData(MEMORYDEVICE_CPU);

        // Let's just memcpy the data instead of using loops
        std::memcpy(rgb, color_frame, rgb_pixel_size * rgbImage->noDims.x * rgbImage->noDims.y);
    } else if (color_format == RS2_FORMAT_RGB8) {
        auto color_frame = reinterpret_cast<const Vector3u *>(color.get_data());
        Vector4u *rgb = rgbImage->GetData(MEMORYDEVICE_CPU);
        const int n = rgbImage->noDims.x * rgbImage->noDims.y;
        for (int i = 0; i < n; i++) {
            rgb[i].r = color_frame[i].r;
            rgb[i].g = color_frame[i].g;
            rgb[i].b = color_frame[i].b;
            rgb[i].a = ~(unsigned short) 0U;
        }
    } else {
        std::cerr << "unsupported color format " << rs2_format_to_string(color_format) << std::endl;
    }

    dataAvailable = true;
}

bool RealSense2FileEngine::hasMoreImages() const {
    return moreFrameFlag;
}
