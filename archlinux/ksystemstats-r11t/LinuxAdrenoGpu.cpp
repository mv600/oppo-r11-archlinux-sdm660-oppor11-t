/*
 * SPDX-FileCopyrightText: 2026 R11T Arch Linux port
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "LinuxAdrenoGpu.h"

#include <QFile>

LinuxAdrenoGpu::LinuxAdrenoGpu(const QString &id, const QString &name, const QString &devfreqPath)
    : GpuDevice(id, name)
    , m_devfreqPath(devfreqPath)
{
}

void LinuxAdrenoGpu::update()
{
    QFile busyPercent(m_devfreqPath % QStringLiteral("/gpu_busy_percent"));
    if (busyPercent.open(QIODevice::ReadOnly)) {
        m_usageProperty->setValue(busyPercent.readAll().trimmed().toInt());
    }

    QFile currentFrequency(m_devfreqPath % QStringLiteral("/cur_freq"));
    if (currentFrequency.open(QIODevice::ReadOnly)) {
        m_coreFrequencyProperty->setValue(currentFrequency.readAll().trimmed().toLongLong() / 1000);
    }
}

void LinuxAdrenoGpu::makeSensors()
{
    // GpuDevice::initialize() expects the complete property set, including the
    // sensors that this platform GPU cannot provide.
    GpuDevice::makeSensors();
}

#include "moc_LinuxAdrenoGpu.cpp"
