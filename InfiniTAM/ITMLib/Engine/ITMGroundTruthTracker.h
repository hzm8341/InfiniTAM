
#ifndef INFINITAM_ITMGROUNDTRUTHTRACKER_H
#define INFINITAM_ITMGROUNDTRUTHTRACKER_H

#include <iostream>
#include <fstream>

#include "ITMTracker.h"
#include "../Utils/ITMOxtsIO.h"

namespace ITMLib {
  namespace Engine {

    using namespace ITMLib::Objects;
    using namespace std;

    /**
     * Dummy tracker which relays pose information from a file.
     * The currently supported file format is the ground truth odometry information from
     * the KITTI odometry dataset.
     *
     * Note that this information is not a 100% "cheaty" ground truth computed using, e.g.,
     * manual annotation or beacons, but the one recorded by the vehicle's IMG/GPS module.
     *
     * In the future, nevertheless, it would be much cooler to just do this using sparse-to-dense
     * odometry from stereo (and maybe lidar) data.
     */
    class ITMGroundTruthTracker : public ITMTracker {

    private:
      int currentFrame = 0;
      vector<Matrix4f> groundTruthPoses;

      // TODO(andrei): Move this helper out of here.
	    /// \brief Loads a KITTI-odometry ground truth pose file.
	    /// \return A list of absolute vehicle poses expressed as 4x4 matrices.
      vector<Matrix4f> readKittiOdometryPoses(const std::string &fpath) {
        ifstream fin(fpath.c_str());
	      if (! fin.is_open()) {
          throw runtime_error("Could not open odometry file.");
        }

	      cout << "Loading odometry ground truth from file: " << fpath << endl;
        vector<Matrix4f> poses;
        while (! fin.eof()) {
	        // This matrix takes a point in the ith coordinate system, and projects it
	        // into the first (=0th, or world) coordinate system.
	        //
	        // The M matrix in InfiniTAM is a modelview matrix, so it transforms points from
	        // the world coordinates, to camera coordinates.
	        //
	        // One therefore needs to set this pose as 'InvM' in the InfiniTAM tracker state.

          Matrix4f pose;
          fin >> pose.m00 >> pose.m10 >> pose.m20 >> pose.m30
              >> pose.m01 >> pose.m11 >> pose.m21 >> pose.m31
              >> pose.m02 >> pose.m12 >> pose.m22 >> pose.m32;
	        pose.m03 = pose.m13 = pose.m23 = 0.0f;
          pose.m33 = 1.0f;
	        poses.push_back(pose);
        }

        return poses;
      }

	    // Taken from 'ITMPose' to aid with debugging.
	    // TODO(andrei): Move to common utility. Can make math-intense code much cleaner.
	    Matrix3f getRot(const Matrix4f M) {
		    Matrix3f R;
		    R.m[0 + 3*0] = M.m[0 + 4*0]; R.m[1 + 3*0] = M.m[1 + 4*0]; R.m[2 + 3*0] = M.m[2 + 4*0];
		    R.m[0 + 3*1] = M.m[0 + 4*1]; R.m[1 + 3*1] = M.m[1 + 4*1]; R.m[2 + 3*1] = M.m[2 + 4*1];
		    R.m[0 + 3*2] = M.m[0 + 4*2]; R.m[1 + 3*2] = M.m[1 + 4*2]; R.m[2 + 3*2] = M.m[2 + 4*2];
		    return R;
	    }

    protected:

    public:
      ITMGroundTruthTracker(const string &groundTruthFpath) {
        cout << "Created ground truth-based tracker. Will read data from: "
             << groundTruthFpath << endl;

	      groundTruthPoses = readKittiOdometryPoses(groundTruthFpath);
	      // TODO(andrei): This code, although untested, should provide a skeleton for
	      // reading OxTS data, such that ground truth from the full KITTI dataset can
	      // be read.
//        vector<OxTSFrame> groundTruthFrames = Objects::readOxtsliteData(groundTruthFpath);
//        groundTruthPoses = Objects::oxtsToPoses(groundTruthFrames, groundTruthTrans, groundTruthRots);
      }

      void TrackCamera(ITMTrackingState *trackingState, const ITMView *view) {
		    this->currentFrame++;
				Matrix4f M = groundTruthPoses[currentFrame];

	      // Mini-hack to ensure translation magnitude is "calibrated" to the
	      // range of the depth map.
		    M.m30 *= 0.10;
		    M.m31 *= 0.10;
		    M.m32 *= 0.10;

	      trackingState->pose_d->SetInvM(M);
      }

      // Note: this doesn't seem to get used much in InfiniTAM. It's just
      // called from 'ITMMainEngine', but none of its implementations
      // currently do anything.
      void UpdateInitialPose(ITMTrackingState *trackingState) {}

      virtual ~ITMGroundTruthTracker() {}

    };

  }
}

#endif //INFINITAM_ITMGROUNDTRUTHTRACKER_H
