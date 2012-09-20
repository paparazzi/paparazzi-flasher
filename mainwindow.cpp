/*
 * This file is part of the Paparazzi UAV project.
 *
 * Copyright (C) 2012 Piotr Esden-Tempski <piotr@esden.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QErrorMessage>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->lineEdit_Binary->setText(getenv("HOME"));

    ui->progressBar->hide();

    device_valid = false;

    connect(ui->pushButton_Flash, SIGNAL(clicked()), this, SLOT(flash()));

    dfu_manager.moveToThread(&dfu_thread);
    connect(ui->pushButton_Open, SIGNAL(clicked()), this, SLOT(openBinary()));
    connect(&dfu_manager, SIGNAL(foundDevice(QString*)), this, SLOT(foundDevice(QString*)));
    connect(&dfu_manager, SIGNAL(lostDevice()), this, SLOT(lostDevice()));
    connect(&dfu_thread, SIGNAL(started()), &dfu_manager, SLOT(start()));
    connect(&dfu_manager, SIGNAL(finishedFlash()), this, SLOT(finishedFlash()));
    connect(&dfu_manager, SIGNAL(flashProgressUpdate(int)), this, SLOT(onFlashProgressUpdate(int)));
    connect(this, SIGNAL(doFlash(QString*)), &dfu_manager, SLOT(flash(QString*)));

    dfu_thread.start();
}

MainWindow::~MainWindow()
{
    dfu_thread.quit();
    dfu_thread.wait();
    delete ui;
}

void MainWindow::openBinary()
{
    ui->lineEdit_Binary->setText(QFileDialog::getOpenFileName(this, tr("Open Binary"), tr("Binary File (*.bin)")));
}

void MainWindow::foundDevice(QString *string)
{
    QPalette plt;
    QColor color(0, 142, 10);
    plt.setColor(QPalette::WindowText, color);
    ui->label_Iface->setPalette(plt);
    ui->label_Iface->setText(*string);
    ui->pushButton_Flash->setEnabled(true);
    delete(string);
    device_valid = true;
}

void MainWindow::lostDevice()
{
    QPalette plt;
    QColor color(142, 0, 10);
    plt.setColor(QPalette::WindowText, color);
    ui->label_Iface->setPalette(plt);
    ui->label_Iface->setText("No device connected.");
    ui->pushButton_Flash->setEnabled(false);

    device_valid = false;
}

void MainWindow::flash()
{
    QFileInfo binary_info(ui->lineEdit_Binary->text());
    QErrorMessage error_message;

    if (!binary_info.exists()) {
        qDebug() << "File does not exist!";
        error_message.showMessage("The file you selected does not exist!");
        error_message.exec();
        return;
    }

    if (!binary_info.isFile()) {
        qDebug() << "\"File\" is not a file!";
        error_message.showMessage("The \"file\" you selected is not a file!");
        error_message.exec();
        return;
    }

    if (binary_info.size() == 0) {
        qDebug() << "File is empty!";
        error_message.showMessage("The file is empty!");
        error_message.exec();
        return;
    }

    ui->progressBar->setValue(0);
    ui->progressBar->show();

    emit doFlash(new QString(ui->lineEdit_Binary->text()));
}

void MainWindow::finishedFlash()
{
    ui->progressBar->hide();
    ui->label_Iface->setText("Done Flashing!");
}

void MainWindow::onFlashProgressUpdate(int percent)
{
    ui->progressBar->setValue(percent);
}
