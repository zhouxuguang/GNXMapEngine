#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "metalwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    QWidget *wrapper = QWidget::createWindowContainer(new VulkanWindow(), this);
    setCentralWidget(wrapper);
    wrapper->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

