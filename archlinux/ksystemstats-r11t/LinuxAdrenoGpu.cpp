/*
 * SPDX-FileCopyrightText: 2026 R11T Arch Linux port
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "LinuxAdrenoGpu.h"

#include <systemstats/SysFsSensor.h>

LinuxAdrenoGpu::LinuxAdrenoGpu(const QString &id, const QString &name, const QString &devfreqPath)
    : GpuDevice(id, name)
    , m_devfreqPath(devfreqPath)
{
}

void LinuxAdrenoGpu::update()
{
    m_usageSensor->update();
    m_coreFrequencySensor->update();
}

void LinuxAdrenoGpu::makeSensors()
{
    // GpuDevice::initialize() expects the complete property set, including the
    // sensors that this platform GPU cannot provide.
    GpuDevice::makeSensors();

    m_usageProperty = new KSysGuard::SysFsSensor(QStringLiteral("usage"),
                                                 m_devfreqPath % QStringLiteral("/gpu_busy_percent"), 0, this);
    m_usageSensor = m_usageProperty;

    auto frequency = new KSysGuard::SysFsSensor(QStringLiteral("coreFrequency"),
                                                 m_devfreqPath % QStringLiteral("/cur_freq"), 0, this);
    frequency->setConvertFunction([](const QByteArray &input) {
        return std::atoll(input) / 1000;
    });
    m_coreFrequencyProperty = frequency;
    m_coreFrequencySensor = frequency;
}

#include "moc_LinuxAdrenoGpu.cpp"
