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

#ifndef DFU_MANAGER_H
#define DFU_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>

#include "upgrade/dfu.h"

class DFUManager : public QObject
{
    Q_OBJECT
public:
    explicit DFUManager(QObject *parent = 0);
    ~DFUManager();
    int get_flash_size();

private:
    QTimer timer;
    struct usb_device *dev;
    struct usb_dev_handle *handle;
    uint16_t iface;
    int state;
    int block_size;
    QMutex flash_size_mutex;
    int flash_size;

    struct usb_device *findDev(void);
    struct usb_dev_handle *getDFUIface(struct usb_device *dev, uint16_t *interface);

signals:
    void foundDevice(QString *string);
    void lostDevice();
    void finishedFlash();
    void flashProgressUpdate(int address, int percentage);
    
private slots:
    void findIFace();
    void start();
    void flash(QString *filename);

public slots:
    
};

#endif // DFU_MANAGER_H
