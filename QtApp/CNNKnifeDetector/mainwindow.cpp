#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QScrollBar>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /*--------configuration--------*/

    mode = 0;
    QString commonPath = "/Users/tianxin/Programming/DeepLearning/YOLOv3/";
    strCfgFile = commonPath + "cfg/yolov2-tiny-knife.cfg";
    strCNNModel = commonPath + "models/yolov2-tiny-knife_10000.weights";
    strClassNames = commonPath + "data/knife-tiny/knife-tiny.names";
    threshold = 0.1;
    camera = 0;

    /*--------init widgets--------*/

    ui->comboBoxMode->setCurrentIndex(0); // Webcam's index is 0
    ui->leCfgFile->setText(strCfgFile);
    ui->leCNNModel->setText(strCNNModel);
    ui->leClassNames->setText(strClassNames);
    ui->leThreshold->setText(QString("%1").arg(threshold));
    ui->leCamera->setText(QString("%1").arg(camera));
    ui->teDetectionLog->setReadOnly(true);
    ui->teDetectionLog->setTextColor(Qt::red);
    ui->teOperationLog->setReadOnly(true);

    // set validator for threshold
    thresholdValidator = new QDoubleValidator(0, 1, 2, ui->leThreshold);
    thresholdValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->leThreshold->setValidator(thresholdValidator);

    // set validator for camera device
    cameraValidator = new QIntValidator(ui->leCamera);
    ui->leCamera->setValidator(cameraValidator);

    // disable widgets according to the state of this program.
    state = STOPPED;
    disableAndEnableWidgets();

    /*--------set up things about detection and capturing frames--------*/

    // set up timer and slot
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(processOneFrame()));
    alertTimer = new QTimer(this);
    connect(alertTimer, SIGNAL(timeout()), SLOT(doAlert()));

    // folder to store captures
    strCapturesFolder = "/Users/tianxin/CNNKnifeDetecorCaptures/";
}

MainWindow::~MainWindow()
{
    delete ui;
}


/*--------------------------------------------------------------------------------------------*/
/*--------helper functions--------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/

bool MainWindow::fileExists(QString path)
{
    QFileInfo check_file(path);
    if (check_file.exists() && check_file.isFile())
        return true;
    return false;
}

bool MainWindow::checkConfig()
{
    QString filename;
    QString errorMsg = "File does not exist:";
    bool errorOccurred = false;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setModal(true);

    // check if .cfg file exists
    filename = ui->leCfgFile->text();
    if (fileExists(filename)) {
        strCfgFile = filename;
    } else {
        errorOccurred = true;
        errorMsg = errorMsg + "\n\t" + filename;
    }

    // check if .weights file exists
    filename = ui->leCNNModel->text();
    if (fileExists(filename)) {
        strCNNModel = filename;
    } else {
        errorOccurred = true;
        errorMsg = errorMsg + "\n\t" + filename;
    }

    // check if .names file exists
    filename = ui->leClassNames->text();
    if (fileExists(filename)) {
        strClassNames = filename;
    } else {
        errorOccurred = true;
        errorMsg = errorMsg + "\n\t" + filename;
    }

    // Check if video file exits
    filename = ui->leClassNames->text();
    if (mode == 1 && !fileExists(filename)) {
        errorOccurred = true;
        errorMsg = errorMsg + "\n\t" + filename;
    }

    if (errorOccurred) {
        msgBox.setText(errorMsg);
        msgBox.exec();
        return false;
    }

    return true;
}

void MainWindow::disableAndEnableWidgets()
{
    if (state == DETECTING) {
        // settings
        ui->comboBoxMode->setEnabled(false);
        ui->btnSettingCfg->setEnabled(false);
        ui->leCfgFile->setEnabled(false);
        ui->btnSettingModel->setEnabled(false);
        ui->leCNNModel->setEnabled(false);
        ui->btnSettingNames->setEnabled(false);
        ui->leClassNames->setEnabled(false);
        ui->leThreshold->setEnabled(false);
        ui->btnSettingCamTest->setEnabled(false);
        ui->leCamera->setEnabled(false);
        ui->btnSettingVideoFile->setEnabled(false);
        ui->leVideoFile->setEnabled(false);
        // controls
        ui->btnCtlStart->setEnabled(false);
        ui->btnCtlStop->setEnabled(true);
        ui->btnCtlCapture->setEnabled(true);
    } else {
        // settings
        ui->comboBoxMode->setEnabled(true);
        ui->btnSettingCfg->setEnabled(true);
        ui->leCfgFile->setEnabled(true);
        ui->btnSettingModel->setEnabled(true);
        ui->leCNNModel->setEnabled(true);
        ui->btnSettingNames->setEnabled(true);
        ui->leClassNames->setEnabled(true);
        ui->leThreshold->setEnabled(true);
        ui->btnSettingCamTest->setEnabled(true);
        ui->leCamera->setEnabled(true);
        ui->btnSettingVideoFile->setEnabled(true);
        ui->leVideoFile->setEnabled(true);
        // controls
        ui->btnCtlStart->setEnabled(true);
        ui->btnCtlStop->setEnabled(false);
        ui->btnCtlCapture->setEnabled(false);
    }
    repaint();
}

void MainWindow::on_btnSettingCamTest_clicked()
{
    cv::VideoCapture testCap = cv::VideoCapture(camera);

    if (testCap.isOpened()) {
        msgBox.setIcon(QMessageBox::Information);
        msg = "Camera: " + QString("%1 seems the best").arg(camera);
    } else {
        msgBox.setIcon(QMessageBox::Warning);
        msg = "Oops! Couldn't find camera: " + QString("%1").arg(camera);
    }

    msgBox.setText(msg);
    msgBox.exec();
}


/*--------------------------------------------------------------------------------------------*/
/*--------functions to handle detection, start, stop and capture------------------------------*/
/*--------------------------------------------------------------------------------------------*/

void MainWindow::on_btnCtlStart_clicked()
{
    // first check configuration
    if (checkConfig() && startDetection()) {
        ui->teOperationLog->append("["
                                   + QDateTime::currentDateTime().toString("hh:mm:ss")
                                   + "]"
                                   + " Detection started!");
        state = DETECTING;
        disableAndEnableWidgets();
    } else {
        ui->teOperationLog->append("["
                                   + QDateTime::currentDateTime().toString("hh:mm:ss")
                                   + "]"
                                   + " Failed to start the detection!");
    }
}

/*
** Functions startDetection() and  processOneFrame() derive from the
** sample code yolo_object_detection.cpp in OpenCV 3.4 written by
** github user @AlexeyAB. I changed a few things so it can work with Qt.
**
** AlexeyAB's github: https://github.com/AlexeyAB
**
** Thanks for his help.
*/

bool MainWindow::startDetection()
{
    //! [Initialize network]
    modelConfiguration = strCfgFile.toStdString().c_str();
    modelBinary = strCNNModel.toStdString().c_str();
    net = cv::dnn::readNetFromDarknet(modelConfiguration, modelBinary);
    //! [Initialize network]

    if (net.empty())
    {
        msg += "Can't load network by using the following files:\n";
        msg += "cfg-file:     " + strCfgFile + "\n";
        msg += "weights-file: " + strCNNModel + "\n";
        msg += "Pretrained models can be downloaded here:\n";
        msg += "https://pjreddie.com/darknet/yolo/";
        msgBox.setText(msg);
        msgBox.exec();
        return false;
    }

    source = strVideoFile.toStdString().c_str(); // set source

    if (mode == 0)  // mode 0 uses a webcam
    {
        cap = cv::VideoCapture(camera); // open camera
        if(!cap.isOpened())
        {
            msg = "Couldn't find camera: " + QString("%1\n").arg(camera);
            msgBox.setText(msg);
            msgBox.exec();
            return false;
        }
    }
    else // mode 1 uses a video file
    {
        source = strVideoFile.toStdString().c_str();
        cap.open(source);
        if (!cap.isOpened())
        {
            msg = "Couldn't open video: " + strVideoFile;
            msgBox.setText(msg);
            msgBox.exec();
            return false;
        }
    }

    // load class names
    std::ifstream classNamesFile(strClassNames.toStdString().c_str());
    if (classNamesFile.is_open())
    {
        std::string className = "";
        while (std::getline(classNamesFile, className))
            classNamesVec.push_back(className);
    }

    // start detection
    timer->start(33);        // 1000ms / 30fps = 33
    alertTimer->start(1000); // 1s

    return true;
}

void MainWindow::processOneFrame()
{
    cv::Mat frame;
    cap >> frame; // get a new frame from camera/video or read image

    if (frame.empty())
        return;

    if (frame.channels() == 4)
        cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);

    //! [Prepare blob]
    cv::Mat inputBlob = cv::dnn::blobFromImage(frame, 1 / 255.F,
                                      cv::Size(416, 416),
                                      cv::Scalar(), true, false); //Convert Mat to batch of images
    //! [Prepare blob]

    //! [Set input blob]
    net.setInput(inputBlob, "data");                   //set the network input
    //! [Set input blob]

    //! [Make forward pass]
    cv::Mat detectionMat = net.forward("detection_out");   //compute output
    //! [Make forward pass]

    std::vector<double> layersTimings;
    double freq = cv::getTickFrequency() / 1000;
    double time = net.getPerfProfile(layersTimings) / freq;
    std::ostringstream ss;
    ss << "FPS: " << 1000/time << " ; time: " << time << " ms";
    cv::putText(frame, ss.str(), cv::Point(20,20), 0, 0.5, cv::Scalar(0,0,255));

    float confidenceThreshold = threshold;
    for (int i = 0; i < detectionMat.rows; i++)
    {
        const int probability_index = 5;
        const int probability_size = detectionMat.cols - probability_index;
        float *prob_array_ptr = &detectionMat.at<float>(i, probability_index);

        size_t objectClass = std::max_element(prob_array_ptr, prob_array_ptr + probability_size) - prob_array_ptr;
        float confidence = detectionMat.at<float>(i, (int)objectClass + probability_index);

        if (confidence > confidenceThreshold)
        {
            knifeDetected = true;
            float x = detectionMat.at<float>(i, 0);
            float y = detectionMat.at<float>(i, 1);
            float width = detectionMat.at<float>(i, 2);
            float height = detectionMat.at<float>(i, 3);
            int xLeftBottom = static_cast<int>((x - width / 2) * frame.cols);
            int yLeftBottom = static_cast<int>((y - height / 2) * frame.rows);
            int xRightTop = static_cast<int>((x + width / 2) * frame.cols);
            int yRightTop = static_cast<int>((y + height / 2) * frame.rows);

            cv::Rect object(xLeftBottom, yLeftBottom,
                        xRightTop - xLeftBottom,
                        yRightTop - yLeftBottom);

            cv::rectangle(frame, object, cv::Scalar(0, 255, 0));

            if (objectClass < classNamesVec.size())
            {
                ss.str("");
                ss << confidence;
                cv::String conf(ss.str());
                cv::String label = cv::String(classNamesVec[objectClass]) + ": " + conf;
                int baseLine = 0;
                cv::Size labelSize = getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
                cv::rectangle(frame, cv::Rect(cv::Point(xLeftBottom, yLeftBottom ),
                                      cv::Size(labelSize.width, labelSize.height + baseLine)),
                          cv::Scalar(255, 255, 255), CV_FILLED);
                cv::putText(frame, label, cv::Point(xLeftBottom, yLeftBottom+labelSize.height),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,0,0));
            }
        }
    }

    // show frame on an image lable
    cv::cvtColor(frame,frame,CV_BGR2RGB);
    qFrame = QImage((uchar*)frame.data,
                    frame.cols, frame.rows, frame.step,
                    QImage::Format_RGB888);
    ui->labelImage->setPixmap(QPixmap::fromImage(qFrame));
}

void MainWindow::doAlert()
{
    if (knifeDetected) {
        ui->teDetectionLog->append("["
                                   + QDateTime::currentDateTime().toString("hh:mm:ss")
                                   + "]"
                                   + " Detected knives!!!");
        ui->teDetectionLog->verticalScrollBar()->setValue(
                    ui->teDetectionLog->verticalScrollBar()->maximumHeight());
        ui->teDetectionLog->setStyleSheet("#teDetectionLog { border: 2px solid red; }");
        knifeDetected = false;
    } else {
        ui->teDetectionLog->setStyleSheet("#teDetectionLog { border: 1px solid black; }");
    }
    //repaint();
}

void MainWindow::on_btnCtlStop_clicked()
{
    ui->teOperationLog->append("["
                               + QDateTime::currentDateTime().toString("hh:mm:ss")
                               + "]"
                               + " Detection stopped!");

    cap.release(); // release video capturing
    timer->stop();
    alertTimer->stop();
    state = STOPPED;
    disableAndEnableWidgets();
}


void MainWindow::on_btnCtlCapture_clicked()
{
    // make sure that there is a folder to hold captures
    QDir captures;
    if (!captures.exists(strCapturesFolder)) {
        captures.mkpath(strCapturesFolder);
    }

    QString filename = strCapturesFolder
            + "capture_"
            + QDateTime::currentDateTime().toString("yyyy_MM_hh_mm_ss")
            + ".png";
    if (qFrame.save(filename)) {
        ui->teOperationLog->append("["
                                   + QDateTime::currentDateTime().toString("hh:mm:ss")
                                   + "]"
                                   + " Capture saved to " + filename);
        repaint();
    }
}


/*--------------------------------------------------------------------------------------------*/
/*--------functions to get configuratoin files------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/

QString MainWindow::chooseFile(QString fileType, QString filter)
{
    return QFileDialog::getOpenFileName(this, "Choose " + fileType + "File",
                                        "/Users/tianxin/Programming/DeepLearning/YOLOv3/", filter);
}

void MainWindow::on_btnSettingCfg_clicked()
{
    ui->leCfgFile->setText(chooseFile("Cfg", "CfgFile (*.cfg)"));
}

void MainWindow::on_btnSettingModel_clicked()
{
    ui->leCNNModel->setText(chooseFile("Model Weigths", "ModelWeightsFile (*.weights)"));
}

void MainWindow::on_btnSettingNames_clicked()
{
    ui->leClassNames->setText(chooseFile("Class Names", "ClassNamesFile (*.names)"));
}


void MainWindow::on_btnSettingVideoFile_clicked()
{
    ui->leVideoFile->setText(chooseFile("Video", "VideoFile (*.mp4)"));
}


/*--------------------------------------------------------------------------------------------*/
/*--------functions to handle xxx_textChanged signals of lineEdits----------------------------*/
/*--------------------------------------------------------------------------------------------*/

void MainWindow::on_leCfgFile_textChanged(const QString &arg1)
{
    // update
    strCfgFile = arg1;
}

void MainWindow::on_leCNNModel_textChanged(const QString &arg1)
{
    // update
    strCNNModel = arg1;
}

void MainWindow::on_leClassNames_textChanged(const QString &arg1)
{
    // update
    strClassNames = arg1;
}

void MainWindow::on_leThreshold_textChanged(const QString &arg1)
{
    double th = arg1.toDouble();
    if (th > 1) {
        th = 1;
        ui->leThreshold->setText("1");
    } else if (th < 0) {
        th = 0;
        ui->leThreshold->setText("0");
    }

    // update
    threshold = th;
}

void MainWindow::on_leCamera_textChanged(const QString &arg1)
{
    // update
    camera = arg1.toInt();
}

void MainWindow::on_leVideoFile_textChanged(const QString &arg1)
{
    strVideoFile = arg1;
}

/*--------------------------------------------------------------------------------------------*/
/*--------Update current mode (using webcam or local video file?)-----------------------------*/
/*--------------------------------------------------------------------------------------------*/

void MainWindow::on_comboBoxMode_currentIndexChanged(int index)
{
    mode = index;
}
