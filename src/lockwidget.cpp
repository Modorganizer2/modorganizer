#include "lockwidget.h"
#include "mainwindow.h"
#include <QGraphicsDropShadowEffect>
#include <QMenuBar>
#include <QStatusBar>

QWidget* createTransparentWidget(QWidget* parent=nullptr)
{
  auto* w = new QWidget(parent);

  w->setWindowOpacity(0);
  w->setAttribute(Qt::WA_NoSystemBackground);
  w->setAttribute(Qt::WA_TranslucentBackground);

  return w;
}


LockWidget::LockWidget(QWidget* parent, Reasons reason) :
  m_parent(parent), m_overlay(nullptr), m_info(nullptr), m_result(NoResult),
  m_filter(nullptr)
{
  if (reason != NoReason) {
    lock(reason);
  }
}

LockWidget::~LockWidget()
{
  unlock();
}

void LockWidget::lock(Reasons reason)
{
  m_result = StillLocked;
  createUi(reason);
}

void LockWidget::unlock()
{
  m_overlay.reset();

  if (m_filter && m_parent) {
    m_parent->removeEventFilter(m_filter.get());
  }

  enableAll();
}

void LockWidget::setInfo(DWORD pid, const QString& name)
{
  m_info->setText(QString("%1 (%2)").arg(name).arg(pid));
}

LockWidget::Results LockWidget::result() const
{
  return m_result;
}

void LockWidget::createUi(Reasons reason)
{
  QWidget* overlayTarget = m_parent;
  if (auto* w = qApp->activeWindow()) {
    overlayTarget = w;
  }

  if (overlayTarget) {
    m_overlay.reset(createTransparentWidget(overlayTarget));
    m_overlay->setWindowFlags(m_overlay->windowFlags() & Qt::FramelessWindowHint);
    m_overlay->setGeometry(overlayTarget->rect());
  } else {
    m_overlay.reset(new QDialog);
  }

  auto* center = new QFrame;

  if (overlayTarget) {
    center->setFrameStyle(QFrame::StyledPanel);
    center->setLineWidth(1);
    center->setAutoFillBackground(true);

    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(50);
    shadow->setOffset(0);
    shadow->setColor(QColor(0, 0, 0, 100));
    center->setGraphicsEffect(shadow);
  }

  m_info = new QLabel(" ");
  m_info->setAlignment(Qt::AlignCenter | Qt::AlignHCenter);

  auto* ly = new QVBoxLayout(center);

  if (!overlayTarget) {
    ly->setContentsMargins(0, 0, 0, 0);
  }

  auto* message = new QLabel;
  ly->addWidget(message);
  ly->addWidget(m_info);

  auto* buttons = new QWidget;
  auto* buttonsLayout = new QHBoxLayout(buttons);
  ly->addWidget(buttons);

  switch (reason)
  {
    case LockUI:
    {
      QString s;

      if (!m_parent) {
        s = QObject::tr("Mod Organizer is currently running an application.");
      } else {
        s = QObject::tr(
          "Mod Organizer is locked while the application is running.");
      }

      message->setText(s);

      auto* unlockButton = new QPushButton(QObject::tr("Unlock"));
      QObject::connect(unlockButton, &QPushButton::clicked, [&]{ onForceUnlock(); });
      buttonsLayout->addWidget(unlockButton);

      break;
    }

    case OutputRequired:
    {
      message->setText(QObject::tr(
        "The application must run to completion because its output is "
        "required."));

      auto* unlockButton = new QPushButton(QObject::tr("Unlock"));
      QObject::connect(unlockButton, &QPushButton::clicked, [&]{ onForceUnlock(); });
      buttonsLayout->addWidget(unlockButton);

      break;
    }

    case PreventExit:
    {
      message->setText(QObject::tr(
        "Mod Organizer is waiting on application to close before exiting."));

      auto* exit = new QPushButton(QObject::tr("Exit Now"));
      QObject::connect(exit, &QPushButton::clicked, [&]{ onForceUnlock(); });
      buttonsLayout->addWidget(exit);

      auto* cancel = new QPushButton(QObject::tr("Cancel"));
      QObject::connect(cancel, &QPushButton::clicked, [&]{ onCancel(); });
      buttonsLayout->addWidget(cancel);

      break;
    }
  }

  auto* grid = new QGridLayout(m_overlay.get());
  grid->addWidget(createTransparentWidget(), 0, 1);
  grid->addWidget(createTransparentWidget(), 2, 1);
  grid->addWidget(createTransparentWidget(), 1, 0);
  grid->addWidget(createTransparentWidget(), 1, 2);
  grid->addWidget(center, 1, 1);

  if (!overlayTarget) {
    grid->setContentsMargins(0, 0, 0, 0);
  }

  grid->setRowStretch(0, 1);
  grid->setRowStretch(2, 1);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(2, 1);

  disableAll();

  if (overlayTarget) {
    m_filter.reset(new Filter);
    m_filter->resized = [=]{ m_overlay->setGeometry(overlayTarget->rect()); };
    m_filter->closed = [=]{ onForceUnlock(); };
    overlayTarget->installEventFilter(m_filter.get());
  }

  m_overlay->setFocusPolicy(Qt::TabFocus);
  m_overlay->setFocus();
  m_overlay->show();
  m_overlay->setEnabled(true);

  if (!overlayTarget) {
    m_overlay->raise();
    m_overlay->activateWindow();
  }
}

void LockWidget::onForceUnlock()
{
  m_result = ForceUnlocked;
  unlock();
}

void LockWidget::onCancel()
{
  m_result = Cancelled;
  unlock();
}

template <class T>
QList<T> findChildrenImmediate(QWidget* parent)
{
  return parent->findChildren<T>(QString(), Qt::FindDirectChildrenOnly);
}

void LockWidget::disableAll()
{
  const auto topLevels = QApplication::topLevelWidgets();

  for (auto* w : topLevels) {
    if (auto* mw=dynamic_cast<QMainWindow*>(w)) {
      disable(mw->centralWidget());
      disable(mw->menuBar());
      disable(mw->statusBar());

      for (auto* tb : findChildrenImmediate<QToolBar*>(w)) {
        disable(tb);
      }

      for (auto* d : findChildrenImmediate<QDockWidget*>(w)) {
        disable(d);
      }
    }

    if (auto* d=dynamic_cast<QDialog*>(w)) {
      // no central widget, just disable the children, except for the overlay
      for (auto* child : findChildrenImmediate<QWidget*>(d)) {
        if (child != m_overlay.get()) {
          disable(child);
        }
      }
    }
  }
}

void LockWidget::enableAll()
{
  for (auto w : m_disabled) {
    if (w) {
      w->setEnabled(true);
    }
  }

  m_disabled.clear();
}

void LockWidget::disable(QWidget* w)
{
  if (w->isEnabled()) {
    w->setEnabled(false);
    m_disabled.push_back(w);
  }
}