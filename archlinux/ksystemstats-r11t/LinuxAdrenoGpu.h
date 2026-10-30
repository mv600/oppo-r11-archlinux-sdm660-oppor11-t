/*
 * SPDX-FileCopyrightText: 2026 R11T Arch Linux port
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "GpuDevice.h"

namespace KSysGuard
{
class SysFsSensor;
}

class LinuxAdrenoGpu : public GpuDevice
{
    Q_OBJECT

public:
    LinuxAdrenoGpu(const QString &id, const QString &name, const QString &devfreqPath);
    ~LinuxAdrenoGpu() override = default;

    void update() override;

protected:
    void makeSensors() override;

private:
    QString m_devfreqPath;
    KSysGuard::SysFsSensor *m_usageSensor = nullptr;
    KSysGuard::SysFsSensor *m_coreFrequencySensor = nullptr;
};
