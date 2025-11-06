#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(400, 200);
    setWindowTitle("System Print Monitor");
    monitor.start();
    addToStartup();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addToStartup()
{
    QString appPath = QCoreApplication::applicationFilePath();
    QString startupDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
                         + "/Microsoft/Windows/Start Menu/Programs/Startup";

    QString linkPath = startupDir + "/PrintMonitor.lnk";

    // Using Windows COM to create a shortcut would be ideal, but in simple form:
    QFile::link(appPath, linkPath);
}

