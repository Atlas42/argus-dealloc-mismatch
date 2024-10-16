#include <iostream>
#include <memory>
#include <functional>

#include "Argus/Argus.h"
#include <cudaEGL.h>

#define CHECK(C) if (!(C)) throw std::runtime_error("ARGUS error at L" + std::to_string(__LINE__))
#define CHECK_A(C) CHECK((C) == Argus::STATUS_OK)
#define CHECK_C(C) CHECK((C) == CUDA_SUCCESS)

template <typename T>
using Cleanup = std::unique_ptr<T, std::function<void(T*)>>;

int main() {
    std::cout << "Started" << std::endl;

    cuInit(0);

    // Create provider
    Argus::UniqueObj<Argus::CameraProvider> provider;
    provider.reset(Argus::CameraProvider::create());

    // -- Init
    auto iCameraProvider = Argus::interface_cast<Argus::ICameraProvider>(provider); CHECK(iCameraProvider);

    // Get devices
    std::vector<Argus::CameraDevice*> cameraDevices;
    CHECK_A(iCameraProvider->getCameraDevices(&cameraDevices)); CHECK(cameraDevices.size() > 0);
    auto cameraDevice = cameraDevices[0]; CHECK(cameraDevice);

    // Select sensor mode
    auto iCameraProperties = Argus::interface_cast<Argus::ICameraProperties>(cameraDevice); CHECK(iCameraProperties);

    std::vector<Argus::SensorMode*> sensorModes;
    CHECK_A(iCameraProperties->getAllSensorModes(&sensorModes)); CHECK(sensorModes.size() > 0);

    auto sensorMode = sensorModes[0];
    auto iSensorMode = Argus::interface_cast<Argus::ISensorMode>(sensorMode); CHECK(iSensorMode);

    auto resolution = iSensorMode->getResolution();

    std::cout << "Camera resolution: " << resolution.width() << "x" << resolution.height() << std::endl;

    // Create the capture session
    Argus::UniqueObj captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    auto iCaptureSession = Argus::interface_cast<Argus::ICaptureSession>(captureSession); CHECK(iCaptureSession);

    // Create output stream
    Argus::UniqueObj streamSettings(iCaptureSession->createOutputStreamSettings(Argus::STREAM_TYPE_EGL));
    auto iEGLStreamSettings = Argus::interface_cast<Argus::IEGLOutputStreamSettings>(streamSettings); CHECK(iEGLStreamSettings);
    iEGLStreamSettings->setPixelFormat(Argus::PIXEL_FMT_YCbCr_420_888);
    iEGLStreamSettings->setResolution(iSensorMode->getResolution());
    iEGLStreamSettings->setMetadataEnable(true);

    Argus::UniqueObj outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    auto iEGLOutputStream = Argus::interface_cast<Argus::IEGLOutputStream>(outputStream); CHECK(iEGLOutputStream);

    Cleanup<Argus::IEGLOutputStream> osCleanup(iEGLOutputStream, [](auto s){ s->disconnect(); });

    // Connect to cuda
    CUdevice cuDevice;
    CUcontext cuContext;
    CHECK_C(cuDeviceGet(&cuDevice, 0));
    CHECK_C(cuDevicePrimaryCtxRetain(&cuContext, cuDevice));
    Cleanup<CUdevice> ctxCleanup(&cuDevice, [](auto d){ CHECK_C(cuDevicePrimaryCtxRelease(*d)); });

    CHECK_C(cuCtxPushCurrent(cuContext));
    CUeglStreamConnection cudaConnection;
    CHECK_C(cuEGLStreamConsumerConnect(&cudaConnection, iEGLOutputStream->getEGLStream()));
    Cleanup<CUeglStreamConnection> conCleanup(&cudaConnection, [](auto c){ cuEGLStreamConsumerDisconnect(c); });
    CHECK_C(cuCtxPopCurrent(nullptr));

    // Create capture request, set sensor mode, and enable output stream.
    Argus::UniqueObj request(iCaptureSession->createRequest());
    auto iRequest = Argus::interface_cast<Argus::IRequest>(request); CHECK(iRequest);
    iRequest->enableOutputStream(outputStream.get());

    // Start capture session
    CHECK(iCaptureSession->repeat(request.get()) == Argus::STATUS_OK);
    Cleanup<Argus::ICaptureSession> rptCleanup(iCaptureSession, [](auto s){s->stopRepeat(); s->waitForIdle();});

    std::cout << "Ended" << std::endl;
}
