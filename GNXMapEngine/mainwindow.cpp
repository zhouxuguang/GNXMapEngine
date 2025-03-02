#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "MetalWindow.h"
#include "VulkanWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
#ifdef __APPLE__
    QWidget *wrapper = QWidget::createWindowContainer(new MetalWindow(), this);
#else
    QWidget *wrapper = QWidget::createWindowContainer(new VulkanWindow(), this);
#endif

    setCentralWidget(wrapper);
    wrapper->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

