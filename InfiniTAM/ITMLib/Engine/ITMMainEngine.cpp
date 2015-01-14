// Copyright 2014 Isis Innovation Limited and the authors of InfiniTAM

#include "ITMMainEngine.h"

#include "ITMTrackerFactory.h"
using namespace ITMLib::Engine;

ITMMainEngine::ITMMainEngine(const ITMLibSettings *settings, const ITMRGBDCalib *calib, Vector2i imgSize_rgb, Vector2i imgSize_d)
{
	if ((imgSize_d.x == -1) || (imgSize_d.y == -1)) imgSize_d = imgSize_rgb;

	this->settings = new ITMLibSettings(*settings);

	this->trackingState = ITMTrackerFactory::MakeTrackingState(*settings, imgSize_rgb, imgSize_d);
	trackingState->pose_d->SetFrom(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); 

	this->view = new ITMView(*calib, imgSize_rgb, imgSize_d, settings->useGPU);

	denseMapper = new ITMDenseMapper<ITMVoxel, ITMVoxelIndex>(this->settings, imgSize_rgb, imgSize_d);

	if (settings->useGPU)
	{
#ifndef COMPILE_WITHOUT_CUDA
		lowLevelEngine = new ITMLowLevelEngine_CUDA();
#endif
	}
	else
	{
		lowLevelEngine = new ITMLowLevelEngine_CPU();
	}

	trackerPrimary = ITMTrackerFactory::MakePrimaryTracker(*settings, imgSize_rgb, imgSize_d, lowLevelEngine);
	trackerSecondary = ITMTrackerFactory::MakeSecondaryTracker<ITMVoxel,ITMVoxelIndex>(*settings, imgSize_rgb, imgSize_d, lowLevelEngine, denseMapper->getScene());

	hasStartedObjectReconstruction = false;
	fusionActive = true;
}

ITMMainEngine::~ITMMainEngine()
{
	delete denseMapper;
	if (trackerPrimary != NULL) delete trackerPrimary;
	if (trackerSecondary != NULL) delete trackerSecondary;
	delete lowLevelEngine;

	delete trackingState;
	delete view;

	delete settings;
}

void ITMMainEngine::ProcessFrame(void)
{
	bool useGPU = settings->useGPU;

	// prepare image and move it to GPU, if required
	if (useGPU)
	{
		view->rgb->UpdateDeviceFromHost();

		switch (view->inputImageType)
		{
		case ITMView::InfiniTAM_FLOAT_DEPTH_IMAGE: view->depth->UpdateDeviceFromHost(); break;
		case ITMView::InfiniTAM_SHORT_DEPTH_IMAGE:
		case ITMView::InfiniTAM_DISPARITY_IMAGE: view->rawDepth->UpdateDeviceFromHost(); break;
		}
	}

	// prepare image and turn it into a depth image
	if (view->inputImageType == ITMView::InfiniTAM_DISPARITY_IMAGE)
	{
		lowLevelEngine->ConvertDisparityToDepth(view->depth, view->rawDepth, &(view->calib->intrinsics_d), &(view->calib->disparityCalib));
	}
	else if (view->inputImageType == ITMView::InfiniTAM_SHORT_DEPTH_IMAGE)
	{
		lowLevelEngine->ConvertDepthMMToFloat(view->depth, view->rawDepth);
	}

	// tracking
	if (hasStartedObjectReconstruction)
	{
		if (trackerPrimary != NULL) trackerPrimary->TrackCamera(trackingState, view);
		if (trackerSecondary != NULL) trackerSecondary->TrackCamera(trackingState, view);

	}


	if (fusionActive) denseMapper->ProcessFrame(view, trackingState->pose_d);

	switch (settings->trackerType)
	{
	case ITMLibSettings::TRACKER_ICP:
	case ITMLibSettings::TRACKER_REN:
		// raycasting
		denseMapper->GetICPMaps(trackingState->pose_d, &(view->calib->intrinsics_d), view, trackingState);
		break;
	case ITMLibSettings::TRACKER_COLOR:
		// raycasting
		ITMPose pose_rgb(view->calib->trafo_rgb_to_depth.calib_inv * trackingState->pose_d->M);
		denseMapper->GetPointCloud(&pose_rgb, &(view->calib->intrinsics_rgb), view, trackingState, settings->skipPoints);
		break;
	}

	hasStartedObjectReconstruction = true;
}

void ITMMainEngine::GetImage(ITMUChar4Image *out, GetImageType getImageType, bool useColour, ITMPose *pose, ITMIntrinsics *intrinsics)
{
	out->Clear();

	switch (getImageType)
	{
	case ITMMainEngine::InfiniTAM_IMAGE_ORIGINAL_RGB:
		if (settings->useGPU) view->rgb->UpdateHostFromDevice();
		out->ChangeDims(view->rgb->noDims);
		out->SetFrom(view->rgb);
		break;
	case ITMMainEngine::InfiniTAM_IMAGE_ORIGINAL_DEPTH:
		if (settings->useGPU) view->depth->UpdateHostFromDevice();
		out->ChangeDims(view->depth->noDims);
		ITMVisualisationEngine<ITMVoxel,ITMVoxelIndex>::DepthToUchar4(out, view->depth);
		break;
	case ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST:
		if (settings->useGPU) trackingState->rendering->UpdateHostFromDevice();
		out->ChangeDims(trackingState->rendering->noDims);
		out->SetFrom(trackingState->rendering);
		break;
	case ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST_FREECAMERA:
		return denseMapper->GetRendering(pose, intrinsics, useColour, out);
		break;
	};
}

void ITMMainEngine::turnOnIntegration()
{
	fusionActive = true;
}

void ITMMainEngine::turnOffIntegration()
{
	fusionActive = false;
}
