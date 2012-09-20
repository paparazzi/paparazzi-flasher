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

#include "dfu_manager.h"

#include <QDebug>
#include <QFile>

#include "usb.h"
#include "upgrade/dfu.h"
#include "upgrade/stm32mem.h"

#define LOAD_ADDRESS 0x8002000

DFUManager::DFUManager(QObject *parent) :
    QObject(parent)
{
    usb_init();

    timer.setInterval(2000);
    connect(&timer, SIGNAL(timeout()), this, SLOT(findIFace()));
    handle = NULL;

    block_size = 1024;
    flash_size_mutex.lock();
    flash_size = 0x20000; /* 128kb */
    flash_size_mutex.unlock();
}

DFUManager::~DFUManager()
{
    if (handle) {
        qDebug() << "Releasing interface on delete.";
        usb_release_interface(handle, iface);
        usb_close(handle);
        handle = NULL;
    }
}

void DFUManager::start()
{
    findIFace();
    timer.start();
}

void DFUManager::findIFace()
{
    QString *deviceName;

    qDebug() << "Releasing Interface...";
    if (handle) {
        usb_release_interface(handle, iface);
        usb_close(handle);
        handle = NULL;
    }
    qDebug() << "Trying to find DFU devices...";
    if(!(dev = findDev()) || !(handle = getDFUIface(dev, &iface))) {
        qDebug() << "FATAL: No compatible device found!\n";
        emit lostDevice();
        return;
    }

    state = dfu_getstate(handle, iface);
    if((state < 0) || (state == STATE_APP_IDLE)) {
        puts("Resetting device in firmware upgrade mode...");
        dfu_detach(handle, iface, 1000);
        usb_release_interface(handle, iface);
        usb_close(handle);
        handle = NULL;
        emit lostDevice();
        return;
    }

    qDebug() << "Found device at " << dev->bus->dirname << ":" << dev->filename;

    deviceName = new QString("Found device at ");
    deviceName->append(dev->bus->dirname);
    deviceName->append(":");
    deviceName->append(dev->filename);

    emit foundDevice(deviceName);
}

struct usb_device *DFUManager::findDev()
{
    struct usb_bus *bus;
    struct usb_device *dev;
    struct usb_dev_handle *handle;
    char man[40];
    char prod[40];

    usb_find_busses();
    usb_find_devices();

    for(bus = usb_get_busses(); bus; bus = bus->next) {
        for(dev = bus->devices; dev; dev = dev->next) {
            /* Check for ST Microelectronics vendor ID */
            if ((dev->descriptor.idVendor != 0x483) &&
                    (dev->descriptor.idVendor != 0x1d50))
                continue;

            handle = usb_open(dev);
            usb_get_string_simple(handle, dev->descriptor.iManufacturer, man,
                                  sizeof(man));
            usb_get_string_simple(handle, dev->descriptor.iProduct, prod,
                                  sizeof(prod));
#if 0
            printf("%s:%s [%04X:%04X] %s : %s\n", bus->dirname, dev->filename,
                   dev->descriptor.idVendor, dev->descriptor.idProduct, man, prod);
#endif
            usb_close(handle);

            if (((dev->descriptor.idProduct == 0x5740) ||
                 (dev->descriptor.idProduct == 0x6018)) &&
                    !strcmp(man, "Black Sphere Technologies") &&
                    !strcmp(prod, "Black Magic Firmware Upgrade")) {
                block_size = 1024;
                flash_size_mutex.lock();
                flash_size = 0x20000; /* 128kb */
                flash_size_mutex.unlock();
                return dev;
            }

            if (((dev->descriptor.idProduct == 0xDF11) ||
                 (dev->descriptor.idProduct == 0x6017)) &&
                    !strcmp(man, "Black Sphere Technologies") &&
                    !strcmp(prod, "Black Magic Probe (Upgrade)")) {
                block_size = 1024;
                flash_size_mutex.lock();
                flash_size = 0x20000; /* 128kb */
                flash_size_mutex.unlock();
                return dev;
            }

            if (((dev->descriptor.idProduct == 0x600F)) &&
                    !strcmp(man, "Transition Robotics Inc.") &&
                    !strcmp(prod, "Lisa/M (Upgrade) V1.0")) {
                block_size = 2048;
                flash_size_mutex.lock();
                flash_size = 0x40000; /* 256kb */
                flash_size_mutex.unlock();
                return dev;
            }
        }
    }
    return NULL;
}

struct usb_dev_handle *DFUManager::getDFUIface(struct usb_device *dev, uint16_t *interface)
{
    int i, j, k;
    struct usb_config_descriptor *config;
    struct usb_interface_descriptor *iface;

    usb_dev_handle *handle;

    for(i = 0; i < dev->descriptor.bNumConfigurations; i++) {
        config = &dev->config[i];

        for(j = 0; j < config->bNumInterfaces; j++) {
            for(k = 0; k < config->interface[j].num_altsetting; k++) {
                iface = &config->interface[j].altsetting[k];
                if((iface->bInterfaceClass == 0xFE) &&
                        (iface->bInterfaceSubClass = 0x01)) {
                    handle = usb_open(dev);
                    //usb_set_configuration(handle, i);
                    usb_claim_interface(handle, j);
                    //usb_set_altinterface(handle, k);
                    //*interface = j;
                    *interface = iface->bInterfaceNumber;
                    return handle;
                }
            }
        }
    }
    return NULL;
}

void DFUManager::flash(QString *filename)
{
    QFile binfile;
    QByteArray binfile_content;
    int bindata_offset;

    timer.stop();

    binfile.setFileName(*filename);
    if(!binfile.open(QIODevice::ReadOnly)) {
        timer.start();
        qDebug() << "Failed to open file " << *filename;
        /* TODO: need to emit an error here */
        return;
    }

    binfile_content = binfile.readAll();

    //binfile_pointer = binfile_content.data_ptr();

    qDebug() << "Opened file " << *filename << " and read " << binfile_content.size() << " bytes?";

    dfu_makeidle(handle, iface);

    for (bindata_offset = 0; bindata_offset < binfile_content.size(); bindata_offset += block_size) {
        emit flashProgressUpdate((bindata_offset*100)/binfile_content.size());
        if (stm32_mem_erase(handle, iface, LOAD_ADDRESS + bindata_offset) != 0) {
            /* TODO: emit error */

            binfile.close();

            //delete(filename);

            emit finishedFlash();

            timer.start();
        }

        stm32_mem_write(handle, iface, (void*)&binfile_content.data()[bindata_offset], block_size);

    }

    stm32_mem_manifest(handle, iface);

    usb_release_interface(handle, iface);
    usb_close(handle);
    handle = NULL;

    binfile.close();

    delete(filename);

    emit finishedFlash();

    timer.start();
}

int DFUManager::get_flash_size()
{
    int size;

    flash_size_mutex.lock();
    size = flash_size;
    flash_size_mutex.unlock();

    return size;
}
