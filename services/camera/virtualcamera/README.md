# Virtual Camera

The virtual camera feature allows 3rd party applications to expose a remote or
virtual camera to the standard Android camera frameworks (Camera2/CameraX, NDK,
camera1).

The stack is composed into 4 different parts:

1.  The **Virtual Camera Service** (this directory), implementing the Camera **HAL**
    and acts as an interface between the Android Camera Server and the *Virtual
    Camera Owner* (via the VirtualDeviceManager APIs).

2.  The **VirtualDeviceManager** (VDM) running in the system process and handling the
    communication between the Virtual Camera service and the Virtual Camera
    owner

3.  The **Virtual Camera Owner** (VCO), the client application declaring the Virtual
    Camera and handling the production of image data. We will also refer to this
    part as the **producer**

4.  The **Consumer Application**, the client application consuming camera data,
    which can be any application using the camera APIs

This document describes the functionalities of the *Virtual Camera Service*

## Before reading

The service implements the Camera HAL. It's best to have a bit of an
understanding of how it works by reading the
[HAL documentation first](https://source.android.com/docs/core/camera).

![](https://source.android.com/static/docs/core/camera/images/ape_fwk_camera2.png)

The documentation for the behavior of each HAL implementation is available in the aidl definition of the Camera HAL under `$ANDROID_BUILD_TOP/hardware/interfaces/camera/`. The main interfaces are:

- `ICameraDeviceSession.aidl`
- `ICameraProvider.aidl`
- `ICameraDevice.aidl`

The Virtual Camera HAL implementations are declared in:
- [VirtualCameraDevice](./VirtualCameraDevice.h)
- [VirtualCameraProvider](./VirtualCameraProvider.h)
- [VirtualCameraSession](./VirtualCameraSession.h)

## Current supported features

Virtual Cameras report `EXTERNAL`
[hardware level](https://developer.android.com/reference/android/hardware/camera2/CameraCharacteristics#INFO_SUPPORTED_HARDWARE_LEVEL)
but some
[functionalities of `EXTERNAL`](https://developer.android.com/reference/android/hardware/camera2/CameraMetadata#INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL)
hardware level are not fully supported.

Here is a list of supported features:
- Support for YUV and JPEG capture and preview

Notable missing features:
-   Support for auto 3A (AWB, AE, AF): virtual camera will announce convergence
    of 3A algorithm even though it can't receive any information about this from
    the owner.

-   No flash/torch support

## Overview

The virtual camera HAL can be divided into 3 main parts:

 - The AIDL service which exposes the virtual camera capabilities to the Android System ([VirtualCameraService](./VirtualCameraService.h)), used to declare instance of virtual cameras. The services communicate back to the server via the [VirtualCameraCallback](./aidl/android/companion/virtualcamera/IVirtualCameraCallback.aidl).
 - The implementation the camera AIDL interface where the capture requests are received. ([VirtualCameraDevice](./VirtualCameraDevice.h), [VirtualCameraProvider](./VirtualCameraProvider.h), [VirtualCameraSession](./VirtualCameraSession.h))
 - The render thread which processes the requests and return the results ([VirtualCameraRenderThread](./VirtualCameraRenderThread.h))

At the core of the system is a CaptureRequest queue that is filled when a call to [ICameraDeviceSession::processCaptureRequest] is made from the App. The queue is processed by the `VirtualCameraRenderThread`. For each request, the render thread will consume a buffer from the input suface filled by the VCO, copy it to the ouput buffer, and return the result to the camera framework.

 > Look at [Life of a capture request](#life-of-a-capture-request) section for more details.


## Graphic data
Graphic data are exchanged shared memory infrastructure. We differentiate the input Surface from the output Surfaces. The input Surface is the one onto which the Virtual Camera Owner writes the image data. The output Surfaces are the ones onto which the Virtual Camera writes the image data for the Camera application to consume.

Like any other Camera HALs, the _output_ Surfaces, onto which the end data is written for the Camera application to consume are received from the Camera application via the [camera server](../../../camera/cameraserver/).

For the producer side, the Virtual Camera exposes a **different** Surface onto which the owner can write
data. That Surface is backed by an EGL Surface Texture which transforms (if needed) the producer
data to the required consumer format (scaling only for now, but we might also
add support for rotation and cropping in the future).

By default, the Virtual Camera supports only one input stream. The biggest resolution stream from the requested output stream will be chosen for the input stream. The data will then be downscaled to the other output stream resolutions when needed.

The Virtual Camera supports multiple input streams if the VCO configures it using `VirtualCameraConfig.Builder.setConcurrentStreamConfigSupported(true)`. In this configuration, the service can request a separate input Surface for each output stream resolution requested by the consumer. This allow the VCO to produce frame asynchronously


Depending on the type of output, the rendering pipelines change. Here is an
overview of the YUV and JPEG pipelines.

#### YUV Rendering:


```
Virtual Device Owner Surface[1] (Producer) --{binds to}--> EGL
Texture[1] --{renders into}--> Client Surface[1-n] (Consumer)
```

#### JPEG Rendering:

When the consumer requests a JPEG format but the producer provides YUV data (or when passthrough is not applicable), the virtual camera performs a YUV to JPEG conversion. The YUV frame provided by the Virtual Camera Owner is bound to an EGL Texture. It is then rendered and read into a temporary buffer where software compression is applied. The resulting compressed JPEG payload is then copied into the Consumer Application's output Surface.


```
Virtual Device Owner Surface[1] (Producer) --{binds to}--> EGL
Texture[1] --{compress data into}--> temporary buffer --{renders into}-->
Client Surface[1-n] (Consumer)
```

#### Passthrough (JPEG/HEIC)

When the requested format is a BLOB (such as JPEG or HEIC), and the requested output resolution matches exactly an input resolution, the virtual camera uses a direct passthrough approach. The Virtual Camera Owner writes the pre-encoded data into a Surface backed by an `ImageReader`. The `VirtualCameraRenderThread` then directly copies the payload from the input buffer to the Consumer Application's output buffer (`memcpy`), bypassing the EGL rendering pipeline entirely.

```
Virtual Device Owner Surface (Producer) --{binds to}--> ImageReader
--{memcpy data into}--> Client Surface (Consumer)
```



## Life of a capture request

> Before reading the following, you must understand the concepts of
> [CaptureRequest](https://developer.android.com/reference/android/hardware/camera2/CaptureRequest)
> and
> [OutputConfiguration](https://developer.android.com/reference/android/hardware/camera2/OutputConfiguration).

1.  The Camera Application (consumer) creates a session with one or more `Surfaces`. Each Surfaces is back by an EGL Texture.

1.  The VirtualCamera owner (VCO/producer) receives a call to
    `VirtualCameraCallback#onConfigureSession` with the session parameters requested by the app.

1.  The VCO receives a call to
    `VirtualCameraCallback#onStreamConfigured` with a reference to a `Surface` where data will be written.
    It will receive a call for either:
      a. The biggest requested ouput matching an input configuration in single stream mode
      b. For each requested input stream 

1.  The consumer starts sending `CaptureRequests`. The producer
    receives a call to `VirtualCameraCallback#onProcessCaptureRequest`, at which
    points it should write the required data into the corresponding surface.

1.  The [VirtualCameraRenderThread](./VirtualCameraRenderThread.cc) consumes
    the enqueued tasks as they come.

    a. It will wait for the producer to write into
    the input Surface (using `Surface::waitForNextFrame`). If the producer is too fast
    compared to the consumer's requested FPS, the thread will throttle to match the maxFps.

    > **Note:** Since the Surface API allows us to wait for the next frame,
    > there is no need for the producer to notify when the frame is ready by
    > calling a `processCaptureResult()` equivalent.

    b.  The EGL Texture is updated with the content of the Surface.

    c.  The content is copied into the output buffer.

1.  The render thread notifies (`ICameraDeviceCallback::notify`) the Camera client "shutter" event and the `CaptureResult`
    is immediatly sent to the consumer.

    >**Note**: usually the `notify()` call should be made at the start of the exposure, but the VirtualCamera is taking a shortcut here by only calling notify when the buffers are filled and returning them in `processCaptureResult()`.

## EGL Rendering

### The render thread

The [VirtualCameraRenderThread](./VirtualCameraRenderThread.h) module takes care
of rendering the input from the owner to the output via the EGL Texture. The
rendering is done either to a JPEG buffer, which is the BLOB rendering for
creating a JPEG or to a YUV buffer used mainly for preview Surfaces or video.
Two EGLPrograms (shaders) defined in [EglProgram](./util/EglProgram.cc) handle
the rendering of the data.

### Initialization

[EGlDisplayContext](./util/EglDisplayContext.h) initializes the whole EGL
environment (Display, Surface, Context, and Config).

The EGL Rendering is backed by a
[ANativeWindow](https://developer.android.com/ndk/reference/group/a-native-window)
which is just the native counterpart of the
[Surface](https://developer.android.com/reference/android/view/Surface), which
itself is the producer side of buffer queue, the consumer being either the
display (Camera preview) or some encoder (to save the data or send it across the
network).

### More about OpenGL

To better understand how the EGL rendering works the following resources can be
used:

Introduction to OpenGL: https://learnopengl.com/

The official documentation of EGL API can be queried at:
https://www.khronos.org/registry/egl/sdk/docs/man/xhtml/

And using Google search with the following query:

```
[function name] site:https://registry.khronos.org/EGL/sdk/docs/man/html/

// example: eglSwapBuffers site:https://registry.khronos.org/EGL/sdk/docs/man/html/
```
