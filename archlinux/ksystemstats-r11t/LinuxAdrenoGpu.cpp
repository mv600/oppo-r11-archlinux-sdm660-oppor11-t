/*
 * SPDX-FileCopyrightText: 2026 R11T Arch Linux port
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "LinuxAdrenoGpu.h"

#include <systemstats/SysFsSensor.h>

LinuxAdrenoGpu::LinuxAdrenoGpu(const QString &id, const QString &name, const QString &devfreqPath)
    : GpuDevice(id, name)
{
    auto usage = new KSysGuard::SysFsSensor(QStringLiteral("usage"),
                                             devfreqPath % QStringLiteral("/gpu_busy_percent"), 0, this);
    m_usageProperty = usage;
    m_usageSensor = usage;

    auto frequency = new KSysGuard::SysFsSensor(QStringLiteral("coreFrequency"),
                                                 devfreqPath % QStringLiteral("/cur_freq"), 0, this);
    frequency->setConvertFunction([](const QByteArray &input) {
        return std::atoll(input) / 1000;
    });
    m_coreFrequencyProperty = frequency;
    m_coreFrequencySensor = frequency;
}

void LinuxAdrenoGpu::update()
{
    m_usageSensor->update();
    m_coreFrequencySensor->update();
}

void LinuxAdrenoGpu::makeSensors()
{
}

#include "moc_LinuxAdrenoGpu.cpp"
