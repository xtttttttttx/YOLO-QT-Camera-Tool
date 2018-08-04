#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QMessageBox>
#include <QImage>
#include <QTimer>

#include <opencv2/dnn.hpp>
#include <opencv2/dnn/shape_utils.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <string>
#include <vector>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_leThreshold_textChanged(const QString &arg1);
    void on_leCfgFile_textChanged(const QString &arg1);
    void on_leCNNModel_textChanged(const QString &arg1);
    void on_leClassNames_textChanged(const QString &arg1);
    void on_leCamera_textChanged(const QString &arg1);
    void on_leVideoFile_textChanged(const QString &arg1);
    void on_btnSettingCfg_clicked();
    void on_btnSettingModel_clicked();
    void on_btnSettingNames_clicked();
    void on_btnCtlStart_clicked();
    void on_btnCtlStop_clicked();
    void on_btnSettingVideoFile_clicked();
    void on_btnSettingCamTest_clicked();
    void on_btnCtlCapture_clicked();
    void on_comboBoxMode_currentIndexChanged(int index);
    void processOneFrame();
    void doAlert();

private:
    Ui::MainWindow *ui;

    // configuration for widgets
    int mode; // 0 for webcam, 1 for local video
    QString strCfgFile;
    QString strCNNModel;
    QString strClassNames;
    double threshold;
    int camera;
    QString strVideoFile;

    // darknet
    cv::dnn::Net net;
    cv::String modelConfiguration;
    cv::String modelBinary;
    cv::String source;
    std::vector<std::string> classNamesVec;

    // hold messages
    QString msg;

    // process frames
    QImage qFrame;
    QTimer *timer;
    QTimer *alertTimer;
    bool knifeDetected;
    cv::VideoCapture cap;

    // folder to store captures
    QString strCapturesFolder;

    // status
    enum State {DETECTING, STOPPED};
    State state;

    // validators
    QDoubleValidator *thresholdValidator;
    QIntValidator *cameraValidator;

    // message boxes
    QMessageBox msgBox;

    // helper functions
    bool fileExists(QString path);
    bool checkConfig();
    void disableAndEnableWidgets();
    QString chooseFile(QString chooseFile, QString filter);
    bool startDetection();
};

#endif // MAINWINDOW_H
