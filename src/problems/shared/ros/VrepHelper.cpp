/** @file VrepHelper.cpp
 *
 * Contains the implementation of the VrepHelper class.
 */
#include "VrepHelper.hpp"

#include <ros/package.h>  // For finding package path
#include <vector>
#include <geometry_msgs/Point.h>

/** 
 * ROS service files actually from package vrep_common
 * However, vrep_common headers cannot be included
 * because vrep_common is a rosbuild package
 * Therefore they are generated by this package
 */
#include "tapir/simRosCopyPasteObjects.h" 
#include "tapir/simRosGetObjectHandle.h"
#include "tapir/simRosSetObjectPosition.h"
#include "tapir/simRosStartSimulation.h"
#include "tapir/simRosStopSimulation.h"
#include "tapir/simRosGetObjectPose.h"
#include "tapir/simRosLoadScene.h"

VrepHelper::VrepHelper():
    running_(false)
{
}

VrepHelper::VrepHelper(ros::NodeHandle *node):
    running_(false)
{
    setRosNode(node);
}

void VrepHelper::setRosNode(ros::NodeHandle *node) {
    node_ = node;
    startClient_ = node_->serviceClient<tapir::simRosStartSimulation>("vrep/simRosStartSimulation");
    stopClient_ = node_->serviceClient<tapir::simRosStopSimulation>("vrep/simRosStopSimulation");
    copyClient_ = node_->serviceClient<tapir::simRosCopyPasteObjects>("vrep/simRosCopyPasteObjects");
    handleClient_ = node_->serviceClient<tapir::simRosGetObjectHandle>("vrep/simRosGetObjectHandle");
    moveClient_ = node_->serviceClient<tapir::simRosSetObjectPosition>("vrep/simRosSetObjectPosition");
    poseClient_ = node_->serviceClient<tapir::simRosGetObjectPose>("vrep/simRosGetObjectPose");
    loadClient_ = node_->serviceClient<tapir::simRosLoadScene>("vrep/simRosLoadScene");
    infoSub_ = node_->subscribe("/vrep/info", 1, &VrepHelper::infoCallback, this);
}

/**
 * Attempt to start or unpause VREP simulation 
 * VREP must already be running. Returns true if success
 */
bool VrepHelper::start() {
    tapir::simRosStartSimulation startSrv;
    startClient_.call(startSrv);
    return startSrv.response.result != -1;
}

/** Attempt to stop VREP simulation. Returns true if success */
bool VrepHelper::stop() {
    tapir::simRosStopSimulation stopSrv;
    stopClient_.call(stopSrv);
    return stopSrv.response.result != -1;
}

/**
 * Returns the handle of an object in VREP simulation
 * Returns -1 if failure
 */
long VrepHelper::getHandle(std::string name) {
	tapir::simRosGetObjectHandle handleSrv;
	handleSrv.request.objectName = name;
    handleClient_.call(handleSrv);
    return handleSrv.response.handle;
}

/**
 * Move an object in VREP simulation to a new position
 * Returns true if success.
 */
bool VrepHelper::moveObject(std::string name, float x, float y, float z) {
	long handle = getHandle(name);
    return moveObject(handle, x, y, z);
}

/**
 * Move an object by handle in VREP simulation to
 * a new position. Returns true if success.
 */
bool VrepHelper::moveObject(long handle, float x, float y, float z) {
	geometry_msgs::Point msg;
	msg.x = x;
    msg.y = y;
    msg.z = z;
    tapir::simRosSetObjectPosition moveSrv;
    moveSrv.request.handle = handle;
    moveSrv.request.relativeToObjectHandle = -1;
    moveSrv.request.position = msg;
    moveClient_.call(moveSrv);
    return moveSrv.response.result != -1;
}

/** Copy an object in VREP simulation. Returns handle of copied object. */
long VrepHelper::copyObject(long handle) {
    std::vector<int> handles;
    handles.push_back(handle);
    tapir::simRosCopyPasteObjects copySrv;
    copySrv.request.objectHandles = handles;
    copyClient_.call(copySrv);
    return copySrv.response.newObjectHandles[0];
}

/** Get the pose of an object in VREP simulation */
geometry_msgs::PoseStamped VrepHelper::getPose(long handle) {
    tapir::simRosGetObjectPose poseSrv;
    poseSrv.request.handle = handle;
    poseSrv.request.relativeToObjectHandle = -1;
    poseClient_.call(poseSrv);
    return poseSrv.response.pose;
}

/** Returns true iff VREP simulation is not stopped */
bool VrepHelper::isRunning() {
    ros::spinOnce();
    return running_;
}

/** Loads a V-REP scene (.ttt file) from absolute file path.
 *  Returns true if success
 */
bool VrepHelper::loadScene(std::string fullPath) {
    tapir::simRosLoadScene loadSrv;
    loadSrv.request.fileName = fullPath;
    loadClient_.call(loadSrv);
    return loadSrv.response.result == 1;
}

/** Loads a V-REP scene (.ttt file), starting from
	 *  <package location>/problems/<problem name>/
	 *  Returns true if success
	 */
bool VrepHelper::loadScene(std::string problemName, std::string relativePath,
		std::string packageName) {
	std::string problemPath = ros::package::getPath(packageName) +
			"/problems/" + problemName;
	std::string fullPath = problemPath + "/" + relativePath;
	return loadScene(fullPath);
}

/** Callback for the /vrep/info topic */
void VrepHelper::infoCallback(const tapir::VrepInfo::ConstPtr& msg) {

    // Set running_ true iff simulation is not stopped/paused
    int status = msg->simulatorState.data;
    running_ = (status == 1);
}
