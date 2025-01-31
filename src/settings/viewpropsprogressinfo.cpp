/*
 * SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "viewpropsprogressinfo.h"

#include "applyviewpropsjob.h"
#include "views/viewproperties.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ViewPropsProgressInfo::ViewPropsProgressInfo(QWidget *parent, const QUrl &dir, const ViewProperties &viewProps)
    : QDialog(parent)
    , m_dir(dir)
    , m_viewProps(nullptr)
    , m_label(nullptr)
    , m_progressBar(nullptr)
    , m_dirSizeJob(nullptr)
    , m_applyViewPropsJob(nullptr)
    , m_timer(nullptr)
{
    const QSize minSize = minimumSize();
    setMinimumSize(QSize(320, minSize.height()));
    setWindowTitle(i18nc("@title:window", "Applying View Properties"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    m_viewProps = new ViewProperties(dir);
    m_viewProps->setDirProperties(viewProps);

    // the view properties are stored by the ApplyViewPropsJob, so prevent
    // that the view properties are saved twice:
    m_viewProps->setAutoSaveEnabled(false);

    auto layout = new QVBoxLayout(this);

    m_label = new QLabel(i18nc("@info:progress", "Counting folders: %1", 0), this);
    layout->addWidget(m_label);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(0);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    layout->addStretch();

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ViewPropsProgressInfo::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ViewPropsProgressInfo::reject);
    layout->addWidget(buttonBox);

    // Use the directory size job to count the number of directories first. This
    // allows to give a progress indication for the user when applying the view
    // properties later.
    m_dirSizeJob = KIO::directorySize(dir);
    connect(m_dirSizeJob, &KIO::DirectorySizeJob::result, this, &ViewPropsProgressInfo::applyViewProperties);

    // The directory size job cannot emit any progress signal, as it is not aware
    // about the total number of directories. Therefor a timer is triggered, which
    // periodically updates the current directory count.
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ViewPropsProgressInfo::updateProgress);
    m_timer->start(300);
}

ViewPropsProgressInfo::~ViewPropsProgressInfo()
{
    delete m_viewProps;
    m_viewProps = nullptr;
}

void ViewPropsProgressInfo::closeEvent(QCloseEvent *event)
{
    m_timer->stop();
    m_applyViewPropsJob = nullptr;
    QDialog::closeEvent(event);
}

void ViewPropsProgressInfo::reject()
{
    if (m_dirSizeJob) {
        m_dirSizeJob->kill();
        m_dirSizeJob = nullptr;
    }

    if (m_applyViewPropsJob) {
        m_applyViewPropsJob->kill();
        m_applyViewPropsJob = nullptr;
    }

    QDialog::reject();
}

void ViewPropsProgressInfo::updateProgress()
{
    if (m_dirSizeJob) {
        const int subdirs = m_dirSizeJob->totalSubdirs();
        m_label->setText(i18nc("@info:progress", "Counting folders: %1", subdirs));
    }

    if (m_applyViewPropsJob) {
        const int progress = m_applyViewPropsJob->progress();
        m_progressBar->setValue(progress);
    }
}

void ViewPropsProgressInfo::applyViewProperties()
{
    if (m_dirSizeJob->error()) {
        return;
    }

    const int subdirs = m_dirSizeJob->totalSubdirs();
    m_label->setText(i18nc("@info:progress", "Folders: %1", subdirs));
    m_progressBar->setMaximum(subdirs);

    m_dirSizeJob = nullptr;

    m_applyViewPropsJob = new ApplyViewPropsJob(m_dir, *m_viewProps);
    connect(m_applyViewPropsJob, &ApplyViewPropsJob::result, this, &ViewPropsProgressInfo::close);
}
