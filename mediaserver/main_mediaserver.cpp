/* Device entry point for the stock MTK camera service ABI. */

#include <new>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include <AudioFlinger.h>
#include <CameraService.h>
#include <MediaPlayerService.h>
#include <AudioPolicyService.h>

using namespace android;

int main(int argc, char** argv)
{
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    LOGI("ServiceManager: %p", sm.get());
    AudioFlinger::instantiate();
    MediaPlayerService::instantiate();

    // The constructor comes from the stock libcameraservice.so and writes
    // 0x68 bytes although the CM7 CameraService header describes 0x48.
    sm->addService(String16(CameraService::getServiceName()),
            new (operator new(MTK_CAMERA_SERVICE_SIZE)) CameraService());

    AudioPolicyService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}
