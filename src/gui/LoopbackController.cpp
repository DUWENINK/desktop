/*
 * Copyright (C) 2018 by AMCO
 * Copyright (C) 2018 by Jesús Deloya <jdeloya_ext@amco.mx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QMessageBox>
#include <QApplication>

#include "LoopbackController.h"
#include "LoopbackFS.h"

#include <AvailabilityMacros.h>


void LoopbackController::mountFailed(QVariantMap userInfo)
{
    qDebug() << "Got mountFailed notification.";
    
    qDebug() << "kGMUserFileSystem Error code: " << userInfo.value("code") << ", userInfo=" << userInfo.value("localizedDescription");
    
    QMessageBox alert;
    alert.setText(userInfo.contains("localizedDescription")?userInfo.value("localizedDescription").toString() : "Unknown error");
    alert.exec();
}

void LoopbackController::didMount(QVariantMap userInfo)
{
    qDebug() << "Got didMount notification.";
    
    QString mountPath = userInfo.value(kGMUserFileSystemMountPathKey).toString();
    QMessageBox alert;
    alert.setText(tr(QString("Mounted at: %1").arg(mountPath).toLatin1().data()));
    alert.exec();
}

void LoopbackController::didUnmount(QVariantMap userInfo) {
    qDebug() << "Got didUnmount notification.";
    
    QApplication::quit();
}

void LoopbackController::unmount()
{
    fs_->unmount();
}

void LoopbackController::slotquotaUpdated(qint64 total, qint64 used)
{
    fs_->setTotalQuota(total);
    fs_->setUsedQuota(used);
}

LoopbackController::LoopbackController(QString rootPath, QString mountPath, OCC::AccountState *accountState, QObject *parent):QObject(parent), fs_(new LoopbackFS(rootPath, false, accountState, this))
{
    qi_ = new OCC::QuotaInfo(accountState, this);
    
    connect(qi_, &OCC::QuotaInfo::quotaUpdated, this, &LoopbackController::slotquotaUpdated);
    connect(fs_.data(), &LoopbackFS::FuseFileSystemDidMount, this, didMount);
    connect(fs_.data(), &LoopbackFS::FuseFileSystemMountFailed, this, mountFailed);
    connect(fs_.data(), &LoopbackFS::FuseFileSystemDidUnmount, this, didUnmount);
    
    qi_->setActive(true);
    
    QStringList options;
    
    QFileInfo icons(QCoreApplication::applicationDirPath() + "/../Resources/LoopbackFS.icns");
    QString volArg = QString("volicon=%1").arg(icons.canonicalFilePath());
    
    options.append(volArg);
    
    // Do not use the 'native_xattr' mount-time option unless the underlying
    // file system supports native extended attributes. Typically, the user
    // would be mounting an HFS+ directory through LoopbackFS, so we do want
    // this option in that case.
    options.append("native_xattr");
    options.append("auto_cache");
    options.append("volname=LoopbackFS");
    fs_->mountAtPath(mountPath, options);
}

/*LoopbackController::~LoopbackController()
{
    //fs_->unmount();
}*/