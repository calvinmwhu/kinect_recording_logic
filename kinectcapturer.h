#ifndef KINECTCAPTURER_H
#define KINECTCAPTURER_H


#include <windows.h>
#include <iostream>
#include <queue>

#include "NuiApi.h"
#include "opencv2\opencv.hpp"


class KinectCapturer
{
    static const int                    cBytesPerPixel   = 4;

    static const NUI_IMAGE_RESOLUTION   cDepthResolution = NUI_IMAGE_RESOLUTION_640x480;
    static const NUI_IMAGE_RESOLUTION   cColorResolution = NUI_IMAGE_RESOLUTION_640x480;
public:
    KinectCapturer();
    KinectCapturer(int);
    ~KinectCapturer(void);
    HANDLE getPointCloudEvent() const;
    HRESULT                 connectSensor();
    void					cleanUp();


    void enter()
        { ::EnterCriticalSection(&m_rep); }
    void leave()
        { ::LeaveCriticalSection(&m_rep); }

private:
    // Kinect

    INuiSensor*            m_pNuiSensor;
    int					   m_nIndex;
    INuiCoordinateMapper*  m_pMapper;

    // buffers
    USHORT*                             m_depthD16;
    BYTE*                               m_colorRGBX;

    //HANDLE                  m_hNextDepthFrameEvent;
    HANDLE                  m_pDepthStreamHandle;
    //HANDLE                  m_hNextColorFrameEvent;
    HANDLE					m_hLastFrameEvent;
    HANDLE                  m_pColorStreamHandle;
    HANDLE					m_hEventThread;
    HANDLE					m_hStopStreamEventThread;
    HANDLE					m_hPointCloudEvent;
    //HANDLE					m_hRenderEvent;

    LONG                    m_depthWidth;
    LONG                    m_depthHeight;

    LONG                    m_colorWidth;
    LONG                    m_colorHeight;


    static DWORD 			StreamEventThread(KinectCapturer*);
    HRESULT					convertFrameToPointCloud();

    KinectCapturer(const KinectCapturer&);
    KinectCapturer& operator=(const KinectCapturer&);
    std::queue<std::string> qqq;
    long long m_count;
    BYTE *color_frame;
    BYTE *depth_frame;
    BYTE *color_buffer;
    BYTE *depth_buffer;

    // synchronization
    CRITICAL_SECTION m_rep;
};


#endif // KINECTCAPTURER

