#include "kinectcapturer.h"
#include <string>
#include <sstream>
#include <fstream>
#include "rapidjson/prettywriter.h" // for stringify JSON
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filestream.h"   // wrapper of C stream for prettywriter as output
using namespace std;
using namespace rapidjson;
#define FILE_PREFIX "C:\\data\\"
#define FRAME_NUM 130

#define ERROR_CHECK( ret )  \
    if ( ret != S_OK ) {    \
    std::stringstream ss;	\
    ss << "failed " #ret " " << std::hex << ret << std::endl;	\
    throw std::runtime_error( ss.str().c_str() );			\
    }

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface*& pInterfaceToRelease)
{
    if (pInterfaceToRelease)
    {
        pInterfaceToRelease->Release();
        pInterfaceToRelease = nullptr;
    }
}


KinectCapturer::KinectCapturer()
{}

KinectCapturer::KinectCapturer(int index):
    m_nIndex(index),
    m_pNuiSensor(nullptr),
    m_count(0)
    //m_hRenderEvent(event)
{
    ::InitializeCriticalSection(&m_rep);
    // get resolution as DWORDS, but store as LONGs to avoid casts later
    DWORD width = 0;
    DWORD height = 0;

    NuiImageResolutionToSize(cDepthResolution, width, height);
    m_depthWidth  = static_cast<LONG>(width);
    m_depthHeight = static_cast<LONG>(height);

    NuiImageResolutionToSize(cColorResolution, width, height);
    m_colorWidth  = static_cast<LONG>(width);
    m_colorHeight = static_cast<LONG>(height);

    color_buffer  = new BYTE[m_colorWidth * m_colorHeight *3 * FRAME_NUM];
    depth_buffer  = new BYTE[m_depthWidth * m_depthHeight * FRAME_NUM];
    color_frame = new BYTE[m_colorWidth * m_colorHeight *3];
    depth_frame = new BYTE[m_depthWidth * m_depthHeight];
}


KinectCapturer::~KinectCapturer(void)
{
    cleanUp();
}

HRESULT KinectCapturer::connectSensor()
{
    INuiSensor * pNuiSensor = nullptr;
    HRESULT hr;

    hr = NuiCreateSensorByIndex(m_nIndex, &pNuiSensor);
    if (FAILED(hr))
    {
        return E_FAIL;
    }

    // Get the status of the sensor, and if connected, then we can initialize it
    hr = pNuiSensor->NuiStatus();
    if (S_OK == hr)
    {
        m_pNuiSensor = pNuiSensor;
    }
    else
    {
        pNuiSensor->Release();
        return E_FAIL;
    }

    if (NULL == m_pNuiSensor)
    {
        return E_FAIL;
    }

    // Initialize the Kinect and specify that we'll be using depth
    hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX);
    ERROR_CHECK(hr);
    hr = m_pNuiSensor->NuiGetCoordinateMapper(&m_pMapper);
    ERROR_CHECK(hr);

    // Open a depth image stream to receive depth frames
    hr = m_pNuiSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
        cDepthResolution,
        0,
        2,
        NULL,
        &m_pDepthStreamHandle);
    ERROR_CHECK(hr);

    // Open a color image stream to receive color frames
    hr = m_pNuiSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,
        cColorResolution,
        0,
        2,
        NULL,
        &m_pColorStreamHandle );
    ERROR_CHECK(hr);
    if (SUCCEEDED(hr))
            {
                hr = m_pNuiSensor->NuiSkeletonTrackingEnable(NULL, NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE);
            }

    m_hStopStreamEventThread = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hLastFrameEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hr = m_pNuiSensor->NuiSetFrameEndEvent(m_hLastFrameEvent, 0);
    //m_hPointCloudEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hEventThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)StreamEventThread, this, 0, nullptr);
    std::cout<< "connect sensor end"<< std::endl;
    return hr;
}



void KinectCapturer::cleanUp()
{
    SetEvent(m_hStopStreamEventThread);
    WaitForSingleObject(m_hEventThread, INFINITE);
    if (m_hEventThread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hEventThread);
    }
    if (m_hStopStreamEventThread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hStopStreamEventThread);
    }
    if (m_hLastFrameEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hLastFrameEvent);
    }
    SafeRelease(m_pNuiSensor);
    ::DeleteCriticalSection(&m_rep);
    delete [] color_frame;
    delete [] depth_frame;
    delete [] color_buffer;
    delete [] depth_buffer;
}

DWORD KinectCapturer::StreamEventThread(KinectCapturer* pThis)
{
    HANDLE events[] = {pThis->m_hStopStreamEventThread, pThis->m_hLastFrameEvent};
    while (true)
    {
        DWORD ret = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE);
        if (WAIT_OBJECT_0 == ret)
            break;
        if (WAIT_OBJECT_0 + 1 == ret)
        {
            if (pThis->m_count >= FRAME_NUM)
                break;
            //std::cerr<< "receive the last frame event"<< std::endl;
            ResetEvent(pThis->m_hLastFrameEvent);
            pThis->convertFrameToPointCloud();
        }
    }

    std::cout<< "begin output"<< std::endl;
    int j = 0;
    while (!pThis->qqq.empty()) {
        string fileName(FILE_PREFIX);
        fileName += "skeleton_"+ to_string(static_cast<unsigned long long>(pThis->m_nIndex)) +"\\";
        fileName += to_string(static_cast<unsigned long long>(j));
        fileName += ".json";
        ofstream myfile;
        myfile.open(fileName.c_str());
        myfile << pThis->qqq.front()<< endl;
        myfile.close();

        pThis->qqq.pop();
        j++;
    }

    for (int i = 0; i< FRAME_NUM; i++){
    cv::Mat color_image = cv::Mat(pThis->m_colorHeight,pThis-> m_colorWidth, CV_8UC3,pThis->color_buffer + i * 640*480*3, cv::Mat::AUTO_STEP);
    cv::Mat depth_image = cv::Mat(pThis->m_depthHeight, pThis->m_depthWidth, CV_8UC1, pThis->depth_buffer + i * 640*480, cv::Mat::AUTO_STEP);
    cv::imwrite(string(FILE_PREFIX) + "depth_frame_"+ to_string(static_cast<unsigned long long>(pThis->m_nIndex)) +"\\" + to_string(static_cast<unsigned long long>(i)) + ".jpg", depth_image);
    cv::imwrite(string(FILE_PREFIX) + "color_frame_"+ to_string(static_cast<unsigned long long>(pThis->m_nIndex)) +"\\" + to_string(static_cast<unsigned long long>(i)) + ".jpg", color_image);
    //cout<<i<<endl;
    }
    std::cout<< "done output"<< std::endl;
    return 0;
}

void static write_skeleton_to_json(NUI_SKELETON_DATA* skeleton,Writer<StringBuffer>& w ) {
    w.StartObject();
    w.String("eTrackingState");
    w.Uint(skeleton->eTrackingState);
    w.String("dwTrackingID");
    w.Uint(skeleton->dwTrackingID);
    w.String("dwEnrollmentIndex");
    w.Uint(skeleton->dwEnrollmentIndex);
    w.String("Position");
    w.StartArray();
    w.Double(skeleton->Position.x);
    w.Double(skeleton->Position.y);
    w.Double(skeleton->Position.z);
    w.EndArray();
    w.String("SkeletonPositions");
    w.StartArray();
    for (int i =0; i!=NUI_SKELETON_POSITION_COUNT; i++) {
        w.StartArray();
        w.Double(skeleton->SkeletonPositions[i].x);
        w.Double(skeleton->SkeletonPositions[i].y);
        w.Double(skeleton->SkeletonPositions[i].z);
        w.EndArray();
    }
    w.EndArray();
    w.String("eSkeletonPositionTrackingState");
    w.StartArray();
    for (int i =0; i!=NUI_SKELETON_POSITION_COUNT; i++) {
        w.Uint(skeleton->eSkeletonPositionTrackingState[i]);
    }
    w.EndArray();
    w.EndObject();
}

HRESULT KinectCapturer::convertFrameToPointCloud()
{
    NUI_IMAGE_FRAME imageFrame;
    NUI_IMAGE_FRAME depthFrame;
    NUI_SKELETON_FRAME skeletonFrame;
    HRESULT hr;

    hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
    ERROR_CHECK(hr);
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
    ERROR_CHECK(hr);
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &depthFrame);
    ERROR_CHECK(hr);

    INuiFrameTexture* pTexture;
    BOOL aaa = false;
    hr = m_pNuiSensor->NuiImageFrameGetDepthImagePixelFrameTexture(m_pDepthStreamHandle, &depthFrame, &aaa, &pTexture);
    if (FAILED(hr))
    {
        return hr;
    }


    NUI_LOCKED_RECT LockedColorRect;
    NUI_LOCKED_RECT LockedDepthRect;

    hr = imageFrame.pFrameTexture->LockRect(0, &LockedColorRect, NULL, 0);
    ERROR_CHECK(hr);
    hr = pTexture->LockRect(0, &LockedDepthRect, NULL, 0);
    //hr = depthFrame.pFrameTexture->LockRect(0, &LockedDepthRect, NULL, 0);
    ERROR_CHECK(hr);


    int safeWidth = m_colorWidth - 1, safeHeight = m_colorHeight - 1;
    NUI_DEPTH_IMAGE_PIXEL* depth_data = (NUI_DEPTH_IMAGE_PIXEL*) LockedDepthRect.pBits;

    static const float bad_point = std::numeric_limits<float>::quiet_NaN ();

    for (int i = 0; i != m_depthWidth * m_depthHeight; i++) {
         color_frame[3*i] = 0x00;
         color_frame[3*i + 1] = 0xff;
         color_frame[3*i + 2] = 0x00;
         depth_frame[i] = 0xff;
    }

    NUI_COLOR_IMAGE_POINT *pColorPoints = new NUI_COLOR_IMAGE_POINT[640*480];
    m_pMapper->MapDepthFrameToColorFrame(cDepthResolution, 640*480, depth_data, NUI_IMAGE_TYPE_COLOR, cColorResolution, 640*480, pColorPoints);
    cv::Mat image = cv::Mat(m_colorHeight, m_colorWidth, CV_8UC4, LockedColorRect.pBits, cv::Mat::AUTO_STEP);
    for(int j = 0; j < m_depthHeight; j++) {
        for(int i = 0; i < m_depthWidth; i++, depth_data++) {
            USHORT player = depth_data->playerIndex;
            USHORT depthValue = depth_data->depth;
            if (!( player == 0 ||
                pColorPoints[j*m_depthWidth+i].x < 0 || pColorPoints[j*m_depthWidth+i].x > safeWidth || pColorPoints[j*m_depthWidth+i].y < 0 || pColorPoints[j*m_depthWidth+i].y > safeHeight))

            {
                depth_frame[j*m_depthWidth+i] = (BYTE) ((depthValue-1024)/8);
                cv::Vec4b color = image.at<cv::Vec4b>(pColorPoints[j*m_depthWidth+i].y,pColorPoints[j*m_depthWidth+i].x);
                color_frame[3*(j*m_depthWidth+i)] = color[0];
                color_frame[3*(j*m_depthWidth+i)+1] = color[1];
                color_frame[3*(j*m_depthWidth+i)+2] = color[2];
            }

        }
    }


    hr = imageFrame.pFrameTexture->UnlockRect(0);
    ERROR_CHECK(hr);
    //hr = depthFrame.pFrameTexture->UnlockRect(0);
    hr = pTexture->UnlockRect(0);
    ERROR_CHECK(hr);

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
    ERROR_CHECK(hr);
    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &depthFrame);
    ERROR_CHECK(hr);

    NUI_SKELETON_DATA* pSkeletonData = skeletonFrame.SkeletonData;

    StringBuffer s;
    Writer<StringBuffer> writer(s);
    write_skeleton_to_json(pSkeletonData, writer);
    qqq.push(s.GetString());
    memcpy(color_buffer + m_count * 640*480*3, color_frame, 640*480*3);
    memcpy(depth_buffer + m_count * 640*480, depth_frame, 640*480);
    m_count++;
    return hr;
}




HANDLE KinectCapturer::getPointCloudEvent() const
{
    return m_hPointCloudEvent;
}
