/***************************************************************************
                          kreportsview.cpp  -  description
                             -------------------
    begin                : Sat Mar 27 2004
    copyright            : (C) 2000-2004 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
                           Ace Jones <ace.j@hotpop.com>
                           (C) 2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kreportsview_p.h"

#include <typeinfo>

// ----------------------------------------------------------------------------
// QT Includes

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFile>
#include <QTimer>
#include <QClipboard>
#include <QList>
#include <QVBoxLayout>
#include <QMimeData>
#include <QIcon>
#include <QUrlQuery>
#include <QFileInfo>
#include <QFileDialog>
#include <QLocale>
#include <QTextCodec>
#include <QPrintDialog>
#include <QMenu>
#ifdef ENABLE_WEBENGINE
#include <QWebEngineView>
#else
#include <KWebView>
#endif

// ----------------------------------------------------------------------------
// KDE Includes

#include <KMessageBox>
#include <KRecentDirs>
#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "ui_reportcontrol.h"

#include "mymoneyfile.h"
#include "mymoneyreport.h"
#include "mymoneyexception.h"
#include "kmymoneyglobalsettings.h"
#include "querytable.h"
#include "objectinfotable.h"
#include "kreportconfigurationfilterdlg.h"
#include "icons/icons.h"
#include <kmymoneywebpage.h>
#include "tocitem.h"
#include "tocitemgroup.h"
#include "tocitemreport.h"
#include "kreportchartview.h"
#include "pivottable.h"
#include "reporttable.h"
#include "../widgets/reportcontrolimpl.h"
#include "mymoneyenums.h"
#include "menuenums.h"

using namespace reports;
using namespace eMyMoney;
using namespace Icons;

#define VIEW_LEDGER         "ledger"
#define VIEW_SCHEDULE       "schedule"
#define VIEW_WELCOME        "welcome"
#define VIEW_HOME           "home"
#define VIEW_REPORTS        "reports"

/**
  * KReportsView Implementation
  */

KReportsView::KReportsView(QWidget *parent) :
  KMyMoneyViewBase(*new KReportsViewPrivate(this), parent)
{
  connect(pActions[eMenu::Action::ReportAccountTransactions], &QAction::triggered, this, &KReportsView::slotReportAccountTransactions);
}

KReportsView::~KReportsView()
{
}

void KReportsView::setDefaultFocus()
{
  Q_D(KReportsView);
  QTimer::singleShot(0, d->m_tocTreeWidget, SLOT(setFocus()));
}

void KReportsView::refresh()
{
  Q_D(KReportsView);
  if (isVisible()) {
    d->loadView();
    d->m_needsRefresh = false;
  } else {
    d->m_needsRefresh = true;
  }
}

void KReportsView::showEvent(QShowEvent * event)
{
  Q_D(KReportsView);
  if (d->m_needLoad)
    d->init();

  emit aboutToShow(View::Reports);

  if (d->m_needsRefresh)
    refresh();

  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());
  if (tab)
    emit reportSelected(tab->report());
  else
    emit reportSelected(MyMoneyReport());

  // don't forget base class implementation
  QWidget::showEvent(event);
}

void KReportsView::updateActions(const MyMoneyObject& obj)
{
  Q_D(KReportsView);
  if (typeid(obj) != typeid(MyMoneyAccount) &&
      (obj.id().isEmpty() && d->m_currentAccount.id().isEmpty())) // do not disable actions that were already disabled)))
    return;

  const auto& acc = static_cast<const MyMoneyAccount&>(obj);

  bool b;
  if (MyMoneyFile::instance()->isStandardAccount(acc.id())) {
    b = false;
  } else {
    switch (acc.accountType()) {
      case eMyMoney::Account::Type::Asset:
      case eMyMoney::Account::Type::Liability:
      case eMyMoney::Account::Type::Equity:
      case eMyMoney::Account::Type::Checkings:
        b = true;
        break;
      default:
        b = false;
        break;
    }
  }
  pActions[eMenu::Action::ReportAccountTransactions]->setEnabled(b);

  d->m_currentAccount = acc;
}

void KReportsView::slotOpenUrl(const QUrl &url)
{
  QString view = url.fileName();
  if (view.isEmpty())
    return;
  QString command = QUrlQuery(url).queryItemValue("command");
  QString id = QUrlQuery(url).queryItemValue("id");
  QString tid = QUrlQuery(url).queryItemValue("tid");

  if (view == VIEW_REPORTS) {

    if (command.isEmpty()) {
      // slotRefreshView();
    } else if (command == QLatin1String("print"))
      slotPrintView();
    else if (command == QLatin1String("copy"))
      slotCopyView();
    else if (command == QLatin1String("save"))
      slotSaveView();
    else if (command == QLatin1String("configure"))
      slotConfigure();
    else if (command == QLatin1String("duplicate"))
      slotDuplicate();
    else if (command == QLatin1String("close"))
      slotCloseCurrent();
    else if (command == QLatin1String("delete"))
      slotDelete();
    else
      qWarning() << i18n("Unknown command '%1' in KReportsView::slotOpenUrl()", qPrintable(command));

  } else if (view == VIEW_LEDGER) {
    emit transactionSelected(id, tid);
  } else {
    qWarning() << i18n("Unknown view '%1' in KReportsView::slotOpenUrl()", qPrintable(view));
  }
}

void KReportsView::slotPrintView()
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());
  if (tab)
    tab->print();
}

void KReportsView::slotCopyView()
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());
  if (tab)
    tab->copyToClipboard();
}

void KReportsView::slotSaveView()
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());
  if (tab) {
    QString filterList = i18nc("CSV (Filefilter)", "CSV files") + QLatin1String(" (*.csv);;") + i18nc("HTML (Filefilter)", "HTML files") + QLatin1String(" (*.html)");
    QUrl newURL = QFileDialog::getSaveFileUrl(this, i18n("Export as"), QUrl::fromLocalFile(KRecentDirs::dir(":kmymoney-export")), filterList, &d->m_selectedExportFilter);
    if (!newURL.isEmpty()) {
      KRecentDirs::add(":kmymoney-export", newURL.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path());
      QString newName = newURL.toDisplayString(QUrl::PreferLocalFile);

      try {
        tab->saveAs(newName, true);
      } catch (const MyMoneyException &e) {
        KMessageBox::error(this, i18n("Failed to save: %1", e.what()));
      }
    }
  }
}

void KReportsView::slotConfigure()
{
  Q_D(KReportsView);
  QString cm = "KReportsView::slotConfigure";

  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());

  if (!tab) // nothing to do
    return;
  int tabNr = d->m_reportTabWidget->currentIndex();

  tab->updateDataRange(); // range will be needed during configuration, but cannot be obtained earlier

  MyMoneyReport report = tab->report();
  if (report.comment() == i18n("Default Report") || report.comment() == i18n("Generated Report")) {
    report.setComment(i18n("Custom Report"));
    report.setName(i18n("%1 (Customized)", report.name()));
  }

  QPointer<KReportConfigurationFilterDlg> dlg = new KReportConfigurationFilterDlg(report);

  if (dlg->exec()) {
    MyMoneyReport newreport = dlg->getConfig();

    // If this report has an ID, then MODIFY it, otherwise ADD it
    MyMoneyFileTransaction ft;
    try {
      if (! newreport.id().isEmpty()) {
        MyMoneyFile::instance()->modifyReport(newreport);
        ft.commit();
        tab->modifyReport(newreport);

        d->m_reportTabWidget->setTabText(tabNr, newreport.name());
        d->m_reportTabWidget->setCurrentIndex(tabNr) ;
      } else {
        MyMoneyFile::instance()->addReport(newreport);
        ft.commit();

        QString reportGroupName = newreport.group();

        // find report group
        TocItemGroup* tocItemGroup = d->m_allTocItemGroups[reportGroupName];
        if (!tocItemGroup) {
          QString error = i18n("Could not find reportgroup \"%1\" for report \"%2\".\nPlease report this error to the developer's list: kmymoney-devel@kde.org", reportGroupName, newreport.name());

          // write to messagehandler
          qWarning() << cm << error;

          // also inform user
          KMessageBox::error(d->m_reportTabWidget, error, i18n("Critical Error"));

          // cleanup
          delete dlg;

          return;
        }

        // do not add TocItemReport to TocItemGroup here,
        // this is done in loadView

        d->addReportTab(newreport);
      }
    } catch (const MyMoneyException &e) {
      KMessageBox::error(this, i18n("Failed to configure report: %1", e.what()));
    }
  }
  delete dlg;
}

void KReportsView::slotDuplicate()
{
  Q_D(KReportsView);
  QString cm = "KReportsView::slotDuplicate";

  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());

  if (!tab) {
    // nothing to do
    return;
  }

  MyMoneyReport dupe = tab->report();
  dupe.setName(i18n("Copy of %1", dupe.name()));
  if (dupe.comment() == i18n("Default Report"))
    dupe.setComment(i18n("Custom Report"));
  dupe.clearId();

  QPointer<KReportConfigurationFilterDlg> dlg = new KReportConfigurationFilterDlg(dupe);
  if (dlg->exec()) {
    MyMoneyReport newReport = dlg->getConfig();
    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->addReport(newReport);
      ft.commit();

      QString reportGroupName = newReport.group();

      // find report group
      TocItemGroup* tocItemGroup = d->m_allTocItemGroups[reportGroupName];
      if (!tocItemGroup) {
        QString error = i18n("Could not find reportgroup \"%1\" for report \"%2\".\nPlease report this error to the developer's list: kmymoney-devel@kde.org", reportGroupName, newReport.name());

        // write to messagehandler
        qWarning() << cm << error;

        // also inform user
        KMessageBox::error(d->m_reportTabWidget, error, i18n("Critical Error"));

        // cleanup
        delete dlg;

        return;
      }

      // do not add TocItemReport to TocItemGroup here,
      // this is done in loadView

      d->addReportTab(newReport);
    } catch (const MyMoneyException &e) {
      QString error = i18n("Cannot add report, reason: \"%1\"", e.what());

      // write to messagehandler
      qWarning() << cm << error;

      // also inform user
      KMessageBox::error(d->m_reportTabWidget, error, i18n("Critical Error"));
    }
  }
  delete dlg;
}

void KReportsView::slotDelete()
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());
  if (!tab) {
    // nothing to do
    return;
  }

  MyMoneyReport report = tab->report();
  if (! report.id().isEmpty()) {
    if (KMessageBox::Continue == d->deleteReportDialog(report.name())) {
      // close the tab and then remove the report so that it is not
      // generated again during the following loadView() call
      slotClose(d->m_reportTabWidget->currentIndex());

      MyMoneyFileTransaction ft;
      MyMoneyFile::instance()->removeReport(report);
      ft.commit();
    }
  } else {
    KMessageBox::information(this,
                             QString("<qt>") +
                             i18n("<b>%1</b> is a default report, so it cannot be deleted.",
                                  report.name()) + QString("</qt>"), i18n("Delete Report?"));
  }
}

void KReportsView::slotOpenReport(const QString& id)
{
  Q_D(KReportsView);
  if (id.isEmpty()) {
    // nothing to  do
    return;
  }

  KReportTab* page = 0;

  // Find the tab which contains the report
  int index = 1;
  while (index < d->m_reportTabWidget->count()) {
    KReportTab* current = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(index));

    if (current->report().id() == id) {
      page = current;
      break;
    }

    ++index;
  }

  // Show the tab, or create a new one, as needed
  if (page)
    d->m_reportTabWidget->setCurrentIndex(index);
  else
    d->addReportTab(MyMoneyFile::instance()->report(id));
}

void KReportsView::slotOpenReport(const MyMoneyReport& report)
{
  Q_D(KReportsView);
  if (d->m_needLoad)
    d->init();
  qDebug() << Q_FUNC_INFO << " " << report.name();
  KReportTab* page = 0;

  // Find the tab which contains the report indicated by this list item
  int index = 1;
  while (index < d->m_reportTabWidget->count()) {
    KReportTab* current = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(index));

    if (current->report().name() == report.name()) {
      page = current;
      break;
    }

    ++index;
  }

  // Show the tab, or create a new one, as needed
  if (page)
    d->m_reportTabWidget->setCurrentIndex(index);
  else
    d->addReportTab(report);

  if (!isVisible())
    emit switchViewRequested(View::Reports);
}

void KReportsView::slotItemDoubleClicked(QTreeWidgetItem* item, int)
{
  Q_D(KReportsView);
  TocItem* tocItem = dynamic_cast<TocItem*>(item);
  if (!tocItem->isReport()) {
    // toggle the expanded-state for reportgroup-items
    item->setExpanded(item->isExpanded() ? false : true);

    // nothing else to do for reportgroup-items
    return;
  }

  TocItemReport* reportTocItem = dynamic_cast<TocItemReport*>(tocItem);

  MyMoneyReport& report = reportTocItem->getReport();

  KReportTab* page = 0;

  // Find the tab which contains the report indicated by this list item
  int index = 1;
  while (index < d->m_reportTabWidget->count()) {
    KReportTab* current = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(index));

    // If this report has an ID, we'll use the ID to match
    if (! report.id().isEmpty()) {
      if (current->report().id() == report.id()) {
        page = current;
        break;
      }
    }
    // Otherwise, use the name to match.  THIS ASSUMES that no 2 default reports
    // have the same name...but that would be pretty a boneheaded thing to do.
    else {
      if (current->report().name() == report.name()) {
        page = current;
        break;
      }
    }

    ++index;
  }

  // Show the tab, or create a new one, as needed
  if (page)
    d->m_reportTabWidget->setCurrentIndex(index);
  else
    d->addReportTab(report);
}

void KReportsView::slotToggleChart()
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->currentWidget());

  if (tab)
    tab->toggleChart();
}

void KReportsView::slotCloseCurrent()
{
  Q_D(KReportsView);
  slotClose(d->m_reportTabWidget->currentIndex());
}

void KReportsView::slotClose(int index)
{
  Q_D(KReportsView);
  KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(index));
  if (tab) {
    d->m_reportTabWidget->removeTab(index);
    tab->setReadyToDelete(true);
  }
}

void KReportsView::slotCloseAll()
{
  Q_D(KReportsView);
  if(!d->m_needLoad) {
    while (true) {
      KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(1));
      if (tab) {
        d->m_reportTabWidget->removeTab(1);
        tab->setReadyToDelete(true);
      } else
        break;
    }
  }
}


void KReportsView::slotListContextMenu(const QPoint & p)
{
  Q_D(KReportsView);
  QTreeWidgetItem *item = d->m_tocTreeWidget->itemAt(p);

  if (!item) {
    return;
  }

  TocItem* tocItem = dynamic_cast<TocItem*>(item);

  if (!tocItem->isReport()) {
    // currently there is no context menu for reportgroup items
    return;
  }

  QMenu* contextmenu = new QMenu(this);

  contextmenu->addAction(i18nc("To open a new report", "&Open"),
                         this, SLOT(slotOpenFromList()));

  contextmenu->addAction(i18nc("Configure a report", "&Configure"),
                         this, SLOT(slotConfigureFromList()));

  contextmenu->addAction(i18n("&New report"),
                         this, SLOT(slotNewFromList()));

  // Only add this option if it's a custom report. Default reports cannot be deleted
  TocItemReport* reportTocItem = dynamic_cast<TocItemReport*>(tocItem);
  MyMoneyReport& report = reportTocItem->getReport();
  if (! report.id().isEmpty()) {
    contextmenu->addAction(i18n("&Delete"),
                           this, SLOT(slotDeleteFromList()));
  }

  contextmenu->popup(d->m_tocTreeWidget->mapToGlobal(p));
}

void KReportsView::slotOpenFromList()
{
  Q_D(KReportsView);
  TocItem* tocItem = dynamic_cast<TocItem*>(d->m_tocTreeWidget->currentItem());

  if (tocItem)
    slotItemDoubleClicked(tocItem, 0);
}

void KReportsView::slotConfigureFromList()
{
  Q_D(KReportsView);
  TocItem* tocItem = dynamic_cast<TocItem*>(d->m_tocTreeWidget->currentItem());

  if (tocItem) {
    slotItemDoubleClicked(tocItem, 0);
    slotConfigure();
  }
}

void KReportsView::slotNewFromList()
{
  Q_D(KReportsView);
  TocItem* tocItem = dynamic_cast<TocItem*>(d->m_tocTreeWidget->currentItem());

  if (tocItem) {
    slotItemDoubleClicked(tocItem, 0);
    slotDuplicate();
  }
}

void KReportsView::slotDeleteFromList()
{
  Q_D(KReportsView);
  TocItem* tocItem = dynamic_cast<TocItem*>(d->m_tocTreeWidget->currentItem());

  if (tocItem) {
    TocItemReport* reportTocItem = dynamic_cast<TocItemReport*>(tocItem);

    MyMoneyReport& report = reportTocItem->getReport();

    // If this report does not have an ID, it's a default report and cannot be deleted
    if (! report.id().isEmpty() &&
        KMessageBox::Continue == d->deleteReportDialog(report.name())) {
      // check if report's tab is open; start from 1 because 0 is toc tab
      for (int i = 1; i < d->m_reportTabWidget->count(); ++i) {
        KReportTab* tab = dynamic_cast<KReportTab*>(d->m_reportTabWidget->widget(i));
        if (tab->report().id() == report.id()) {
          slotClose(i); // if open, close it, so no crash when switching to it
          break;
        }
      }
      MyMoneyFileTransaction ft;
      MyMoneyFile::instance()->removeReport(report);
      ft.commit();
    }
  }
}

void KReportsView::slotReportAccountTransactions()
{
  Q_D(KReportsView);
  // Generate a transaction report that contains transactions for only the
  // currently selected account.
  if (!d->m_currentAccount.id().isEmpty()) {
    MyMoneyReport report(
      MyMoneyReport::eAccount,
      MyMoneyReport::eQCnumber | MyMoneyReport::eQCpayee | MyMoneyReport::eQCcategory,
      eMyMoney::TransactionFilter::Date::YearToDate,
      MyMoneyReport::eDetailAll,
      i18n("%1 YTD Account Transactions", d->m_currentAccount.name()),
      i18n("Generated Report")
    );
    report.setGroup(i18n("Transactions"));
    report.addAccount(d->m_currentAccount.id());
    emit switchViewRequested(View::Reports);
    slotOpenReport(report);
  }
}

// Make sure, that these definitions are only used within this file
// this does not seem to be necessary, but when building RPMs the
// build option 'final' is used and all CPP files are concatenated.
// So it could well be, that in another CPP file these definitions
// are also used.
#undef VIEW_LEDGER
#undef VIEW_SCHEDULE
#undef VIEW_WELCOME
#undef VIEW_HOME
#undef VIEW_REPORTS
