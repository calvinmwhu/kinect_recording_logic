#include "mainwindow.h"
#include "kinectcapturer.h"
#include <QApplication>
#include <QString>


static float fAspect = 1;

// error callback function

static void error_callback(int error, const char* description)
{
    QString str = QString::number(error);
    str.append(": ");
    str.append(description);
    fputs(str.toStdString().c_str(), stderr);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    int num;
    NuiGetSensorCount(&num);
    std::cout << num << std::endl;
    // create event
    KinectCapturer *m_kinect = new KinectCapturer(0);
    if (m_kinect->connectSensor() != S_OK)
    {
        std::cerr << "kinect 0 initialization failed.";
        return -1;
    }
    KinectCapturer *m_kinect1 = new KinectCapturer(1);
    if (m_kinect1->connectSensor() != S_OK)
    {
        std::cerr << "kinect 1 initialization failed.";
        return -1;
    }

//    w.show();
    delete m_kinect;
    delete m_kinect1;
    return a.exec();
}

