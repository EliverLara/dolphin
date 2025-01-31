/*
 * SPDX-FileCopyrightText: 2011 Peter Penz <peter.penz19@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemlistviewanimation.h"
#include "kitemviews/kitemlistview.h"

#include <QPropertyAnimation>

KItemListViewAnimation::KItemListViewAnimation(QObject *parent)
    : QObject(parent)
    , m_scrollOrientation(Qt::Vertical)
    , m_scrollOffset(0)
    , m_animation()
{
}

KItemListViewAnimation::~KItemListViewAnimation()
{
    for (int type = 0; type < AnimationTypeCount; ++type) {
        qDeleteAll(m_animation[type]);
    }
}

void KItemListViewAnimation::setScrollOrientation(Qt::Orientation orientation)
{
    m_scrollOrientation = orientation;
}

Qt::Orientation KItemListViewAnimation::scrollOrientation() const
{
    return m_scrollOrientation;
}

void KItemListViewAnimation::setScrollOffset(qreal offset)
{
    const qreal diff = m_scrollOffset - offset;
    m_scrollOffset = offset;

    // The change of the offset requires that the position of all
    // animated QGraphicsWidgets get adjusted. An exception is made
    // for the delete animation that should just fade away on the
    // existing position.
    for (int type = 0; type < AnimationTypeCount; ++type) {
        if (type == DeleteAnimation) {
            continue;
        }

        QHashIterator<QGraphicsWidget *, QPropertyAnimation *> it(m_animation[type]);
        while (it.hasNext()) {
            it.next();

            QGraphicsWidget *widget = it.key();
            QPropertyAnimation *propertyAnim = it.value();

            QPointF currentPos = widget->pos();
            if (m_scrollOrientation == Qt::Vertical) {
                currentPos.ry() += diff;
            } else {
                currentPos.rx() += diff;
            }

            if (type == MovingAnimation) {
                // Stop the animation, calculate the moved start- and end-value
                // and restart the animation for the remaining duration.
                const int remainingDuration = propertyAnim->duration() - propertyAnim->currentTime();

                const bool block = propertyAnim->signalsBlocked();
                propertyAnim->blockSignals(true);
                propertyAnim->stop();

                QPointF endPos = propertyAnim->endValue().toPointF();
                if (m_scrollOrientation == Qt::Vertical) {
                    endPos.ry() += diff;
                } else {
                    endPos.rx() += diff;
                }

                propertyAnim->setDuration(remainingDuration);
                propertyAnim->setStartValue(currentPos);
                propertyAnim->setEndValue(endPos);
                propertyAnim->start();
                propertyAnim->blockSignals(block);
            } else {
                widget->setPos(currentPos);
            }
        }
    }
}

qreal KItemListViewAnimation::scrollOffset() const
{
    return m_scrollOffset;
}

void KItemListViewAnimation::start(QGraphicsWidget *widget, AnimationType type, const QVariant &endValue)
{
    stop(widget, type);

    QPropertyAnimation *propertyAnim = nullptr;
    const int animationDuration = widget->style()->styleHint(QStyle::SH_Widget_Animate) ? 200 : 1;

    switch (type) {
    case MovingAnimation: {
        const QPointF newPos = endValue.toPointF();
        if (newPos == widget->pos()) {
            return;
        }

        propertyAnim = new QPropertyAnimation(widget, "pos");
        propertyAnim->setDuration(animationDuration);
        propertyAnim->setEndValue(newPos);
        break;
    }

    case CreateAnimation: {
        propertyAnim = new QPropertyAnimation(widget, "opacity");
        propertyAnim->setEasingCurve(QEasingCurve::InQuart);
        propertyAnim->setDuration(animationDuration);
        propertyAnim->setStartValue(0.0);
        propertyAnim->setEndValue(1.0);
        break;
    }

    case DeleteAnimation: {
        propertyAnim = new QPropertyAnimation(widget, "opacity");
        propertyAnim->setEasingCurve(QEasingCurve::OutQuart);
        propertyAnim->setDuration(animationDuration);
        propertyAnim->setStartValue(1.0);
        propertyAnim->setEndValue(0.0);
        break;
    }

    case ResizeAnimation: {
        const QSizeF newSize = endValue.toSizeF();
        if (newSize == widget->size()) {
            return;
        }

        propertyAnim = new QPropertyAnimation(widget, "size");
        propertyAnim->setDuration(animationDuration);
        propertyAnim->setEndValue(newSize);
        break;
    }

    case IconResizeAnimation: {
        propertyAnim = new QPropertyAnimation(widget, QByteArrayLiteral("iconSize"));
        propertyAnim->setDuration(animationDuration);
        propertyAnim->setEndValue(endValue);
        break;
    }

    default:
        Q_UNREACHABLE();
        break;
    }

    Q_ASSERT(propertyAnim);
    connect(propertyAnim, &QPropertyAnimation::finished, this, &KItemListViewAnimation::slotFinished);
    m_animation[type].insert(widget, propertyAnim);

    propertyAnim->start();
}

void KItemListViewAnimation::stop(QGraphicsWidget *widget, AnimationType type)
{
    QPropertyAnimation *propertyAnim = m_animation[type].value(widget);
    if (propertyAnim) {
        propertyAnim->stop();

        switch (type) {
        case MovingAnimation:
            break;
        case CreateAnimation:
            widget->setOpacity(1.0);
            break;
        case DeleteAnimation:
            widget->setOpacity(0.0);
            break;
        case ResizeAnimation:
            break;
        default:
            break;
        }

        m_animation[type].remove(widget);
        delete propertyAnim;

        Q_EMIT finished(widget, type);
    }
}

void KItemListViewAnimation::stop(QGraphicsWidget *widget)
{
    for (int type = 0; type < AnimationTypeCount; ++type) {
        stop(widget, static_cast<AnimationType>(type));
    }
}

bool KItemListViewAnimation::isStarted(QGraphicsWidget *widget, AnimationType type) const
{
    return m_animation[type].value(widget);
}

bool KItemListViewAnimation::isStarted(QGraphicsWidget *widget) const
{
    for (int type = 0; type < AnimationTypeCount; ++type) {
        if (isStarted(widget, static_cast<AnimationType>(type))) {
            return true;
        }
    }
    return false;
}

void KItemListViewAnimation::slotFinished()
{
    QPropertyAnimation *finishedAnim = qobject_cast<QPropertyAnimation *>(sender());
    for (int type = 0; type < AnimationTypeCount; ++type) {
        QMutableHashIterator<QGraphicsWidget *, QPropertyAnimation *> it(m_animation[type]);
        while (it.hasNext()) {
            it.next();
            QPropertyAnimation *propertyAnim = it.value();
            if (propertyAnim == finishedAnim) {
                QGraphicsWidget *widget = it.key();
                it.remove();
                finishedAnim->deleteLater();

                Q_EMIT finished(widget, static_cast<AnimationType>(type));
                return;
            }
        }
    }
    Q_ASSERT(false);
}
