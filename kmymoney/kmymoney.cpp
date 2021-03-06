/***************************************************************************
                          kmymoney.cpp
                             -------------------
    copyright            : (C) 2000 by Michael Edwardes <mte@users.sourceforge.net>
                           (C) 2007 by Thomas Baumgart <ipwizard@users.sourceforge.net>
                           (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>

****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <config-kmymoney.h>

#include "kmymoney.h"

// ----------------------------------------------------------------------------
// Std C++ / STL Includes

#include <typeinfo>
#include <iostream>
#include <memory>

// ----------------------------------------------------------------------------
// QT Includes

#include <QDir>
#include <QDateTime>         // only for performance tests
#include <QTimer>
#include <QByteArray>
#include <QBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QList>
#include <QUrl>
#include <QClipboard>
#include <QKeySequence>
#include <QIcon>
#include <QInputDialog>
#include <QStatusBar>
#include <QPushButton>
#include <QListWidget>
#include <QApplication>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KToolBar>
#include <KMessageBox>
#include <KLocalizedString>
#include <KConfig>
#include <KStandardAction>
#include <KActionCollection>
#include <KTipDialog>
#include <KRun>
#include <KConfigDialog>
#include <KXMLGUIFactory>
#include <KRecentFilesAction>
#include <KRecentDirs>
#include <KProcess>
#include <KAboutApplicationDialog>
#ifdef KF5Holidays_FOUND
#include <KHolidays/Holiday>
#include <KHolidays/HolidayRegion>
#endif

// ----------------------------------------------------------------------------
// Project Includes

#include "kmymoneyglobalsettings.h"
#include "kmymoneyadaptor.h"

#include "dialogs/settings/ksettingskmymoney.h"
#include "dialogs/kbackupdlg.h"
#include "dialogs/kenterscheduledlg.h"
#include "dialogs/kconfirmmanualenterdlg.h"
#include "dialogs/kmymoneypricedlg.h"
#include "dialogs/kcurrencyeditdlg.h"
#include "dialogs/kequitypriceupdatedlg.h"
#include "dialogs/kmymoneyfileinfodlg.h"
#include "dialogs/kfindtransactiondlg.h"
#include "dialogs/knewbankdlg.h"
#include "wizards/newinvestmentwizard/knewinvestmentwizard.h"
#include "dialogs/knewaccountdlg.h"
#include "dialogs/editpersonaldatadlg.h"
#include "dialogs/kselectdatabasedlg.h"
#include "dialogs/kcurrencycalculator.h"
#include "dialogs/keditscheduledlg.h"
#include "wizards/newloanwizard/keditloanwizard.h"
#include "dialogs/kpayeereassigndlg.h"
#include "dialogs/kcategoryreassigndlg.h"
#include "wizards/endingbalancedlg/kendingbalancedlg.h"
#include "dialogs/kbalancechartdlg.h"
#include "dialogs/kgeneratesqldlg.h"
#include "dialogs/kloadtemplatedlg.h"
#include "dialogs/kgpgkeyselectiondlg.h"
#include "dialogs/ktemplateexportdlg.h"
#include "dialogs/transactionmatcher.h"
#include "wizards/newuserwizard/knewuserwizard.h"
#include "wizards/newaccountwizard/knewaccountwizard.h"
#include "dialogs/kbalancewarning.h"
#include "widgets/kmymoneyaccountselector.h"
#include "widgets/kmymoneypayeecombo.h"
#include "widgets/onlinejobmessagesview.h"

#include "widgets/kmymoneymvccombo.h"

#include "views/kmymoneyview.h"
#include "views/konlinejoboutbox.h"
#include "models/onlinejobmessagesmodel.h"

#include "mymoney/mymoneyobject.h"
#include "mymoney/mymoneyfile.h"
#include "mymoney/mymoneyinstitution.h"
#include "mymoney/mymoneyaccount.h"
#include "mymoney/mymoneyaccountloan.h"
#include "mymoney/mymoneysecurity.h"
#include "mymoney/mymoneypayee.h"
#include "mymoney/mymoneytag.h"
#include "mymoney/mymoneybudget.h"
#include "mymoney/mymoneyreport.h"
#include "mymoney/mymoneysplit.h"
#include "mymoney/mymoneyutils.h"
#include "mymoney/mymoneystatement.h"
#include "mymoney/mymoneyforecast.h"
#include "mymoney/mymoneytransactionfilter.h"

#include "mymoney/onlinejobmessage.h"

#include "converter/mymoneystatementreader.h"
#include "converter/mymoneytemplate.h"

#include "plugins/interfaces/kmmviewinterface.h"
#include "plugins/interfaces/kmmstatementinterface.h"
#include "plugins/interfaces/kmmimportinterface.h"
#include "plugins/interfaceloader.h"
#include "plugins/onlinepluginextended.h"
#include "pluginloader.h"

#include "tasks/credittransfer.h"

#include "icons/icons.h"

#include "misc/webconnect.h"

#include "storage/mymoneystoragemgr.h"
#include "storage/mymoneystoragesql.h"

#include <libkgpgfile/kgpgfile.h>

#include "transactioneditor.h"
#include "konlinetransferform.h"
#include <QHBoxLayout>
#include <QFileDialog>

#include "kmymoneyutils.h"
#include "kcreditswindow.h"

#include "ledgerdelegate.h"
#include "storageenums.h"
#include "mymoneyenums.h"
#include "dialogenums.h"
#include "menuenums.h"

#include "misc/platformtools.h"

// includes needed for shared global settings
#include "mymoney_config.h"
#include "widgets_config.h"

#ifdef KMM_DEBUG
#include "mymoney/storage/mymoneystoragedump.h"
#include "mymoneytracer.h"
#endif

using namespace Icons;
using namespace eMenu;

static constexpr char recoveryKeyId[] = "59B0F826D2B08440";

// define the default period to warn about an expiring recoverkey to 30 days
// but allows to override this setting during build time
#ifndef RECOVER_KEY_EXPIRATION_WARNING
#define RECOVER_KEY_EXPIRATION_WARNING 30
#endif

QHash<eMenu::Action, QAction *> pActions;
QHash<eMenu::Menu, QMenu *> pMenus;

enum backupStateE {
  BACKUP_IDLE = 0,
  BACKUP_MOUNTING,
  BACKUP_COPYING,
  BACKUP_UNMOUNTING
};

class KMyMoneyApp::Private
{
public:
  Private(KMyMoneyApp *app) :
      q(app),
      m_ft(0),
      m_moveToAccountSelector(0),
      m_statementXMLindex(0),
      m_balanceWarning(0),
      m_collectingStatements(false),
      m_backupResult(0),
      m_backupMount(0),
      m_ignoreBackupExitCode(false),
      m_myMoneyView(0),
      m_progressBar(0),
      m_smtReader(0),
      m_searchDlg(0),
      m_autoSaveTimer(0),
      m_progressTimer(0),
      m_inAutoSaving(false),
      m_transactionEditor(0),
      m_endingBalanceDlg(0),
      m_saveEncrypted(0),
      m_additionalKeyLabel(0),
      m_additionalKeyButton(0),
      m_recentFiles(0),
#ifdef KF5Holidays_FOUND
      m_holidayRegion(0),
#endif
      m_applicationIsReady(true),
      m_webConnect(new WebConnect(app)) {
    // since the days of the week are from 1 to 7,
    // and a day of the week is used to index this bit array,
    // resize the array to 8 elements (element 0 is left unused)
    m_processingDays.resize(8);

  }

  void closeFile();
  void unlinkStatementXML();
  void moveInvestmentTransaction(const QString& fromId,
                                 const QString& toId,
                                 const MyMoneyTransaction& t);
  QList<QPair<MyMoneyTransaction, MyMoneySplit> > automaticReconciliation(const MyMoneyAccount &account,
      const QList<QPair<MyMoneyTransaction, MyMoneySplit> > &transactions,
      const MyMoneyMoney &amount);


  /**
    * The public interface.
    */
  KMyMoneyApp * const q;

  MyMoneyFileTransaction*       m_ft;
  KMyMoneyAccountSelector*      m_moveToAccountSelector;
  int                           m_statementXMLindex;
  KBalanceWarning*              m_balanceWarning;

  bool                          m_collectingStatements;
  QStringList                   m_statementResults;
  QString                       m_lastPayeeEnteredId;

  /** the configuration object of the application */
  KSharedConfigPtr m_config;

  /**
   * @brief Structure of plugins objects by their interfaces
   */
  KMyMoneyPlugin::Container m_plugins;

  /**
    * The following variable represents the state while crafting a backup.
    * It can have the following values
    *
    * - IDLE: the default value if not performing a backup
    * - MOUNTING: when a mount command has been issued
    * - COPYING:  when a copy command has been issued
    * - UNMOUNTING: when an unmount command has been issued
    */
  backupStateE   m_backupState;

  /**
    * This variable keeps the result of the backup operation.
    */
  int     m_backupResult;

  /**
    * This variable is set, when the user selected to mount/unmount
    * the backup volume.
    */
  bool    m_backupMount;

  /**
    * Flag for internal run control
    */
  bool    m_ignoreBackupExitCode;

  KProcess m_proc;

  /// A pointer to the view holding the tabs.
  KMyMoneyView *m_myMoneyView;

  /// The URL of the file currently being edited when open.
  QUrl  m_fileName;

  bool m_startDialog;
  QString m_mountpoint;

  QProgressBar* m_progressBar;
  QTime         m_lastUpdate;
  QLabel*       m_statusLabel;

  MyMoneyStatementReader* m_smtReader;
  // allows multiple imports to be launched trough web connect and to be executed sequentially
  QQueue<QString> m_importUrlsQueue;
  KFindTransactionDlg* m_searchDlg;

  MyMoneyAccount        m_selectedAccount;
  MyMoneyAccount        m_reconciliationAccount;
  MyMoneySchedule       m_selectedSchedule;
  KMyMoneyRegister::SelectedTransactions m_selectedTransactions;

  // This is Auto Saving related
  bool                  m_autoSaveEnabled;
  QTimer*               m_autoSaveTimer;
  QTimer*               m_progressTimer;
  int                   m_autoSavePeriod;
  bool                  m_inAutoSaving;

  // pointer to the current transaction editor
  TransactionEditor*    m_transactionEditor;

  // Reconciliation dialog
  KEndingBalanceDlg*    m_endingBalanceDlg;

  // Pointer to the combo box used for key selection during
  // File/Save as
  KComboBox*            m_saveEncrypted;

  // id's that need to be remembered
  QString               m_accountGoto, m_payeeGoto;

  QStringList           m_additionalGpgKeys;
  QLabel*               m_additionalKeyLabel;
  QPushButton*          m_additionalKeyButton;

  KRecentFilesAction*   m_recentFiles;

#ifdef KF5Holidays_FOUND
  // used by the calendar interface for schedules
  KHolidays::HolidayRegion* m_holidayRegion;
#endif
  QBitArray             m_processingDays;
  QMap<QDate, bool>     m_holidayMap;
  QStringList           m_consistencyCheckResult;
  bool                  m_applicationIsReady;

  WebConnect*           m_webConnect;

  // methods
  void consistencyCheck(bool alwaysDisplayResults);
  static void setThemedCSS();
  void copyConsistencyCheckResults();
  void saveConsistencyCheckResults();
};

KMyMoneyApp::KMyMoneyApp(QWidget* parent) :
    KXmlGuiWindow(parent),
    d(new Private(this))
{
#ifdef KMM_DBUS
  new KmymoneyAdaptor(this);
  QDBusConnection::sessionBus().registerObject("/KMymoney", this);
  QDBusConnection::sessionBus().interface()->registerService(
    "org.kde.kmymoney", QDBusConnectionInterface::DontQueueService);
#endif
  // Register the main engine types used as meta-objects
  qRegisterMetaType<MyMoneyMoney>("MyMoneyMoney");
  qRegisterMetaType<MyMoneySecurity>("MyMoneySecurity");

  MyMoney::injectExternalSettings(KMyMoneyGlobalSettings::self());
  Widgets::injectExternalSettings(KMyMoneyGlobalSettings::self());

  // preset the pointer because we need it during the course of this constructor
  kmymoney = this;
  d->m_config = KSharedConfig::openConfig();

  d->setThemedCSS();

  MyMoneyTransactionFilter::setFiscalYearStart(KMyMoneyGlobalSettings::firstFiscalMonth(), KMyMoneyGlobalSettings::firstFiscalDay());

  updateCaption(true);

  QFrame* frame = new QFrame;
  frame->setFrameStyle(QFrame::NoFrame);
  // values for margin (11) and spacing(6) taken from KDialog implementation
  QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, frame);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(6);

  {
    QString themeName = KMyMoneySettings::iconsTheme();                 // get theme user wants
    if (!themeName.isEmpty() && themeName != QLatin1Literal("system"))  // if it isn't default theme then set it
      QIcon::setThemeName(themeName);
    Icons::setIconThemeNames(QIcon::themeName());                       // get whatever theme user ends up with and hope our icon names will fit that theme
  }

  initStatusBar();
  pActions = initActions();
  pMenus = initMenus();

  d->m_myMoneyView = new KMyMoneyView(this/*the global variable kmymoney is not yet assigned. So we pass it here*/);
  layout->addWidget(d->m_myMoneyView, 10);
  connect(d->m_myMoneyView, &KMyMoneyView::aboutToChangeView, this, &KMyMoneyApp::slotResetSelections);
  connect(d->m_myMoneyView, SIGNAL(currentPageChanged(KPageWidgetItem*,KPageWidgetItem*)),
          this, SLOT(slotUpdateActions()));

  connect(d->m_myMoneyView, &KMyMoneyView::statusMsg, this, &KMyMoneyApp::slotStatusMsg);
  connect(d->m_myMoneyView, &KMyMoneyView::statusProgress, this, &KMyMoneyApp::slotStatusProgressBar);

  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  readOptions();

  // now initialize the plugin structure
  createInterfaces();
  KMyMoneyPlugin::pluginHandling(KMyMoneyPlugin::Action::Load, d->m_plugins, this, guiFactory());
  onlineJobAdministration::instance()->setOnlinePlugins(d->m_plugins.extended);
  d->m_myMoneyView->setOnlinePlugins(d->m_plugins.online);

  setCentralWidget(frame);

  connect(&d->m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotBackupHandleEvents()));

  // force to show the home page if the file is closed
  connect(pActions[Action::ViewTransactionDetail], &QAction::toggled, d->m_myMoneyView, &KMyMoneyView::slotShowTransactionDetail);

  d->m_backupState = BACKUP_IDLE;

  QLocale locale;
  int weekStart = locale.firstDayOfWeek();
  int weekEnd = weekStart-1;
  if (weekEnd < Qt::Monday) {
    weekEnd = Qt::Sunday;
  }
  bool startFirst = (weekStart < weekEnd);
  for (int i = 0; i < 8; ++i) {
    if (startFirst)
      d->m_processingDays.setBit(i, (i >= weekStart && i <= weekEnd));
    else
      d->m_processingDays.setBit(i, (i >= weekStart || i <= weekEnd));
  }
  d->m_autoSaveTimer = new QTimer(this);
  d->m_progressTimer = new QTimer(this);

  connect(d->m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));
  connect(d->m_progressTimer, SIGNAL(timeout()), this, SLOT(slotStatusProgressDone()));

  // make sure, we get a note when the engine changes state
  connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotDataChanged()));

  // connect the WebConnect server
  connect(d->m_webConnect, SIGNAL(gotUrl(QUrl)), this, SLOT(webConnect(QUrl)));
  // make sure we have a balance warning object
  d->m_balanceWarning = new KBalanceWarning(this);

  // setup the initial configuration
  slotUpdateConfiguration(QString());

  // kickstart date change timer
  slotDateChanged();

  connect(this, SIGNAL(fileLoaded(QUrl)), onlineJobAdministration::instance(), SLOT(updateOnlineTaskProperties()));

}

KMyMoneyApp::~KMyMoneyApp()
{
  // delete cached objects since the are in the way
  // when unloading the plugins
  onlineJobAdministration::instance()->clearCaches();

  // we need to unload all plugins before we destroy anything else
  KMyMoneyPlugin::pluginHandling(KMyMoneyPlugin::Action::Unload, d->m_plugins, this, guiFactory());

  delete d->m_searchDlg;
  delete d->m_transactionEditor;
  delete d->m_endingBalanceDlg;
  delete d->m_moveToAccountSelector;
#ifdef KF5Holidays_FOUND
  delete d->m_holidayRegion;
#endif
  Widgets::injectExternalSettings(nullptr);
  MyMoney::injectExternalSettings(nullptr);
  delete d;
}

QUrl KMyMoneyApp::lastOpenedURL()
{
  QUrl url = d->m_startDialog ? QUrl() : d->m_fileName;

  if (!url.isValid()) {
    url = QUrl::fromUserInput(readLastUsedFile());
  }

  ready();

  return url;
}

void KMyMoneyApp::slotObjectDestroyed(QObject* o)
{
  if (o == d->m_moveToAccountSelector) {
    d->m_moveToAccountSelector = 0;
  }
}

void KMyMoneyApp::slotInstallConsistencyCheckContextMenu()
{
  // this code relies on the implementation of KMessageBox::informationList to add a context menu to that list,
  // please adjust it if it's necessary or rewrite the way the consistency check results are displayed
  if (QWidget* dialog = QApplication::activeModalWidget()) {
    if (QListWidget* widget = dialog->findChild<QListWidget *>()) {
      // give the user a hint that the data can be saved
      widget->setToolTip(i18n("This is the consistency check log, use the context menu to copy or save it."));
      widget->setWhatsThis(widget->toolTip());
      widget->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(widget, SIGNAL(customContextMenuRequested(QPoint)), SLOT(slotShowContextMenuForConsistencyCheck(QPoint)));
    }
  }
}

void KMyMoneyApp::slotShowContextMenuForConsistencyCheck(const QPoint &pos)
{
  // allow the user to save the consistency check results
  if (QWidget* widget = qobject_cast< QWidget* >(sender())) {
    QMenu contextMenu(widget);
    QAction* copy = new QAction(i18n("Copy to clipboard"), widget);
    QAction* save = new QAction(i18n("Save to file"), widget);
    contextMenu.addAction(copy);
    contextMenu.addAction(save);
    QAction *result = contextMenu.exec(widget->mapToGlobal(pos));
    if (result == copy) {
      // copy the consistency check results to the clipboard
      d->copyConsistencyCheckResults();
    } else if (result == save) {
      // save the consistency check results to a file
      d->saveConsistencyCheckResults();
    }
  }
}

QHash<eMenu::Menu, QMenu *> KMyMoneyApp::initMenus()
{
  QHash<Menu, QMenu *> lutMenus;
  const QHash<Menu, QString> menuNames {
    {Menu::Institution,             QStringLiteral("institution_context_menu")},
    {Menu::Account,                 QStringLiteral("account_context_menu")},
    {Menu::Schedule,                QStringLiteral("schedule_context_menu")},
    {Menu::Category,                QStringLiteral("category_context_menu")},
    {Menu::Tag,                     QStringLiteral("tag_context_menu")},
    {Menu::Payee,                   QStringLiteral("payee_context_menu")},
    {Menu::Investment,              QStringLiteral("investment_context_menu")},
    {Menu::Transaction,             QStringLiteral("transaction_context_menu")},
    {Menu::MoveTransaction,         QStringLiteral("transaction_move_menu")},
    {Menu::MarkTransaction,         QStringLiteral("transaction_mark_menu")},
    {Menu::MarkTransactionContext,  QStringLiteral("transaction_context_mark_menu")},
    {Menu::OnlineJob,               QStringLiteral("onlinejob_context_menu")}
  };

  for (auto it = menuNames.cbegin(); it != menuNames.cend(); ++it)
    lutMenus.insert(it.key(), qobject_cast<QMenu*>(factory()->container(it.value(), this)));
  return lutMenus;
}

QHash<Action, QAction *> KMyMoneyApp::initActions()
{
  auto aC = actionCollection();

  // *************
  // Adding standard actions
  // *************
  KStandardAction::openNew(this, &KMyMoneyApp::slotFileNew, aC);
  KStandardAction::open(this, &KMyMoneyApp::slotFileOpen, aC);
  d->m_recentFiles = KStandardAction::openRecent(this, &KMyMoneyApp::slotFileOpenRecent, aC);
  KStandardAction::save(this, &KMyMoneyApp::slotFileSave, aC);
  KStandardAction::saveAs(this, &KMyMoneyApp::slotFileSaveAs, aC);
  KStandardAction::close(this, &KMyMoneyApp::slotFileClose, aC);
  KStandardAction::quit(this, &KMyMoneyApp::slotFileQuit, aC);
  KStandardAction::print(this, &KMyMoneyApp::slotPrintView, aC);
  KStandardAction::preferences(this, &KMyMoneyApp::slotSettings, aC);

  /* Look-up table for all custom actions.
  It's required for:
  1) building QList with QActions to be added to ActionCollection
  2) adding custom features to QActions like e.g. keyboard shortcut
  */
  QHash<Action, QAction *> lutActions;

  // *************
  // Adding all actions
  // *************
  {
    // struct for creating useless (unconnected) QAction
    struct actionInfo {
      Action  action;
      QString name;
      QString text;
      Icon    icon;
    };

    const QVector<actionInfo> actionInfos {
      // *************
      // The File menu
      // *************
      {Action::FileOpenDatabase,              QStringLiteral("open_database"),                  i18n("Open database..."),                           Icon::SVNUpdate},
      {Action::FileSaveAsDatabase,            QStringLiteral("saveas_database"),                i18n("Save as database..."),                        Icon::FileArchiver},
      {Action::FileBackup,                    QStringLiteral("file_backup"),                    i18n("Backup..."),                                  Icon::Empty},
      {Action::FileImportStatement,           QStringLiteral("file_import_statement"),          i18n("Statement file..."),                          Icon::Empty},
      {Action::FileImportTemplate,            QStringLiteral("file_import_template"),           i18n("Account Template..."),                        Icon::Empty},
      {Action::FileExportTemplate,            QStringLiteral("file_export_template"),           i18n("Account Template..."),                        Icon::Empty},
      {Action::FilePersonalData,              QStringLiteral("view_personal_data"),             i18n("Personal Data..."),                           Icon::UserProperties},
#ifdef KMM_DEBUG
      {Action::FileDump,                      QStringLiteral("file_dump"),                      i18n("Dump Memory"),                                Icon::Empty},
#endif
      {Action::FileInformation,               QStringLiteral("view_file_info"),                 i18n("File-Information..."),                        Icon::DocumentProperties},
      // *************
      // The Edit menu
      // *************
      {Action::EditFindTransaction,           QStringLiteral("edit_find_transaction"),          i18n("Find transaction..."),                        Icon::EditFindTransaction},
      // *************
      // The View menu
      // *************
      {Action::ViewTransactionDetail,         QStringLiteral("view_show_transaction_detail"),   i18n("Show Transaction Detail"),                    Icon::ViewTransactionDetail},
      {Action::ViewHideReconciled,            QStringLiteral("view_hide_reconciled_transactions"), i18n("Hide reconciled transactions"),            Icon::HideReconciled},
      {Action::ViewHideCategories,            QStringLiteral("view_hide_unused_categories"),    i18n("Hide unused categories"),                     Icon::HideCategories},
      {Action::ViewShowAll,                   QStringLiteral("view_show_all_accounts"),         i18n("Show all accounts"),                          Icon::Empty},
      // *********************
      // The institutions menu
      // *********************
      {Action::NewInstitution,                QStringLiteral("institution_new"),                i18n("New institution..."),                         Icon::InstitutionNew},
      {Action::EditInstitution,               QStringLiteral("institution_edit"),               i18n("Edit institution..."),                        Icon::InstitutionEdit},
      {Action::DeleteInstitution,             QStringLiteral("institution_delete"),             i18n("Delete institution..."),                      Icon::InstitutionDelete},
      // *****************
      // The accounts menu
      // *****************
      {Action::NewAccount,                    QStringLiteral("account_new"),                    i18n("New account..."),                             Icon::AccountNew},
      {Action::OpenAccount,                   QStringLiteral("account_open"),                   i18n("Open ledger"),                                Icon::ViewFinancialList},
      {Action::StartReconciliation,           QStringLiteral("account_reconcile"),              i18n("Reconcile..."),                               Icon::Reconcile},
      {Action::FinishReconciliation,          QStringLiteral("account_reconcile_finish"),       i18nc("Finish reconciliation", "Finish"),           Icon::AccountFinishReconciliation},
      {Action::PostponeReconciliation,        QStringLiteral("account_reconcile_postpone"),     i18n("Postpone reconciliation"),                    Icon::MediaPlaybackPause},
      {Action::EditAccount,                   QStringLiteral("account_edit"),                   i18n("Edit account..."),                            Icon::AccountEdit},
      {Action::DeleteAccount,                 QStringLiteral("account_delete"),                 i18n("Delete account..."),                          Icon::AccountDelete},
      {Action::CloseAccount,                  QStringLiteral("account_close"),                  i18n("Close account"),                              Icon::AccountClose},
      {Action::ReopenAccount,                 QStringLiteral("account_reopen"),                 i18n("Reopen account"),                             Icon::AccountReopen},
      {Action::ReportAccountTransactions,     QStringLiteral("account_transaction_report"),     i18n("Transaction report"),                         Icon::ViewFinancialList},
      {Action::ChartAccountBalance,           QStringLiteral("account_chart"),                  i18n("Show balance chart..."),                      Icon::OfficeChartLine},
      {Action::MapOnlineAccount,              QStringLiteral("account_online_map"),             i18n("Map account..."),                             Icon::NewsSubscribe},
      {Action::UnmapOnlineAccount,            QStringLiteral("account_online_unmap"),           i18n("Unmap account..."),                           Icon::NewsUnsubscribe},
      {Action::UpdateAccount,                 QStringLiteral("account_online_update"),          i18n("Update account..."),                          Icon::AccountUpdate},
      {Action::UpdateAllAccounts,             QStringLiteral("account_online_update_all"),      i18n("Update all accounts..."),                     Icon::AccountUpdateAll},
      {Action::AccountCreditTransfer,         QStringLiteral("account_online_new_credit_transfer"), i18n("New credit transfer"),                        Icon::AccountCreditTransfer},
      // *******************
      // The categories menu
      // *******************
      {Action::NewCategory,                   QStringLiteral("category_new"),                   i18n("New category..."),                            Icon::CategoryNew},
      {Action::EditCategory,                  QStringLiteral("category_edit"),                  i18n("Edit category..."),                           Icon::CategoryEdit},
      {Action::DeleteCategory,                QStringLiteral("category_delete"),                i18n("Delete category..."),                         Icon::CategoryDelete},
      // **************
      // The tools menu
      // **************
      {Action::ToolCurrencies,                QStringLiteral("tools_currency_editor"),          i18n("Currencies..."),                              Icon::ViewCurrencyList},
      {Action::ToolPrices,                    QStringLiteral("tools_price_editor"),             i18n("Prices..."),                                  Icon::Empty},
      {Action::ToolUpdatePrices,              QStringLiteral("tools_update_prices"),            i18n("Update Stock and Currency Prices..."),        Icon::ToolUpdatePrices},
      {Action::ToolConsistency,               QStringLiteral("tools_consistency_check"),        i18n("Consistency Check"),                          Icon::Empty},
      {Action::ToolPerformance,               QStringLiteral("tools_performancetest"),          i18n("Performance-Test"),                           Icon::Fork},
      {Action::ToolSQL,                       QStringLiteral("tools_generate_sql"),             i18n("Generate Database SQL"),                      Icon::Empty},
      {Action::ToolCalculator,                QStringLiteral("tools_kcalc"),                    i18n("Calculator..."),                              Icon::AccessoriesCalculator},
      // *****************
      // The settings menu
      // *****************
      {Action::SettingsAllMessages,           QStringLiteral("settings_enable_messages"),       i18n("Enable all messages"),                        Icon::Empty},
      // *************
      // The help menu
      // *************
      {Action::HelpShow,                      QStringLiteral("help_show_tip"),                  i18n("&Show tip of the day"),                       Icon::Tip},
      // ***************************
      // Actions w/o main menu entry
      // ***************************
      {Action::NewTransaction,                QStringLiteral("transaction_new"),                i18nc("New transaction button", "New"),             Icon::TransactionNew},
      {Action::EditTransaction,               QStringLiteral("transaction_edit"),               i18nc("Edit transaction button", "Edit"),           Icon::TransactionEdit},
      {Action::EnterTransaction,              QStringLiteral("transaction_enter"),              i18nc("Enter transaction", "Enter"),                Icon::DialogOK},
      {Action::EditSplits,                    QStringLiteral("transaction_editsplits"),         i18nc("Edit split button", "Edit splits"),          Icon::Split},
      {Action::CancelTransaction,             QStringLiteral("transaction_cancel"),             i18nc("Cancel transaction edit", "Cancel"),         Icon::DialogCancel},
      {Action::DeleteTransaction,             QStringLiteral("transaction_delete"),             i18nc("Delete transaction", "Delete"),              Icon::EditDelete},
      {Action::DuplicateTransaction,          QStringLiteral("transaction_duplicate"),          i18nc("Duplicate transaction", "Duplicate"),        Icon::EditCopy},
      {Action::MatchTransaction,              QStringLiteral("transaction_match"),              i18nc("Button text for match transaction", "Match"),Icon::TransactionMatch},
      {Action::AcceptTransaction,             QStringLiteral("transaction_accept"),             i18nc("Accept 'imported' and 'matched' transaction", "Accept"), Icon::TransactionAccept},
      {Action::ToggleReconciliationFlag,      QStringLiteral("transaction_mark_toggle"),        i18nc("Toggle reconciliation flag", "Toggle"),     Icon::Empty},
      {Action::MarkCleared,                   QStringLiteral("transaction_mark_cleared"),       i18nc("Mark transaction cleared", "Cleared"),       Icon::Empty},
      {Action::MarkReconciled,                QStringLiteral("transaction_mark_reconciled"),    i18nc("Mark transaction reconciled", "Reconciled"), Icon::Empty},
      {Action::MarkNotReconciled,             QStringLiteral("transaction_mark_notreconciled"), i18nc("Mark transaction not reconciled", "Not reconciled"),     Icon::Empty},
      {Action::SelectAllTransactions,         QStringLiteral("transaction_select_all"),         i18nc("Select all transactions", "Select all"),     Icon::Empty},
      {Action::GoToAccount,                   QStringLiteral("transaction_goto_account"),       i18n("Go to account"),                              Icon::GoJump},
      {Action::GoToPayee,                     QStringLiteral("transaction_goto_payee"),         i18n("Go to payee"),                                Icon::GoJump},
      {Action::NewScheduledTransaction,       QStringLiteral("transaction_create_schedule"),    i18n("Create scheduled transaction..."),            Icon::AppointmentNew},
      {Action::AssignTransactionsNumber,      QStringLiteral("transaction_assign_number"),      i18n("Assign next number"),                         Icon::Empty},
      {Action::CombineTransactions,           QStringLiteral("transaction_combine"),            i18nc("Combine transactions", "Combine"),           Icon::Empty},
      {Action::CopySplits,                    QStringLiteral("transaction_copy_splits"),        i18n("Copy splits"),                                Icon::Empty},
      //Investment
      {Action::NewInvestment,                 QStringLiteral("investment_new"),                 i18n("New investment..."),                          Icon::InvestmentNew},
      {Action::EditInvestment,                QStringLiteral("investment_edit"),                i18n("Edit investment..."),                         Icon::InvestmentEdit},
      {Action::DeleteInvestment,              QStringLiteral("investment_delete"),              i18n("Delete investment..."),                       Icon::InvestmentDelete},
      {Action::UpdatePriceOnline,             QStringLiteral("investment_online_price_update"), i18n("Online price update..."),                     Icon::InvestmentOnlinePrice},
      {Action::UpdatePriceManually,           QStringLiteral("investment_manual_price_update"), i18n("Manual price update..."),                     Icon::Empty},
      //Schedule
      {Action::NewSchedule,                   QStringLiteral("schedule_new"),                   i18n("New scheduled transaction"),                  Icon::AppointmentNew},
      {Action::EditSchedule,                  QStringLiteral("schedule_edit"),                  i18n("Edit scheduled transaction"),                 Icon::DocumentEdit},
      {Action::DeleteSchedule,                QStringLiteral("schedule_delete"),                i18n("Delete scheduled transaction"),               Icon::EditDelete},
      {Action::DuplicateSchedule,             QStringLiteral("schedule_duplicate"),             i18n("Duplicate scheduled transaction"),            Icon::EditCopy},
      {Action::EnterSchedule,                 QStringLiteral("schedule_enter"),                 i18n("Enter next transaction..."),                  Icon::KeyEnter},
      {Action::SkipSchedule,                  QStringLiteral("schedule_skip"),                  i18n("Skip next transaction..."),                   Icon::MediaSeekForward},
      //Payees
      {Action::NewPayee,                      QStringLiteral("payee_new"),                      i18n("New payee"),                                  Icon::ListAddUser},
      {Action::RenamePayee,                   QStringLiteral("payee_rename"),                   i18n("Rename payee"),                               Icon::PayeeRename},
      {Action::DeletePayee,                   QStringLiteral("payee_delete"),                   i18n("Delete payee"),                               Icon::ListRemoveUser},
      {Action::MergePayee,                    QStringLiteral("payee_merge"),                    i18n("Merge payees"),                               Icon::PayeeMerge},
      //Tags
      {Action::NewTag,                        QStringLiteral("tag_new"),                        i18n("New tag"),                                    Icon::ListAddTag},
      {Action::RenameTag,                     QStringLiteral("tag_rename"),                     i18n("Rename tag"),                                 Icon::TagRename},
      {Action::DeleteTag,                     QStringLiteral("tag_delete"),                     i18n("Delete tag"),                                 Icon::ListRemoveTag},
      //debug actions
#ifdef KMM_DEBUG
      {Action::WizardNewUser,                 QStringLiteral("new_user_wizard"),                i18n("Test new feature"),                           Icon::Empty},
      {Action::DebugTraces,                   QStringLiteral("debug_traces"),                   i18n("Debug Traces"),                               Icon::Empty},
#endif
      {Action::DebugTimers,                   QStringLiteral("debug_timers"),                   i18n("Debug Timers"),                               Icon::Empty},
      // onlineJob actions
      {Action::DeleteOnlineJob,               QStringLiteral("onlinejob_delete"),               i18n("Remove credit transfer"),                     Icon::EditDelete},
      {Action::EditOnlineJob,                 QStringLiteral("onlinejob_edit"),                 i18n("Edit credit transfer"),                       Icon::DocumentEdit},
      {Action::LogOnlineJob,                  QStringLiteral("onlinejob_log"),                  i18n("Show log"),                                   Icon::Empty},
    };

    for (const auto& info : actionInfos) {
      auto a = new QAction(this);
      // KActionCollection::addAction by name sets object name anyways,
      // so, as better alternative, set it here right from the start
      a->setObjectName(info.name);
      a->setText(info.text);
      if (info.icon != Icon::Empty) // no need to set empty icon
        a->setIcon(Icons::get(info.icon));
      a->setEnabled(false);
      lutActions.insert(info.action, a);  // store QAction's pointer for later processing
    }
  }

  {
    // List with slots that get connected here. Other slots get connected in e.g. appropriate views
    typedef void(KMyMoneyApp::*KMyMoneyAppFunc)();
    const QHash<eMenu::Action, KMyMoneyAppFunc> actionConnections {
      // *************
      // The File menu
      // *************
      {Action::FileOpenDatabase,              &KMyMoneyApp::slotOpenDatabase},
      {Action::FileSaveAsDatabase,            &KMyMoneyApp::slotSaveAsDatabase},
      {Action::FileBackup,                    &KMyMoneyApp::slotBackupFile},
      {Action::FileImportTemplate,            &KMyMoneyApp::slotLoadAccountTemplates},
      {Action::FileExportTemplate,            &KMyMoneyApp::slotSaveAccountTemplates},
      {Action::FilePersonalData,              &KMyMoneyApp::slotFileViewPersonal},
#ifdef KMM_DEBUG
      {Action::FileDump,                      &KMyMoneyApp::slotFileFileInfo},
#endif
      {Action::FileInformation,               &KMyMoneyApp::slotFileInfoDialog},
      // *************
      // The Edit menu
      // *************
      {Action::EditFindTransaction,           &KMyMoneyApp::slotFindTransaction},
      // *************
      // The View menu
      // *************
      {Action::ViewTransactionDetail,         &KMyMoneyApp::slotShowTransactionDetail},
      {Action::ViewHideReconciled,            &KMyMoneyApp::slotHideReconciledTransactions},
      {Action::ViewHideCategories,            &KMyMoneyApp::slotHideUnusedCategories},
      {Action::ViewShowAll,                   &KMyMoneyApp::slotShowAllAccounts},
      // *****************
      // The accounts menu
      // *****************
      {Action::MapOnlineAccount,              &KMyMoneyApp::slotAccountMapOnline},
      {Action::UnmapOnlineAccount,            &KMyMoneyApp::slotAccountUnmapOnline},
      {Action::UpdateAccount,                 &KMyMoneyApp::slotAccountUpdateOnline},
      {Action::UpdateAllAccounts,             &KMyMoneyApp::slotAccountUpdateOnlineAll},
      // **************
      // The tools menu
      // **************
      {Action::ToolCurrencies,                &KMyMoneyApp::slotCurrencyDialog},
      {Action::ToolPrices,                    &KMyMoneyApp::slotPriceDialog},
      {Action::ToolUpdatePrices,              &KMyMoneyApp::slotEquityPriceUpdate},
      {Action::ToolConsistency,               &KMyMoneyApp::slotFileConsistencyCheck},
      {Action::ToolPerformance,               &KMyMoneyApp::slotPerformanceTest},
      {Action::ToolSQL,                       &KMyMoneyApp::slotGenerateSql},
      {Action::ToolCalculator,                &KMyMoneyApp::slotToolsStartKCalc},
      // *****************
      // The settings menu
      // *****************
      {Action::SettingsAllMessages,           &KMyMoneyApp::slotEnableMessages},
      // *************
      // The help menu
      // *************
      {Action::HelpShow,                      &KMyMoneyApp::slotShowTipOfTheDay},
      // ***************************
      // Actions w/o main menu entry
      // ***************************
      //debug actions
#ifdef KMM_DEBUG
      {Action::WizardNewUser,                 &KMyMoneyApp::slotNewFeature},
      {Action::DebugTraces,                   &KMyMoneyApp::slotToggleTraces},
#endif
      {Action::DebugTimers,                   &KMyMoneyApp::slotToggleTimers},
    };

    for (auto connection = actionConnections.cbegin(); connection != actionConnections.cend(); ++connection)
      connect(lutActions[connection.key()], &QAction::triggered, this, connection.value());
  }

  // *************
  // Setting some of added actions checkable
  // *************
  {
    // Some of acitions schould be checkable,
    // so set them here
    const QVector<Action> checkableActions {
      Action::ViewTransactionDetail, Action::ViewHideReconciled, Action::ViewHideCategories,
    #ifdef KMM_DEBUG
          Action::DebugTraces,
    #endif
          Action::ViewShowAll
    };

    for (const auto& it : checkableActions) {
      lutActions[it]->setCheckable(true);
      lutActions[it]->setEnabled(true);
    }
  }

  // *************
  // Setting keyboard shortcuts for some of added actions
  // *************
  {
    const QVector<QPair<Action, QKeySequence>> actionShortcuts {
      {qMakePair(Action::EditFindTransaction,         Qt::CTRL + Qt::Key_F)},
      {qMakePair(Action::ViewTransactionDetail,       Qt::CTRL + Qt::Key_T)},
      {qMakePair(Action::ViewHideReconciled,          Qt::CTRL + Qt::Key_R)},
      {qMakePair(Action::ViewHideCategories,          Qt::CTRL + Qt::Key_U)},
      {qMakePair(Action::ViewShowAll,                 Qt::CTRL + Qt::SHIFT + Qt::Key_A)},
      {qMakePair(Action::StartReconciliation,         Qt::CTRL + Qt::SHIFT + Qt::Key_R)},
      {qMakePair(Action::NewTransaction,              Qt::CTRL + Qt::Key_Insert)},
      {qMakePair(Action::ToggleReconciliationFlag,    Qt::CTRL + Qt::Key_Space)},
      {qMakePair(Action::MarkCleared,                 Qt::CTRL + Qt::Key_Alt + Qt::Key_Space)},
      {qMakePair(Action::MarkReconciled,              Qt::CTRL + Qt::Key_Shift + Qt::Key_Space)},
      {qMakePair(Action::SelectAllTransactions,       Qt::CTRL + Qt::Key_A)},
#ifdef KMM_DEBUG
      {qMakePair(Action::WizardNewUser,               Qt::CTRL + Qt::Key_G)},
#endif
      {qMakePair(Action::AssignTransactionsNumber,    Qt::CTRL + Qt::Key_Shift + Qt::Key_N)}
    };

    for(const auto& it : actionShortcuts)
      aC->setDefaultShortcut(lutActions[it.first], it.second);
  }

  // *************
  // Misc settings
  // *************
  connect(onlineJobAdministration::instance(), &onlineJobAdministration::canSendCreditTransferChanged,  lutActions.value(Action::AccountCreditTransfer), &QAction::setEnabled);

  // Setup transaction detail switch
  lutActions[Action::ViewTransactionDetail]->setChecked(KMyMoneyGlobalSettings::showRegisterDetailed());
  lutActions[Action::ViewHideReconciled]->setChecked(KMyMoneyGlobalSettings::hideReconciledTransactions());
  lutActions[Action::ViewHideCategories]->setChecked(KMyMoneyGlobalSettings::hideUnusedCategory());
  lutActions[Action::ViewShowAll]->setChecked(false);

  // *************
  // Adding actions to ActionCollection
  // *************
  actionCollection()->addActions(lutActions.values());

  // ************************
  // Currently unused actions
  // ************************
#if 0
  new KToolBarPopupAction(i18n("View back"), "go-previous", 0, this, SLOT(slotShowPreviousView()), actionCollection(), "go_back");
  new KToolBarPopupAction(i18n("View forward"), "go-next", 0, this, SLOT(slotShowNextView()), actionCollection(), "go_forward");

  action("go_back")->setEnabled(false);
  action("go_forward")->setEnabled(false);
#endif

  // use the absolute path to your kmymoneyui.rc file for testing purpose in createGUI();
  setupGUI();

  // reconnect about app entry to dialog with full credits information
  auto aboutApp = aC->action(QString::fromLatin1(KStandardAction::name(KStandardAction::AboutApp)));
  aboutApp->disconnect();
  connect(aboutApp, &QAction::triggered, this, &KMyMoneyApp::slotShowCredits);

  QMenu *menuContainer;
  menuContainer = static_cast<QMenu*>(factory()->container(QStringLiteral("import"), this));
  menuContainer->setIcon(Icons::get(Icon::DocumentImport));

  menuContainer = static_cast<QMenu*>(factory()->container(QStringLiteral("export"), this));
  menuContainer->setIcon(Icons::get(Icon::DocumentExport));
  return lutActions;
}

#ifdef KMM_DEBUG
void KMyMoneyApp::dumpActions() const
{
  const QList<QAction*> list = actionCollection()->actions();
  foreach (const auto it, list)
    std::cout << qPrintable(it->objectName()) << ": " << qPrintable(it->text()) << std::endl;
}
#endif

bool KMyMoneyApp::isActionToggled(const Action _a)
{
  return pActions[_a]->isChecked();
}

void KMyMoneyApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR

  d->m_statusLabel = new QLabel;
  statusBar()->addWidget(d->m_statusLabel);
  ready();

  // Initialization of progress bar taken from KDevelop ;-)
  d->m_progressBar = new QProgressBar;
  statusBar()->addWidget(d->m_progressBar);
  d->m_progressBar->setFixedHeight(d->m_progressBar->sizeHint().height() - 8);

  // hide the progress bar for now
  slotStatusProgressBar(-1, -1);
}

void KMyMoneyApp::saveOptions()
{
  KConfigGroup grp = d->m_config->group("General Options");
  grp.writeEntry("Geometry", size());

  grp.writeEntry("Show Statusbar", actionCollection()->action(KStandardAction::name(KStandardAction::ShowStatusbar))->isChecked());

  KConfigGroup toolbarGrp = d->m_config->group("mainToolBar");
  toolBar("mainToolBar")->saveSettings(toolbarGrp);

  d->m_recentFiles->saveEntries(d->m_config->group("Recent Files"));

}


void KMyMoneyApp::readOptions()
{
  KConfigGroup grp = d->m_config->group("General Options");


  pActions[Action::ViewHideReconciled]->setChecked(KMyMoneyGlobalSettings::hideReconciledTransactions());
  pActions[Action::ViewHideCategories]->setChecked(KMyMoneyGlobalSettings::hideUnusedCategory());

  d->m_recentFiles->loadEntries(d->m_config->group("Recent Files"));

  // Startdialog is written in the settings dialog
  d->m_startDialog = grp.readEntry("StartDialog", true);
}

void KMyMoneyApp::resizeEvent(QResizeEvent* ev)
{
  KMainWindow::resizeEvent(ev);
  updateCaption(true);
}

int KMyMoneyApp::askSaveOnClose()
{
  int ans;
  if (KMyMoneyGlobalSettings::autoSaveOnClose()) {
    ans = KMessageBox::Yes;
  } else {
    ans = KMessageBox::warningYesNoCancel(this, i18n("The file has been changed, save it?"));
  }
  return ans;
}

bool KMyMoneyApp::queryClose()
{
  if (!isReady())
    return false;

  if (d->m_myMoneyView->dirty()) {
    int ans = askSaveOnClose();

    if (ans == KMessageBox::Cancel)
      return false;
    else if (ans == KMessageBox::Yes) {
      bool saved = slotFileSave();
      saveOptions();
      return saved;
    }
  }
//  if (d->m_myMoneyView->isDatabase())
//    slotFileClose(); // close off the database
  saveOptions();
  return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////
void KMyMoneyApp::slotFileInfoDialog()
{
  QPointer<KMyMoneyFileInfoDlg> dlg = new KMyMoneyFileInfoDlg(0);
  dlg->exec();
  delete dlg;
}

void KMyMoneyApp::slotPerformanceTest()
{
  // dump performance report to stderr

  int measurement[2];
  QTime timer;
  MyMoneyAccount acc;

  qDebug("--- Starting performance tests ---");

  // AccountList
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  timer.start();
  for (int i = 0; i < 1000; ++i) {
    QList<MyMoneyAccount> list;

    MyMoneyFile::instance()->accountList(list);
    measurement[i != 0] = timer.elapsed();
  }
  std::cerr << "accountList()" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // Balance of asset account(s)
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->asset();
  for (int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->balance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "balance(Asset)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // total balance of asset account
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->asset();
  for (int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->totalBalance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "totalBalance(Asset)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // Balance of expense account(s)
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->expense();
  for (int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->balance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "balance(Expense)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // total balance of expense account
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->expense();
  timer.start();
  for (int i = 0; i < 1000; ++i) {
    MyMoneyMoney result = MyMoneyFile::instance()->totalBalance(acc.id());
    measurement[i != 0] = timer.elapsed();
  }
  std::cerr << "totalBalance(Expense)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // transaction list
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  if (MyMoneyFile::instance()->asset().accountCount()) {
    MyMoneyTransactionFilter filter(MyMoneyFile::instance()->asset().accountList()[0]);
    filter.setDateFilter(QDate(), QDate::currentDate());
    QList<MyMoneyTransaction> list;

    timer.start();
    for (int i = 0; i < 100; ++i) {
      list = MyMoneyFile::instance()->transactionList(filter);
      measurement[i != 0] = timer.elapsed();
    }
    std::cerr << "transactionList()" << std::endl;
    std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
    std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
    std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 100 << " msec" << std::endl;
  }

  // transaction list
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  if (MyMoneyFile::instance()->asset().accountCount()) {
    MyMoneyTransactionFilter filter(MyMoneyFile::instance()->asset().accountList()[0]);
    filter.setDateFilter(QDate(), QDate::currentDate());
    QList<MyMoneyTransaction> list;

    timer.start();
    for (int i = 0; i < 100; ++i) {
      MyMoneyFile::instance()->transactionList(list, filter);
      measurement[i != 0] = timer.elapsed();
    }
    std::cerr << "transactionList(list)" << std::endl;
    std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
    std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
    std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 100 << " msec" << std::endl;
  }
  MyMoneyFile::instance()->preloadCache();
}

void KMyMoneyApp::slotFileNew()
{
  KMSTATUS(i18n("Creating new document..."));

  slotFileClose();

  if (!d->m_myMoneyView->fileOpen()) {
    // next line required until we move all file handling out of KMyMoneyView
    d->m_myMoneyView->newFile();

    d->m_fileName = QUrl();
    updateCaption();

    NewUserWizard::Wizard *wizard = new NewUserWizard::Wizard();

    if (wizard->exec() == QDialog::Accepted) {
      MyMoneyFileTransaction ft;
      MyMoneyFile* file = MyMoneyFile::instance();
      try {
        // store the user info
        file->setUser(wizard->user());

        // create and setup base currency
        file->addCurrency(wizard->baseCurrency());
        file->setBaseCurrency(wizard->baseCurrency());

        // create a possible institution
        MyMoneyInstitution inst = wizard->institution();
        if (inst.name().length()) {
          file->addInstitution(inst);
        }

        // create a possible checking account
        auto acc = wizard->account();
        if (acc.name().length()) {
          acc.setInstitutionId(inst.id());
          MyMoneyAccount asset = file->asset();
          file->addAccount(acc, asset);

          // create possible opening balance transaction
          if (!wizard->openingBalance().isZero()) {
            file->createOpeningBalanceTransaction(acc, wizard->openingBalance());
          }
        }

        // import the account templates
        QList<MyMoneyTemplate> templates = wizard->templates();
        QList<MyMoneyTemplate>::iterator it_t;
        for (it_t = templates.begin(); it_t != templates.end(); ++it_t) {
          (*it_t).importTemplate(&progressCallback);
        }

        d->m_fileName = wizard->url();
        ft.commit();
        KMyMoneyGlobalSettings::setFirstTimeRun(false);

        // FIXME This is a bit clumsy. We re-read the freshly
        // created file to be able to run through all the
        // fixup logic and then save it to keep the modified
        // flag off.
        slotFileSave();
        d->m_myMoneyView->readFile(d->m_fileName);
        slotFileSave();

        // now keep the filename in the recent files used list
        //KRecentFilesAction *p = dynamic_cast<KRecentFilesAction*>(action(KStandardAction::name(KStandardAction::OpenRecent)));
        //if(p)
        d->m_recentFiles->addUrl(d->m_fileName);
        writeLastUsedFile(d->m_fileName.url());

      } catch (const MyMoneyException &) {
        // next line required until we move all file handling out of KMyMoneyView
        d->m_myMoneyView->closeFile();
      }
      if (wizard->startSettingsAfterFinished())
        slotSettings();
    } else {
      // next line required until we move all file handling out of KMyMoneyView
      d->m_myMoneyView->closeFile();
    }
    delete wizard;
    updateCaption();

    emit fileLoaded(d->m_fileName);
  }
}

// General open
void KMyMoneyApp::slotFileOpen()
{
  KMSTATUS(i18n("Open a file."));

  QString prevDir = readLastUsedDir();
  QPointer<QFileDialog> dialog = new QFileDialog(this, QString(), prevDir,
                                                 i18n("KMyMoney files (*.kmy *.xml);;All files"));
  dialog->setFileMode(QFileDialog::ExistingFile);
  dialog->setAcceptMode(QFileDialog::AcceptOpen);

  if (dialog->exec() == QDialog::Accepted && dialog != nullptr) {
    slotFileOpenRecent(dialog->selectedUrls().first());
  }
  delete dialog;
}

void KMyMoneyApp::slotOpenDatabase()
{
  KMSTATUS(i18n("Open a file."));
  QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::ReadWrite);
  if (!dialog->checkDrivers()) {
    delete dialog;
    return;
  }

  if (dialog->exec() == QDialog::Accepted && dialog != 0) {
    slotFileOpenRecent(dialog->selectedURL());
  }
  delete dialog;
}

bool KMyMoneyApp::isImportableFile(const QUrl &url)
{
  bool result = false;

  // Iterate through the plugins and see if there's a loaded plugin who can handle it
  QMap<QString, KMyMoneyPlugin::ImporterPlugin*>::const_iterator it_plugin = d->m_plugins.importer.constBegin();
  while (it_plugin != d->m_plugins.importer.constEnd()) {
    if ((*it_plugin)->isMyFormat(url.path())) {
      result = true;
      break;
    }
    ++it_plugin;
  }

  // If we did not find a match, try importing it as a KMM statement file,
  // which is really just for testing.  the statement file is not exposed
  // to users.
  if (it_plugin == d->m_plugins.importer.constEnd())
    if (MyMoneyStatement::isStatementFile(url.path()))
      result = true;

  // Place code here to test for QIF and other locally-supported formats
  // (i.e. not a plugin). If you add them here, be sure to add it to
  // the webConnect function.

  return result;
}


void KMyMoneyApp::slotFileOpenRecent(const QUrl &url)
{
  KMSTATUS(i18n("Loading file..."));
  QUrl lastFile = d->m_fileName;

  // check if there are other instances which might have this file open
  QList<QString> list = instanceList();
  QList<QString>::ConstIterator it;
  bool duplicate = false;
#ifdef KMM_DBUS
  for (it = list.constBegin(); duplicate == false && it != list.constEnd(); ++it) {
    QDBusInterface remoteApp(*it, "/KMymoney", "org.kde.kmymoney");
    QDBusReply<QString> reply = remoteApp.call("filename");
    if (!reply.isValid()) {
      qDebug("D-Bus error while calling app->filename()");
    } else {
      if (reply.value() == url.url()) {
        duplicate = true;
      }
    }
  }
#endif
  if (!duplicate) {
    QUrl newurl = url;
    if ((newurl.scheme() == QLatin1String("sql"))) {
      const QString key = QLatin1String("driver");
      // take care and convert some old url to their new counterpart
      QUrlQuery query(newurl);
      if (query.queryItemValue(key) == QLatin1String("QMYSQL3")) { // fix any old urls
        query.removeQueryItem(key);
        query.addQueryItem(key, QLatin1String("QMYSQL"));
      }
      if (query.queryItemValue(key) == QLatin1String("QSQLITE3")) {
        query.removeQueryItem(key);
        query.addQueryItem(key, QLatin1String("QSQLITE"));
      }
      newurl.setQuery(query);

      if (query.queryItemValue(key) == QLatin1String("QSQLITE")) {
        newurl.setUserInfo(QString());
        newurl.setHost(QString());
      }
      // check if a password is needed. it may be if the URL came from the last/recent file list
      QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::ReadWrite, newurl);
      if (!dialog->checkDrivers()) {
        delete dialog;
        return;
      }
      // if we need to supply a password, then show the dialog
      // otherwise it isn't needed
      if ((query.queryItemValue("secure").toLower() == QLatin1String("yes")) && newurl.password().isEmpty()) {
        if (dialog->exec() == QDialog::Accepted && dialog != nullptr) {
          newurl = dialog->selectedURL();
        } else {
          delete dialog;
          return;
        }
      }
      delete dialog;
    }

    if (newurl.scheme() == QLatin1String("sql") || KMyMoneyUtils::fileExists(newurl)) {
      slotFileClose();
      if (!d->m_myMoneyView->fileOpen()) {
        try {
          if (d->m_myMoneyView->readFile(newurl)) {
            if ((d->m_myMoneyView->isNativeFile())) {
              d->m_fileName = newurl;
              updateCaption();
              d->m_recentFiles->addUrl(newurl);
              writeLastUsedFile(newurl.toDisplayString(QUrl::PreferLocalFile));
            } else {
              d->m_fileName = QUrl(); // imported files have no filename
            }
            // Check the schedules
            slotCheckSchedules();
          }
        } catch (const MyMoneyException &e) {
          KMessageBox::sorry(this, i18n("Cannot open file as requested. Error was: %1", e.what()));
        }
        updateCaption();
        emit fileLoaded(d->m_fileName);
      } else {
        /*fileOpen failed - should we do something
         or maybe fileOpen puts out the message... - it does for database*/
      }
    } else { // newurl invalid
      slotFileClose();
      KMessageBox::sorry(this, i18n("<p><b>%1</b> is either an invalid filename or the file does not exist. You can open another file or create a new one.</p>", url.toDisplayString(QUrl::PreferLocalFile)), i18n("File not found"));
    }
  } else { // isDuplicate
    KMessageBox::sorry(this, i18n("<p>File <b>%1</b> is already opened in another instance of KMyMoney</p>", url.toDisplayString(QUrl::PreferLocalFile)), i18n("Duplicate open"));
  }
}

bool KMyMoneyApp::slotFileSave()
{
  // if there's nothing changed, there's no need to save anything
  if (!d->m_myMoneyView->dirty())
    return true;

  bool rc = false;

  KMSTATUS(i18n("Saving file..."));

  if (d->m_fileName.isEmpty())
    return slotFileSaveAs();

  d->consistencyCheck(false);

  setEnabled(false);
  if (d->m_myMoneyView->isDatabase())
    rc = d->m_myMoneyView->saveDatabase(d->m_fileName);
  else
    rc = d->m_myMoneyView->saveFile(d->m_fileName, MyMoneyFile::instance()->value("kmm-encryption-key"));
  setEnabled(true);

  d->m_autoSaveTimer->stop();

  updateCaption();
  return rc;
}

void KMyMoneyApp::slotFileSaveAsFilterChanged(const QString& filter)
{
  if (!d->m_saveEncrypted)
    return;
  if (filter.compare(QLatin1String("*.kmy"), Qt::CaseInsensitive) != 0) {
    d->m_saveEncrypted->setCurrentItem(0);
    d->m_saveEncrypted->setEnabled(false);
  } else {
    d->m_saveEncrypted->setEnabled(true);
  }
}

void KMyMoneyApp::slotManageGpgKeys()
{
  QPointer<KGpgKeySelectionDlg> dlg = new KGpgKeySelectionDlg(this);
  dlg->setKeys(d->m_additionalGpgKeys);
  if (dlg->exec() == QDialog::Accepted && dlg != 0) {
    d->m_additionalGpgKeys = dlg->keys();
    d->m_additionalKeyLabel->setText(i18n("Additional encryption keys to be used: %1", d->m_additionalGpgKeys.count()));
  }
  delete dlg;
}

void KMyMoneyApp::slotKeySelected(int idx)
{
  int cnt = 0;
  if (idx != 0) {
    cnt = d->m_additionalGpgKeys.count();
  }
  d->m_additionalKeyLabel->setEnabled(idx != 0);
  d->m_additionalKeyButton->setEnabled(idx != 0);
  d->m_additionalKeyLabel->setText(i18n("Additional encryption keys to be used: %1", cnt));
}

bool KMyMoneyApp::slotFileSaveAs()
{
  bool rc = false;
  KMSTATUS(i18n("Saving file with a new filename..."));

  // fill the additional key list with the default
  d->m_additionalGpgKeys = KMyMoneyGlobalSettings::gpgRecipientList();

  QWidget* vbox = new QWidget();
  QVBoxLayout *vboxVBoxLayout = new QVBoxLayout(vbox);
  vboxVBoxLayout->setMargin(0);
  if (KGPGFile::GPGAvailable()) {
    QWidget* keyBox = new QWidget(vbox);
    QHBoxLayout *keyBoxHBoxLayout = new QHBoxLayout(keyBox);
    keyBoxHBoxLayout->setMargin(0);
    vboxVBoxLayout->addWidget(keyBox);
    QLabel *keyLabel = new QLabel(i18n("Encryption key to be used"), keyBox);
    keyBoxHBoxLayout->addWidget(keyLabel);
    d->m_saveEncrypted = new KComboBox(keyBox);
    keyBoxHBoxLayout->addWidget(d->m_saveEncrypted);

    QWidget* labelBox = new QWidget(vbox);
    QHBoxLayout *labelBoxHBoxLayout = new QHBoxLayout(labelBox);
    labelBoxHBoxLayout->setMargin(0);
    vboxVBoxLayout->addWidget(labelBox);
    d->m_additionalKeyLabel = new QLabel(i18n("Additional encryption keys to be used: %1", d->m_additionalGpgKeys.count()), labelBox);
    labelBoxHBoxLayout->addWidget(d->m_additionalKeyLabel);
    d->m_additionalKeyButton = new QPushButton(i18n("Manage additional keys"), labelBox);
    labelBoxHBoxLayout->addWidget(d->m_additionalKeyButton);
    connect(d->m_additionalKeyButton, SIGNAL(clicked()), this, SLOT(slotManageGpgKeys()));
    connect(d->m_saveEncrypted, SIGNAL(activated(int)), this, SLOT(slotKeySelected(int)));

    // fill the secret key list and combo box
    QStringList keyList;
    KGPGFile::secretKeyList(keyList);
    d->m_saveEncrypted->addItem(i18n("No encryption"));

    for (QStringList::iterator it = keyList.begin(); it != keyList.end(); ++it) {
      QStringList fields = (*it).split(':', QString::SkipEmptyParts);
      if (fields[0] != recoveryKeyId) {
        // replace parenthesis in name field with brackets
        auto name = fields[1];
        name.replace('(', "[");
        name.replace(')', "]");
        name = QString("%1 (0x%2)").arg(name).arg(fields[0]);
        d->m_saveEncrypted->addItem(name);
        if (name.contains(KMyMoneyGlobalSettings::gpgRecipient())) {
          d->m_saveEncrypted->setCurrentItem(name);
        }
      }
    }
  }

  QString prevDir; // don't prompt file name if not a native file
  if (d->m_myMoneyView->isNativeFile())
    prevDir = readLastUsedDir();

  QPointer<QFileDialog> dlg =
    new QFileDialog(this, i18n("Save As"), prevDir,
                    QString(QLatin1String("%2 (%1);;")).arg(QStringLiteral("*.kmy")).arg(i18nc("KMyMoney (Filefilter)", "KMyMoney files")) +
                    QString(QLatin1String("%2 (%1);;")).arg(QStringLiteral("*.xml")).arg(i18nc("XML (Filefilter)", "XML files")) +
                    QString(QLatin1String("%2 (%1);;")).arg(QStringLiteral("*.anon.xml")).arg(i18nc("Anonymous (Filefilter)", "Anonymous files")) +
                    QString(QLatin1String("%2 (%1);;")).arg(QStringLiteral("*")).arg(i18nc("All files (Filefilter)", "All files")));
  dlg->setAcceptMode(QFileDialog::AcceptSave);
  connect(dlg, SIGNAL(filterChanged(QString)), this, SLOT(slotFileSaveAsFilterChanged(QString)));

  if (dlg->exec() == QDialog::Accepted && dlg != 0) {
    QUrl newURL = dlg->selectedUrls().first();
    if (!newURL.fileName().isEmpty()) {
      d->consistencyCheck(false);
      // deleting the dialog will delete the combobox pointed to by d->m_saveEncrypted so get the key name here
      QString selectedKeyName;
      if (d->m_saveEncrypted && d->m_saveEncrypted->currentIndex() != 0)
        selectedKeyName = d->m_saveEncrypted->currentText();

      d->m_saveEncrypted = 0;

      QString newName = newURL.toDisplayString(QUrl::PreferLocalFile);

      // append extension if not present
      if (!newName.endsWith(QLatin1String(".kmy"), Qt::CaseInsensitive) &&
          !newName.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive))
        newName.append(QLatin1String(".kmy"));
      newURL = QUrl::fromUserInput(newName);
      d->m_recentFiles->addUrl(newURL);

      setEnabled(false);
      // If this is the anonymous file export, just save it, don't actually take the
      // name, or remember it! Don't even try to encrypt it
      if (newName.endsWith(QLatin1String(".anon.xml"), Qt::CaseInsensitive))
        rc = d->m_myMoneyView->saveFile(newURL);
      else {
        d->m_fileName = newURL;
        QString encryptionKeys;
        QRegExp keyExp(".* \\((.*)\\)");
        if (keyExp.indexIn(selectedKeyName) != -1) {
          encryptionKeys = keyExp.cap(1);
        }
        if (!d->m_additionalGpgKeys.isEmpty()) {
          if (!encryptionKeys.isEmpty())
            encryptionKeys.append(QLatin1Char(','));
          encryptionKeys.append(d->m_additionalGpgKeys.join(QLatin1Char(',')));
        }
        rc = d->m_myMoneyView->saveFile(d->m_fileName, encryptionKeys);
        //write the directory used for this file as the default one for next time.
        writeLastUsedDir(newURL.toDisplayString(QUrl::RemoveFilename | QUrl::PreferLocalFile | QUrl::StripTrailingSlash));
        writeLastUsedFile(newName);
      }
      d->m_autoSaveTimer->stop();
      setEnabled(true);
    }
  }

  delete dlg;
  updateCaption();
  return rc;
}

void KMyMoneyApp::slotSaveAsDatabase()
{
  saveAsDatabase();
}

bool KMyMoneyApp::saveAsDatabase()
{
  bool rc = false;
  QUrl oldUrl;
  // in event of it being a database, ensure that all data is read into storage for saveas
  if (d->m_myMoneyView->isDatabase())
    oldUrl = d->m_fileName.isEmpty() ? lastOpenedURL() : d->m_fileName;

  KMSTATUS(i18n("Saving file to database..."));
  QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::WriteOnly);
  QUrl url = oldUrl;
  if (!dialog->checkDrivers()) {
    delete dialog;
    return (false);
  }

  while (oldUrl == url && dialog->exec() == QDialog::Accepted && dialog != 0) {
    url = dialog->selectedURL();
    // If the protocol is SQL for the old and new, and the hostname and database names match
    // Let the user know that the current database cannot be saved on top of itself.
    if (url.scheme() == "sql" && oldUrl.scheme() == "sql"
        && oldUrl.host() == url.host()
        && QUrlQuery(oldUrl).queryItemValue("driver") == QUrlQuery(url).queryItemValue("driver")
        && oldUrl.path().right(oldUrl.path().length() - 1) == url.path().right(url.path().length() - 1)) {
      KMessageBox::sorry(this, i18n("Cannot save to current database."));
    } else {
      try {
        rc = d->m_myMoneyView->saveAsDatabase(url);
      } catch (const MyMoneyException &e) {
        KMessageBox::sorry(this, i18n("Cannot save to current database: %1", e.what()));
      }
    }
  }
  delete dialog;

  if (rc) {
    //KRecentFilesAction *p = dynamic_cast<KRecentFilesAction*>(action("file_open_recent"));
    //if(p)
    d->m_recentFiles->addUrl(url);
    writeLastUsedFile(url.toDisplayString(QUrl::PreferLocalFile));
  }
  d->m_autoSaveTimer->stop();
  updateCaption();
  return rc;
}

void KMyMoneyApp::slotFileCloseWindow()
{
  KMSTATUS(i18n("Closing window..."));

  if (d->m_myMoneyView->dirty()) {
    int answer = askSaveOnClose();
    if (answer == KMessageBox::Cancel)
      return;
    else if (answer == KMessageBox::Yes)
      slotFileSave();
  }
  close();
}

void KMyMoneyApp::slotFileClose()
{
  bool okToSelect = true;

  // check if transaction editor is open and ask user what he wants to do
//  slotTransactionsCancelOrEnter(okToSelect);

  if (!okToSelect)
    return;

  // no update status here, as we might delete the status too early.
  if (d->m_myMoneyView->dirty()) {
    int answer = askSaveOnClose();
    if (answer == KMessageBox::Cancel)
      return;
    else if (answer == KMessageBox::Yes)
      slotFileSave();
  }

  d->closeFile();
}

void KMyMoneyApp::slotFileQuit()
{
  // don't modify the status message here as this will prevent quit from working!!
  // See the beginning of queryClose() and isReady() why. Thomas Baumgart 2005-10-17

  bool quitApplication = true;

  QList<KMainWindow*> memberList = KMainWindow::memberList();
  if (!memberList.isEmpty()) {

    QList<KMainWindow*>::const_iterator w_it = memberList.constBegin();
    for (; w_it != memberList.constEnd(); ++w_it) {
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
      // the window and the application stay open.
      if (!(*w_it)->close()) {
        quitApplication = false;
        break;
      }
    }
  }

  // We will only quit if all windows were processed and not cancelled
  if (quitApplication) {
    QCoreApplication::quit();
  }
}

void KMyMoneyApp::slotShowTransactionDetail()
{

}

void KMyMoneyApp::slotHideReconciledTransactions()
{
  KMyMoneyGlobalSettings::setHideReconciledTransactions(pActions[Action::ViewHideReconciled]->isChecked());
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoneyApp::slotHideUnusedCategories()
{
  KMyMoneyGlobalSettings::setHideUnusedCategory(pActions[Action::ViewHideCategories]->isChecked());
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoneyApp::slotShowAllAccounts()
{
  KMyMoneyGlobalSettings::setShowAllAccounts(pActions[Action::ViewShowAll]->isChecked());
  d->m_myMoneyView->slotRefreshViews();
}

#ifdef KMM_DEBUG
void KMyMoneyApp::slotFileFileInfo()
{
  if (!d->m_myMoneyView->fileOpen()) {
    KMessageBox::information(this, i18n("No KMyMoneyFile open"));
    return;
  }

  QFile g("kmymoney.dump");
  g.open(QIODevice::WriteOnly);
  QDataStream st(&g);
  MyMoneyStorageDump dumper;
  dumper.writeStream(st, MyMoneyFile::instance()->storage());
  g.close();
}

void KMyMoneyApp::slotToggleTraces()
{
  MyMoneyTracer::onOff(pActions[Action::DebugTraces]->isChecked() ? 1 : 0);
}
#endif

void KMyMoneyApp::slotToggleTimers()
{
  extern bool timersOn; // main.cpp

  timersOn = pActions[Action::DebugTimers]->isChecked();
}

QString KMyMoneyApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  QString previousMessage = d->m_statusLabel->text();
  d->m_applicationIsReady = false;

  QString currentMessage = text;
  if (currentMessage.isEmpty() || currentMessage == i18nc("Application is ready to use", "Ready.")) {
    d->m_applicationIsReady = true;
    currentMessage = i18nc("Application is ready to use", "Ready.");
  }
  statusBar()->clearMessage();
  d->m_statusLabel->setText(currentMessage);
  return previousMessage;
}

void KMyMoneyApp::ready()
{
  slotStatusMsg(QString());
}

bool KMyMoneyApp::isReady()
{
  return d->m_applicationIsReady;
}

void KMyMoneyApp::slotStatusProgressBar(int current, int total)
{
  if (total == -1 && current == -1) {     // reset
    if (d->m_progressTimer) {
      d->m_progressTimer->start(500);     // remove from screen in 500 msec
      d->m_progressBar->setValue(d->m_progressBar->maximum());
    }

  } else if (total != 0) {                // init
    d->m_progressTimer->stop();
    d->m_progressBar->setMaximum(total);
    d->m_progressBar->setValue(0);
    d->m_progressBar->show();

  } else {                                // update
    QTime currentTime = QTime::currentTime();
    // only process painting if last update is at least 250 ms ago
    if (abs(d->m_lastUpdate.msecsTo(currentTime)) > 250) {
      d->m_progressBar->setValue(current);
      d->m_lastUpdate = currentTime;
    }
  }
}

void KMyMoneyApp::slotStatusProgressDone()
{
  d->m_progressTimer->stop();
  d->m_progressBar->reset();
  d->m_progressBar->hide();
  d->m_progressBar->setValue(0);
}

void KMyMoneyApp::progressCallback(int current, int total, const QString& msg)
{
  if (!msg.isEmpty())
    kmymoney->slotStatusMsg(msg);

  kmymoney->slotStatusProgressBar(current, total);
}

void KMyMoneyApp::slotFileViewPersonal()
{
  if (!d->m_myMoneyView->fileOpen()) {
    KMessageBox::information(this, i18n("No KMyMoneyFile open"));
    return;
  }

  KMSTATUS(i18n("Viewing personal data..."));

  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyPayee user = file->user();

  QPointer<EditPersonalDataDlg> editPersonalDataDlg = new EditPersonalDataDlg(user.name(), user.address(),
      user.city(), user.state(), user.postcode(), user.telephone(),
      user.email(), this, i18n("Edit Personal Data"));

  if (editPersonalDataDlg->exec() == QDialog::Accepted && editPersonalDataDlg != 0) {
    user.setName(editPersonalDataDlg->userName());
    user.setAddress(editPersonalDataDlg->userStreet());
    user.setCity(editPersonalDataDlg->userTown());
    user.setState(editPersonalDataDlg->userCountry());
    user.setPostcode(editPersonalDataDlg->userPostcode());
    user.setTelephone(editPersonalDataDlg->userTelephone());
    user.setEmail(editPersonalDataDlg->userEmail());
    MyMoneyFileTransaction ft;
    try {
      file->setUser(user);
      ft.commit();
    } catch (const MyMoneyException &e) {
      KMessageBox::information(this, i18n("Unable to store user information: %1", e.what()));
    }
  }
  delete editPersonalDataDlg;
}

void KMyMoneyApp::slotLoadAccountTemplates()
{
  KMSTATUS(i18n("Importing account templates."));

  int rc;
  QPointer<KLoadTemplateDlg> dlg = new KLoadTemplateDlg();
  if ((rc = dlg->exec()) == QDialog::Accepted && dlg != 0) {
    MyMoneyFileTransaction ft;
    try {
      // import the account templates
      QList<MyMoneyTemplate> templates = dlg->templates();
      QList<MyMoneyTemplate>::iterator it_t;
      for (it_t = templates.begin(); it_t != templates.end(); ++it_t) {
        (*it_t).importTemplate(&progressCallback);
      }
      ft.commit();
    } catch (const MyMoneyException &e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to import template(s): %1, thrown in %2:%3", e.what(), e.file(), e.line()));
    }
  }
  delete dlg;
}

void KMyMoneyApp::slotSaveAccountTemplates()
{
  KMSTATUS(i18n("Exporting account templates."));

  QString savePath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/templates/" + QLocale().name();
  QDir d(savePath);
  if (!d.exists())
      d.mkpath(savePath);
  QString newName = QFileDialog::getSaveFileName(this, i18n("Save as..."), savePath,
                    i18n("KMyMoney template files (*.kmt);;All files (*)"));

  //
  // If there is no file extension, then append a .kmt at the end of the file name.
  // If there is a file extension, make sure it is .kmt, delete any others.
  //
  if (!newName.isEmpty()) {
    // find last . delimiter
    int nLoc = newName.lastIndexOf('.');
    if (nLoc != -1) {
      QString strExt, strTemp;
      strTemp = newName.left(nLoc + 1);
      strExt = newName.right(newName.length() - (nLoc + 1));
      if ((strExt.indexOf("kmt", 0, Qt::CaseInsensitive) == -1)) {
        strTemp.append("kmt");
        //append to make complete file name
        newName = strTemp;
      }
    } else {
      newName.append(".kmt");
    }

    if (okToWriteFile(QUrl::fromLocalFile(newName))) {
      QPointer <KTemplateExportDlg> dlg = new KTemplateExportDlg(this);
      if (dlg->exec() == QDialog::Accepted && dlg) {
          MyMoneyTemplate templ;
          templ.setTitle(dlg->title());
          templ.setShortDescription(dlg->shortDescription());
          templ.setLongDescription(dlg->longDescription());
          templ.exportTemplate(&progressCallback);
          templ.saveTemplate(QUrl::fromLocalFile(newName));
      }
      delete dlg;
    }
  }
}

bool KMyMoneyApp::okToWriteFile(const QUrl &url)
{
  Q_UNUSED(url)

  // check if the file exists and warn the user
  bool reallySaveFile = true;

  if (KMyMoneyUtils::fileExists(url)) {
    if (KMessageBox::warningYesNo(this, QLatin1String("<qt>") + i18n("The file <b>%1</b> already exists. Do you really want to overwrite it?", url.toDisplayString(QUrl::PreferLocalFile)) + QLatin1String("</qt>"), i18n("File already exists")) != KMessageBox::Yes)
    reallySaveFile = false;
  }
  return reallySaveFile;
}

void KMyMoneyApp::slotSettings()
{
  // if we already have an instance of the settings dialog, then use it
  if (KConfigDialog::showDialog("KMyMoney-Settings"))
    return;

  // otherwise, we have to create it
  auto dlg = new KSettingsKMyMoney(this, "KMyMoney-Settings", KMyMoneyGlobalSettings::self());
  connect(dlg, &KSettingsKMyMoney::settingsChanged, this, &KMyMoneyApp::slotUpdateConfiguration);
  dlg->show();
}

void KMyMoneyApp::slotShowCredits()
{
  KAboutData aboutData = initializeCreditsData();
  KAboutApplicationDialog dlg(aboutData, this);
  dlg.exec();
}

void KMyMoneyApp::slotUpdateConfiguration(const QString &dialogName)
{
  if(dialogName.compare(QLatin1String("Plugins")) == 0) {
    KMyMoneyPlugin::pluginHandling(KMyMoneyPlugin::Action::Reorganize, d->m_plugins, this, guiFactory());
    onlineJobAdministration::instance()->updateActions();
    return;
  }
  MyMoneyTransactionFilter::setFiscalYearStart(KMyMoneyGlobalSettings::firstFiscalMonth(), KMyMoneyGlobalSettings::firstFiscalDay());

#ifdef ENABLE_UNFINISHEDFEATURES
  LedgerSeparator::setFirstFiscalDate(KMyMoneyGlobalSettings::firstFiscalMonth(), KMyMoneyGlobalSettings::firstFiscalDay());
#endif

  d->m_myMoneyView->updateViewType();

  // update the sql storage module settings
  MyMoneyStorageSql::setStartDate(KMyMoneyGlobalSettings::startDate().date());

  // update the report module settings
  MyMoneyReport::setLineWidth(KMyMoneyGlobalSettings::lineWidth());

  // update the holiday region configuration
  setHolidayRegion(KMyMoneyGlobalSettings::holidayRegion());

  d->m_myMoneyView->slotRefreshViews();

  // re-read autosave configuration
  d->m_autoSaveEnabled = KMyMoneyGlobalSettings::autoSaveFile();
  d->m_autoSavePeriod = KMyMoneyGlobalSettings::autoSavePeriod();

  // stop timer if turned off but running
  if (d->m_autoSaveTimer->isActive() && !d->m_autoSaveEnabled) {
    d->m_autoSaveTimer->stop();
  }
  // start timer if turned on and needed but not running
  if (!d->m_autoSaveTimer->isActive() && d->m_autoSaveEnabled && d->m_myMoneyView->dirty()) {
    d->m_autoSaveTimer->setSingleShot(true);
    d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);
  }

  d->setThemedCSS();

  // check if the recovery key is still valid or expires soon

  if (KMyMoneySettings::writeDataEncrypted() && KMyMoneySettings::encryptRecover()) {
    if (KGPGFile::GPGAvailable()) {
      KGPGFile file;
      QDateTime expirationDate = file.keyExpires(QLatin1String(recoveryKeyId));
      if (expirationDate.isValid() && QDateTime::currentDateTime().daysTo(expirationDate) <= RECOVER_KEY_EXPIRATION_WARNING) {
        bool skipMessage = false;

        //get global config object for our app.
        KSharedConfigPtr kconfig = KSharedConfig::openConfig();
        KConfigGroup grp;
        QDate lastWarned;
        if (kconfig) {
          grp = d->m_config->group("General Options");
          lastWarned = grp.readEntry("LastRecoverKeyExpirationWarning", QDate());
          if (QDate::currentDate() == lastWarned) {
            skipMessage = true;
          }
        }
        if (!skipMessage) {
          if (kconfig) {
            grp.writeEntry("LastRecoverKeyExpirationWarning", QDate::currentDate());
          }
          KMessageBox::information(this, i18np("You have configured KMyMoney to use GPG to protect your data and to encrypt your data also with the KMyMoney recover key. This key is about to expire in %1 day. Please update the key from a keyserver using your GPG frontend (e.g. KGPG).", "You have configured KMyMoney to use GPG to protect your data and to encrypt your data also with the KMyMoney recover key. This key is about to expire in %1 days. Please update the key from a keyserver using your GPG frontend (e.g. KGPG).", QDateTime::currentDateTime().daysTo(expirationDate)), i18n("Recover key expires soon"));
        }
      }
    }
  }
}

void KMyMoneyApp::slotBackupFile()
{
  // Save the file first so isLocalFile() works
  if (d->m_myMoneyView && d->m_myMoneyView->dirty())

  {
    if (KMessageBox::questionYesNo(this, i18n("The file must be saved first "
                                   "before it can be backed up.  Do you want to continue?")) == KMessageBox::No) {
      return;

    }

    slotFileSave();
  }



  if (d->m_fileName.isEmpty())
    return;

  if (!d->m_fileName.isLocalFile()) {
    KMessageBox::sorry(this,
                       i18n("The current implementation of the backup functionality only supports local files as source files. Your current source file is '%1'.", d->m_fileName.url()),

                       i18n("Local files only"));
    return;
  }

  QPointer<KBackupDlg> backupDlg = new KBackupDlg(this);
  int returncode = backupDlg->exec();
  if (returncode == QDialog::Accepted && backupDlg != 0) {


    d->m_backupMount = backupDlg->mountCheckBox();
    d->m_proc.clearProgram();
    d->m_backupState = BACKUP_MOUNTING;
    d->m_mountpoint = backupDlg->mountPoint();

    if (d->m_backupMount) {
      slotBackupMount();
    } else {
      progressCallback(0, 300, "");
#ifdef Q_OS_WIN
      d->m_ignoreBackupExitCode = true;
      QTimer::singleShot(0, this, SLOT(slotBackupHandleEvents()));
#else
      // If we don't have to mount a device, we just issue
      // a dummy command to start the copy operation
      d->m_proc.setProgram("true");
      d->m_proc.start();
#endif
    }

  }

  delete backupDlg;
}

void KMyMoneyApp::slotBackupMount()
{
  progressCallback(0, 300, i18n("Mounting %1", d->m_mountpoint));
  d->m_proc.setProgram("mount");
  d->m_proc << d->m_mountpoint;
  d->m_proc.start();
}

bool KMyMoneyApp::slotBackupWriteFile()
{
  QFileInfo fi(d->m_fileName.fileName());
  QString today = QDate::currentDate().toString("-yyyy-MM-dd.") + fi.suffix();
  QString backupfile = d->m_mountpoint + '/' + d->m_fileName.fileName();
  KMyMoneyUtils::appendCorrectFileExt(backupfile, today);

  // check if file already exists and ask what to do
  QFile f(backupfile);
  if (f.exists()) {
    int answer = KMessageBox::warningContinueCancel(this, i18n("Backup file for today exists on that device. Replace?"), i18n("Backup"), KGuiItem(i18n("&Replace")));
    if (answer == KMessageBox::Cancel) {
      return false;
    }
  }

  progressCallback(50, 0, i18n("Writing %1", backupfile));
  d->m_proc.clearProgram();
#ifdef Q_OS_WIN
  d->m_proc << "cmd.exe" << "/c" << "copy" << "/b" << "/y";
  d->m_proc << (QDir::toNativeSeparators(d->m_fileName.toLocalFile()) + "+ nul") << QDir::toNativeSeparators(backupfile);
#else
  d->m_proc << "cp" << "-f";
  d->m_proc << d->m_fileName.toLocalFile() << backupfile;
#endif
  d->m_backupState = BACKUP_COPYING;
  d->m_proc.start();
  return true;
}

void KMyMoneyApp::slotBackupUnmount()
{
  progressCallback(250, 0, i18n("Unmounting %1", d->m_mountpoint));
  d->m_proc.clearProgram();
  d->m_proc.setProgram("umount");
  d->m_proc << d->m_mountpoint;
  d->m_backupState = BACKUP_UNMOUNTING;
  d->m_proc.start();
}

void KMyMoneyApp::slotBackupFinish()
{
  d->m_backupState = BACKUP_IDLE;
  progressCallback(-1, -1, QString());
  ready();
}

void KMyMoneyApp::slotBackupHandleEvents()
{
  switch (d->m_backupState) {
    case BACKUP_MOUNTING:

      if (d->m_ignoreBackupExitCode ||
         (d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0)) {
        d->m_ignoreBackupExitCode = false;
        d->m_backupResult = 0;
        if (!slotBackupWriteFile()) {
          d->m_backupResult = 1;
          if (d->m_backupMount)
            slotBackupUnmount();
          else
            slotBackupFinish();
        }
      } else {
        KMessageBox::information(this, i18n("Error mounting device"), i18n("Backup"));
        d->m_backupResult = 1;
        if (d->m_backupMount)
          slotBackupUnmount();
        else
          slotBackupFinish();
      }
      break;

    case BACKUP_COPYING:
      if (d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0) {

        if (d->m_backupMount) {
          slotBackupUnmount();
        } else {
          progressCallback(300, 0, i18nc("Backup done", "Done"));
          KMessageBox::information(this, i18n("File successfully backed up"), i18n("Backup"));
          slotBackupFinish();
        }
      } else {
        qDebug("copy exit code is %d", d->m_proc.exitCode());
        d->m_backupResult = 1;
        KMessageBox::information(this, i18n("Error copying file to device"), i18n("Backup"));
        if (d->m_backupMount)
          slotBackupUnmount();
        else
          slotBackupFinish();
      }
      break;


    case BACKUP_UNMOUNTING:
      if (d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0) {

        progressCallback(300, 0, i18nc("Backup done", "Done"));
        if (d->m_backupResult == 0)
          KMessageBox::information(this, i18n("File successfully backed up"), i18n("Backup"));
      } else {
        KMessageBox::information(this, i18n("Error unmounting device"), i18n("Backup"));
      }
      slotBackupFinish();
      break;

    default:
      qWarning("Unknown state for backup operation!");
      progressCallback(-1, -1, QString());
      ready();
      break;
  }
}

void KMyMoneyApp::slotShowTipOfTheDay()
{
  KTipDialog::showTip(d->m_myMoneyView, "", true);
}

void KMyMoneyApp::slotShowPreviousView()
{

}

void KMyMoneyApp::slotShowNextView()
{

}

void KMyMoneyApp::slotGenerateSql()
{
  QPointer<KGenerateSqlDlg> editor = new KGenerateSqlDlg(this);
  editor->setObjectName("Generate Database SQL");
  editor->exec();
  delete editor;
}

void KMyMoneyApp::slotToolsStartKCalc()
{
  QString cmd = KMyMoneyGlobalSettings::externalCalculator();
  // if none is present, we fall back to the default
  if (cmd.isEmpty()) {
#if defined(Q_OS_WIN32)
    cmd = QLatin1String("calc");
#elif defined(Q_OS_MAC)
    cmd = QLatin1String("open -a Calculator");
#else
    cmd = QLatin1String("kcalc");
#endif
  }
  KRun::runCommand(cmd, this);
}

void KMyMoneyApp::slotFindTransaction()
{
  if (d->m_searchDlg == 0) {
    d->m_searchDlg = new KFindTransactionDlg(this);
    connect(d->m_searchDlg, SIGNAL(destroyed()), this, SLOT(slotCloseSearchDialog()));
    connect(d->m_searchDlg, SIGNAL(transactionSelected(QString,QString)),
            d->m_myMoneyView, SLOT(slotLedgerSelected(QString,QString)));
  }
  d->m_searchDlg->show();
  d->m_searchDlg->raise();
  d->m_searchDlg->activateWindow();
}

void KMyMoneyApp::slotCloseSearchDialog()
{
  if (d->m_searchDlg)
    d->m_searchDlg->deleteLater();
  d->m_searchDlg = 0;
}

void KMyMoneyApp::createAccount(MyMoneyAccount& newAccount, MyMoneyAccount& parentAccount, MyMoneyAccount& brokerageAccount, MyMoneyMoney openingBal)
{
  MyMoneyFile *file = MyMoneyFile::instance();
  try {
    const MyMoneySecurity& sec = file->security(newAccount.currencyId());
    // Check the opening balance
    if (openingBal.isPositive() && newAccount.accountGroup() == eMyMoney::Account::Type::Liability) {
      QString message = i18n("This account is a liability and if the "
                             "opening balance represents money owed, then it should be negative.  "
                             "Negate the amount?\n\n"
                             "Please click Yes to change the opening balance to %1,\n"
                             "Please click No to leave the amount as %2,\n"
                             "Please click Cancel to abort the account creation."
                             , MyMoneyUtils::formatMoney(-openingBal, newAccount, sec)
                             , MyMoneyUtils::formatMoney(openingBal, newAccount, sec));

      int ans = KMessageBox::questionYesNoCancel(this, message);
      if (ans == KMessageBox::Yes) {
        openingBal = -openingBal;

      } else if (ans == KMessageBox::Cancel)
        return;
    }

    file->createAccount(newAccount, parentAccount, brokerageAccount, openingBal);

  } catch (const MyMoneyException &e) {
    KMessageBox::information(this, i18n("Unable to add account: %1", e.what()));
  }
}

void KMyMoneyApp::slotInvestmentNew(MyMoneyAccount& account, const MyMoneyAccount& parent)
{
  KNewInvestmentWizard::newInvestment(account, parent);
}

void KMyMoneyApp::slotCategoryNew(MyMoneyAccount& account, const MyMoneyAccount& parent)
{
  KNewAccountDlg::newCategory(account, parent);
}

void KMyMoneyApp::slotCategoryNew(MyMoneyAccount& account)
{
  KNewAccountDlg::newCategory(account, MyMoneyAccount());
}

void KMyMoneyApp::slotAccountNew(MyMoneyAccount& account)
{
  NewAccountWizard::Wizard::newAccount(account);
}

void KMyMoneyApp::createSchedule(MyMoneySchedule newSchedule, MyMoneyAccount& newAccount)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  // Add the schedule only if one exists
  //
  // Remember to modify the first split to reference the newly created account
  if (!newSchedule.name().isEmpty()) {
    try {
      // We assume at least 2 splits in the transaction
      MyMoneyTransaction t = newSchedule.transaction();
      if (t.splitCount() < 2) {
        throw MYMONEYEXCEPTION("Transaction for schedule has less than 2 splits!");
      }

      MyMoneyFileTransaction ft;
      try {
        file->addSchedule(newSchedule);

        // in case of a loan account, we keep a reference to this
        // schedule in the account
        if (newAccount.accountType() == eMyMoney::Account::Type::Loan
            || newAccount.accountType() == eMyMoney::Account::Type::AssetLoan) {
          newAccount.setValue("schedule", newSchedule.id());
          file->modifyAccount(newAccount);
        }
        ft.commit();
      } catch (const MyMoneyException &e) {
        KMessageBox::information(this, i18n("Unable to add scheduled transaction: %1", e.what()));
      }
    } catch (const MyMoneyException &e) {
      KMessageBox::information(this, i18n("Unable to add scheduled transaction: %1", e.what()));
    }
  }
}

QList<QPair<MyMoneyTransaction, MyMoneySplit> > KMyMoneyApp::Private::automaticReconciliation(const MyMoneyAccount &account,
    const QList<QPair<MyMoneyTransaction, MyMoneySplit> > &transactions,
    const MyMoneyMoney &amount)
{
  static const int NR_OF_STEPS_LIMIT = 300000;
  static const int PROGRESSBAR_STEPS = 1000;
  QList<QPair<MyMoneyTransaction, MyMoneySplit> > result = transactions;

  KMSTATUS(i18n("Running automatic reconciliation"));
  int progressBarIndex = 0;
  kmymoney->slotStatusProgressBar(progressBarIndex, NR_OF_STEPS_LIMIT / PROGRESSBAR_STEPS);

  // optimize the most common case - all transactions should be cleared
  QListIterator<QPair<MyMoneyTransaction, MyMoneySplit> > itTransactionSplitResult(result);
  MyMoneyMoney transactionsBalance;
  while (itTransactionSplitResult.hasNext()) {
    const QPair<MyMoneyTransaction, MyMoneySplit> &transactionSplit = itTransactionSplitResult.next();
    transactionsBalance += transactionSplit.second.shares();
  }
  if (amount == transactionsBalance) {
    result = transactions;
    return result;
  }
  kmymoney->slotStatusProgressBar(progressBarIndex++, 0);
  // only one transaction is uncleared
  itTransactionSplitResult.toFront();
  int index = 0;
  while (itTransactionSplitResult.hasNext()) {
    const QPair<MyMoneyTransaction, MyMoneySplit> &transactionSplit = itTransactionSplitResult.next();
    if (transactionsBalance - transactionSplit.second.shares() == amount) {
      result.removeAt(index);
      return result;
    }
    index++;
  }
  kmymoney->slotStatusProgressBar(progressBarIndex++, 0);

  // more than one transaction is uncleared - apply the algorithm
  result.clear();

  const MyMoneySecurity &security = MyMoneyFile::instance()->security(account.currencyId());
  double precision = 0.1 / account.fraction(security);

  QList<MyMoneyMoney> sumList;
  sumList << MyMoneyMoney();

  QMap<MyMoneyMoney, QList<QPair<QString, QString> > > sumToComponentsMap;

  // compute the possible matches
  QListIterator<QPair<MyMoneyTransaction, MyMoneySplit> > itTransactionSplit(transactions);
  while (itTransactionSplit.hasNext()) {
    const QPair<MyMoneyTransaction, MyMoneySplit> &transactionSplit = itTransactionSplit.next();
    QListIterator<MyMoneyMoney> itSum(sumList);
    QList<MyMoneyMoney> tempList;
    while (itSum.hasNext()) {
      const MyMoneyMoney &sum = itSum.next();
      QList<QPair<QString, QString> > splitIds;
      splitIds << qMakePair<QString, QString>(transactionSplit.first.id(), transactionSplit.second.id());
      if (sumToComponentsMap.contains(sum)) {
        if (sumToComponentsMap.value(sum).contains(qMakePair<QString, QString>(transactionSplit.first.id(), transactionSplit.second.id()))) {
          continue;
        }
        splitIds.append(sumToComponentsMap.value(sum));
      }
      tempList << transactionSplit.second.shares() + sum;
      sumToComponentsMap[transactionSplit.second.shares() + sum] = splitIds;
      int size = sumToComponentsMap.size();
      if (size % PROGRESSBAR_STEPS == 0) {
        kmymoney->slotStatusProgressBar(progressBarIndex++, 0);
      }
      if (size > NR_OF_STEPS_LIMIT) {
        return result; // it's taking too much resources abort the algorithm
      }
    }
    QList<MyMoneyMoney> unionList;
    unionList.append(tempList);
    unionList.append(sumList);
    qSort(unionList);
    sumList.clear();
    MyMoneyMoney smallestSumFromUnion = unionList.first();
    sumList.append(smallestSumFromUnion);
    QListIterator<MyMoneyMoney> itUnion(unionList);
    while (itUnion.hasNext()) {
      MyMoneyMoney sumFromUnion = itUnion.next();
      if (smallestSumFromUnion < MyMoneyMoney(1 - precision / transactions.size())*sumFromUnion) {
        smallestSumFromUnion = sumFromUnion;
        sumList.append(sumFromUnion);
      }
    }
  }

  kmymoney->slotStatusProgressBar(NR_OF_STEPS_LIMIT / PROGRESSBAR_STEPS, 0);
  if (sumToComponentsMap.contains(amount)) {
    QListIterator<QPair<MyMoneyTransaction, MyMoneySplit> > itTransactionSplit(transactions);
    while (itTransactionSplit.hasNext()) {
      const QPair<MyMoneyTransaction, MyMoneySplit> &transactionSplit = itTransactionSplit.next();
      const QList<QPair<QString, QString> > &splitIds = sumToComponentsMap.value(amount);
      if (splitIds.contains(qMakePair<QString, QString>(transactionSplit.first.id(), transactionSplit.second.id()))) {
        result.append(transactionSplit);
      }
    }
  }

#ifdef KMM_DEBUG
  qDebug("For the amount %s a number of %d possible sums where computed from the set of %d transactions: ",
         qPrintable(MyMoneyUtils::formatMoney(amount, security)), sumToComponentsMap.size(), transactions.size());
#endif

  kmymoney->slotStatusProgressBar(-1, -1);
  return result;
}

void KMyMoneyApp::slotReparentAccount(const MyMoneyAccount& _src, const MyMoneyInstitution& _dst)
{
  MyMoneyAccount src(_src);
  src.setInstitutionId(_dst.id());
  MyMoneyFileTransaction ft;
  try {
    MyMoneyFile::instance()->modifyAccount(src);
    ft.commit();
  } catch (const MyMoneyException &e) {
    KMessageBox::sorry(this, i18n("<p><b>%1</b> cannot be moved to institution <b>%2</b>. Reason: %3</p>", src.name(), _dst.name(), e.what()));
  }
}

void KMyMoneyApp::slotReparentAccount(const MyMoneyAccount& _src, const MyMoneyAccount& _dst)
{
  MyMoneyAccount src(_src);
  MyMoneyAccount dst(_dst);
  MyMoneyFileTransaction ft;
  try {
    MyMoneyFile::instance()->reparentAccount(src, dst);
    ft.commit();
  } catch (const MyMoneyException &e) {
    KMessageBox::sorry(this, i18n("<p><b>%1</b> cannot be moved to <b>%2</b>. Reason: %3</p>", src.name(), dst.name(), e.what()));
  }
}

void KMyMoneyApp::slotScheduleNew(const MyMoneyTransaction& _t, eMyMoney::Schedule::Occurrence occurrence)
{
  KEditScheduleDlg::newSchedule(_t, occurrence);
}

void KMyMoneyApp::slotPayeeNew(const QString& newnameBase, QString& id)
{
  KMyMoneyUtils::newPayee(newnameBase, id);
}

void KMyMoneyApp::slotNewFeature()
{
}

// move a stock transaction from one investment account to another
void KMyMoneyApp::Private::moveInvestmentTransaction(const QString& /*fromId*/,
    const QString& toId,
    const MyMoneyTransaction& tx)
{
  MyMoneyAccount toInvAcc = MyMoneyFile::instance()->account(toId);
  MyMoneyTransaction t(tx);
  // first determine which stock we are dealing with.
  // fortunately, investment transactions have only one stock involved
  QString stockAccountId;
  QString stockSecurityId;
  MyMoneySplit s;
  foreach (const auto split, t.splits()) {
    stockAccountId = split.accountId();
    stockSecurityId =
      MyMoneyFile::instance()->account(stockAccountId).currencyId();
    if (!MyMoneyFile::instance()->security(stockSecurityId).isCurrency()) {
      s = split;
      break;
    }
  }
  // Now check the target investment account to see if it
  // contains a stock with this id
  QString newStockAccountId;
  foreach (const auto sAccount, toInvAcc.accountList()) {
    if (MyMoneyFile::instance()->account(sAccount).currencyId() ==
        stockSecurityId) {
      newStockAccountId = sAccount;
      break;
    }
  }
  // if it doesn't exist, we need to add it as a copy of the old one
  // no 'copyAccount()' function??
  if (newStockAccountId.isEmpty()) {
    MyMoneyAccount stockAccount =
      MyMoneyFile::instance()->account(stockAccountId);
    MyMoneyAccount newStock;
    newStock.setName(stockAccount.name());
    newStock.setNumber(stockAccount.number());
    newStock.setDescription(stockAccount.description());
    newStock.setInstitutionId(stockAccount.institutionId());
    newStock.setOpeningDate(stockAccount.openingDate());
    newStock.setAccountType(stockAccount.accountType());
    newStock.setCurrencyId(stockAccount.currencyId());
    newStock.setClosed(stockAccount.isClosed());
    MyMoneyFile::instance()->addAccount(newStock, toInvAcc);
    newStockAccountId = newStock.id();
  }
  // now update the split and the transaction
  s.setAccountId(newStockAccountId);
  t.modifySplit(s);
  MyMoneyFile::instance()->modifyTransaction(t);
}

void KMyMoneyApp::showContextMenu(const QString& containerName)
{
  QWidget* w = factory()->container(containerName, this);
  QMenu *menu = dynamic_cast<QMenu*>(w);
  if (menu)
    menu->exec(QCursor::pos());
  else
    qDebug("menu '%s' not found: w = %p, menu = %p", qPrintable(containerName), w, menu);
}

void KMyMoneyApp::slotPrintView()
{
  d->m_myMoneyView->slotPrintView();
}

void KMyMoneyApp::updateCaption(bool skipActions)
{
  QString caption;

  caption = d->m_fileName.fileName();

  if (caption.isEmpty() && d->m_myMoneyView && d->m_myMoneyView->fileOpen())
    caption = i18n("Untitled");

  // MyMoneyFile::instance()->dirty() throws an exception, if
  // there's no storage object available. In this case, we
  // assume that the storage object is not changed. Actually,
  // this can only happen if we are newly created early on.
  bool modified;
  try {
    modified = MyMoneyFile::instance()->dirty();
  } catch (const MyMoneyException &) {
    modified = false;
    skipActions = true;
  }

#ifdef KMM_DEBUG
  caption += QString(" (%1 x %2)").arg(width()).arg(height());
#endif

  setCaption(caption, modified);

  if (!skipActions) {
    d->m_myMoneyView->enableViewsIfFileOpen();
    slotUpdateActions();
  }
}

void KMyMoneyApp::slotUpdateActions()
{
  const auto file = MyMoneyFile::instance();
  const bool fileOpen = d->m_myMoneyView->fileOpen();
  const bool modified = file->dirty();
//  const bool importRunning = (d->m_smtReader != 0);
  auto aC = actionCollection();

  // *************
  // Disabling actions to be disabled at this point
  // *************
  {
    static const QVector<Action> disabledActions {
      Action::UpdateAllAccounts
    };

    for (const auto& a : disabledActions)
      pActions[a]->setEnabled(false);
  }

  // *************
  // Disabling actions based on conditions
  // *************
  {
    QString tooltip = i18n("Create a new transaction");
    const QVector<QPair<Action, bool>> actionStates {
      {qMakePair(Action::FileOpenDatabase, true)},
      {qMakePair(Action::FileSaveAsDatabase, fileOpen)},
      {qMakePair(Action::FilePersonalData, fileOpen)},
      {qMakePair(Action::FileBackup, (fileOpen && !d->m_myMoneyView->isDatabase()))},
      {qMakePair(Action::FileInformation, fileOpen)},
      {qMakePair(Action::FileImportTemplate, fileOpen/* && !importRunning*/)},
      {qMakePair(Action::FileExportTemplate, fileOpen/* && !importRunning*/)},
#ifdef KMM_DEBUG
      {qMakePair(Action::FileDump, fileOpen)},
#endif
      {qMakePair(Action::EditFindTransaction, fileOpen)},
      {qMakePair(Action::ToolCurrencies, fileOpen)},
      {qMakePair(Action::ToolPrices, fileOpen)},
      {qMakePair(Action::ToolUpdatePrices, fileOpen)},
      {qMakePair(Action::ToolConsistency, fileOpen)},
      {qMakePair(Action::NewAccount, fileOpen)},
      {qMakePair(Action::AccountCreditTransfer, onlineJobAdministration::instance()->canSendCreditTransfer())},
      {qMakePair(Action::NewInstitution, fileOpen)},
//      {qMakePair(Action::TransactionNew, (fileOpen && d->m_myMoneyView->canCreateTransactions(KMyMoneyRegister::SelectedTransactions(), tooltip)))},
      {qMakePair(Action::NewSchedule, fileOpen)},
//      {qMakePair(Action::CurrencyNew, fileOpen)},
//      {qMakePair(Action::PriceNew, fileOpen)},
    };

    for (const auto& a : actionStates)
      pActions[a.first]->setEnabled(a.second);
  }

  // *************
  // Disabling standard actions based on conditions
  // *************
  aC->action(QString::fromLatin1(KStandardAction::name(KStandardAction::Save)))->setEnabled(modified /*&& !d->m_myMoneyView->isDatabase()*/);
  aC->action(QString::fromLatin1(KStandardAction::name(KStandardAction::SaveAs)))->setEnabled(fileOpen);
  aC->action(QString::fromLatin1(KStandardAction::name(KStandardAction::Close)))->setEnabled(fileOpen);
  aC->action(QString::fromLatin1(KStandardAction::name(KStandardAction::Print)))->setEnabled(fileOpen && d->m_myMoneyView->canPrint());

  // *************
  // Enabling actions based on conditions
  // *************
  QList<MyMoneyAccount> accList;
  file->accountList(accList);
  QList<MyMoneyAccount>::const_iterator it_a;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p = d->m_plugins.online.constEnd();
  for (it_a = accList.constBegin(); (it_p == d->m_plugins.online.constEnd()) && (it_a != accList.constEnd()); ++it_a) {
    if ((*it_a).hasOnlineMapping()) {
      // check if provider is available
      it_p = d->m_plugins.online.constFind((*it_a).onlineBankingSettings().value("provider").toLower());
      if (it_p != d->m_plugins.online.constEnd()) {
        QStringList protocols;
        (*it_p)->protocols(protocols);
        if (protocols.count() > 0) {
          pActions[Action::UpdateAllAccounts]->setEnabled(true);
        }
      }
    }
  }
}

void KMyMoneyApp::slotResetSelections()
{
  slotSelectAccount(MyMoneyAccount());
  d->m_myMoneyView->slotObjectSelected(MyMoneyAccount());
  d->m_myMoneyView->slotObjectSelected(MyMoneyInstitution());
  d->m_myMoneyView->slotObjectSelected(MyMoneySchedule());
  d->m_myMoneyView->slotObjectSelected(MyMoneyTag());
  d->m_myMoneyView->slotTransactionsSelected(KMyMoneyRegister::SelectedTransactions());
  slotUpdateActions();
}

void KMyMoneyApp::slotSelectAccount(const MyMoneyObject& obj)
{
  if (typeid(obj) != typeid(MyMoneyAccount))
    return;

  d->m_selectedAccount = MyMoneyAccount();
  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
  if (!acc.isInvest())
    d->m_selectedAccount = acc;

  // qDebug("slotSelectAccount('%s')", d->m_selectedAccount.name().data());
  slotUpdateActions();
  emit accountSelected(d->m_selectedAccount);
}

void KMyMoneyApp::slotDataChanged()
{
  // As this method is called every time the MyMoneyFile instance
  // notifies a modification, it's the perfect place to start the timer if needed
  if (d->m_autoSaveEnabled && !d->m_autoSaveTimer->isActive()) {
    d->m_autoSaveTimer->setSingleShot(true);
    d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);  //miliseconds
  }
  updateCaption();
}

void KMyMoneyApp::slotCurrencyDialog()
{
  QPointer<KCurrencyEditDlg> dlg = new KCurrencyEditDlg(this);
  dlg->exec();
  delete dlg;
}

void KMyMoneyApp::slotPriceDialog()
{
  QPointer<KMyMoneyPriceDlg> dlg = new KMyMoneyPriceDlg(this);
  dlg->exec();
  delete dlg;
}

void KMyMoneyApp::slotFileConsistencyCheck()
{
  d->consistencyCheck(true);
  updateCaption();
}

void KMyMoneyApp::Private::consistencyCheck(bool alwaysDisplayResult)
{
  KMSTATUS(i18n("Running consistency check..."));

  MyMoneyFileTransaction ft;
  try {
    m_consistencyCheckResult = MyMoneyFile::instance()->consistencyCheck();
    ft.commit();
  } catch (const MyMoneyException &e) {
    m_consistencyCheckResult.append(i18n("Consistency check failed: %1", e.what()));
    // always display the result if the check failed
    alwaysDisplayResult = true;
  }

  // in case the consistency check was OK, we get a single line as result
  // in all errneous cases, we get more than one line and force the
  // display of them.

  if (alwaysDisplayResult || m_consistencyCheckResult.size() > 1) {
    QString msg = i18n("The consistency check has found no issues in your data. Details are presented below.");
    if (m_consistencyCheckResult.size() > 1)
      msg = i18n("The consistency check has found some issues in your data. Details are presented below. Those issues that could not be corrected automatically need to be solved by the user.");
    // install a context menu for the list after the dialog is displayed
    QTimer::singleShot(500, q, SLOT(slotInstallConsistencyCheckContextMenu()));
    KMessageBox::informationList(0, msg, m_consistencyCheckResult, i18n("Consistency check result"));
  }
  // this data is no longer needed
  m_consistencyCheckResult.clear();
}

void KMyMoneyApp::Private::copyConsistencyCheckResults()
{
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_consistencyCheckResult.join(QLatin1String("\n")));
}

void KMyMoneyApp::Private::saveConsistencyCheckResults()
{
  QUrl fileUrl = QFileDialog::getSaveFileUrl(q);

  if (!fileUrl.isEmpty()) {
    QFile file(fileUrl.toLocalFile());
    if (file.open(QFile::WriteOnly | QFile::Append | QFile::Text)) {
      QTextStream out(&file);
      out << m_consistencyCheckResult.join(QLatin1String("\n"));
      file.close();
    }
  }
}

void KMyMoneyApp::Private::setThemedCSS()
{
  const QStringList CSSnames {QStringLiteral("kmymoney.css"), QStringLiteral("welcome.css")};

  const QString rcDir("/html/");
  const QStringList defaultCSSDirs = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, rcDir, QStandardPaths::LocateDirectory);

  // scan the list of directories to find the ones that really
  // contains all files we look for
  QString defaultCSSDir;
  foreach (const auto dir, defaultCSSDirs) {
    defaultCSSDir = dir;
    foreach (const auto CSSname, CSSnames) {
      QFileInfo fileInfo(defaultCSSDir + CSSname);
      if (!fileInfo.exists()) {
        defaultCSSDir.clear();
        break;
      }
    }
    if (!defaultCSSDir.isEmpty()) {
      break;
    }
  }

  // make sure we have the local directory where the themed version is stored
  const QString themedCSSDir  = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).first() + rcDir;
  QDir().mkpath(themedCSSDir);

  foreach (const auto CSSname, CSSnames) {
    const QString defaultCSSFilename = defaultCSSDir + CSSname;
    QFileInfo fileInfo(defaultCSSFilename);
    if (fileInfo.exists()) {
      const QString themedCSSFilename = themedCSSDir + CSSname;
      QFile::remove(themedCSSFilename);
      if (QFile::copy(defaultCSSFilename, themedCSSFilename)) {
        QFile cssFile (themedCSSFilename);
        if (cssFile.open(QIODevice::ReadWrite)) {
          QTextStream cssStream(&cssFile);
          auto cssText = cssStream.readAll();
          cssText.replace(QLatin1String("./"), defaultCSSDir, Qt::CaseSensitive);
          cssText.replace(QLatin1String("WindowText"),    KMyMoneyGlobalSettings::schemeColor(SchemeColor::WindowText).name(),        Qt::CaseSensitive);
          cssText.replace(QLatin1String("Window"),        KMyMoneyGlobalSettings::schemeColor(SchemeColor::WindowBackground).name(),  Qt::CaseSensitive);
          cssText.replace(QLatin1String("HighlightText"), KMyMoneyGlobalSettings::schemeColor(SchemeColor::ListHighlightText).name(), Qt::CaseSensitive);
          cssText.replace(QLatin1String("Highlight"),     KMyMoneyGlobalSettings::schemeColor(SchemeColor::ListHighlight).name(),     Qt::CaseSensitive);
          cssText.replace(QLatin1String("black"),         KMyMoneyGlobalSettings::schemeColor(SchemeColor::ListGrid).name(),          Qt::CaseSensitive);
          cssStream.seek(0);
          cssStream << cssText;
          cssFile.close();
        }
      }
    }
  }
}

void KMyMoneyApp::slotCheckSchedules()
{
  if (KMyMoneyGlobalSettings::checkSchedule() == true) {

    KMSTATUS(i18n("Checking for overdue scheduled transactions..."));
    MyMoneyFile *file = MyMoneyFile::instance();
    QDate checkDate = QDate::currentDate().addDays(KMyMoneyGlobalSettings::checkSchedulePreview());

    QList<MyMoneySchedule> scheduleList =  file->scheduleList();
    QList<MyMoneySchedule>::Iterator it;

    eDialogs::ScheduleResultCode rc = eDialogs::ScheduleResultCode::Enter;
    for (it = scheduleList.begin(); (it != scheduleList.end()) && (rc != eDialogs::ScheduleResultCode::Cancel); ++it) {
      // Get the copy in the file because it might be modified by commitTransaction
      MyMoneySchedule schedule = file->schedule((*it).id());

      if (schedule.autoEnter()) {
        try {
          while (!schedule.isFinished() && (schedule.adjustedNextDueDate() <= checkDate)
                 && rc != eDialogs::ScheduleResultCode::Ignore
                 && rc != eDialogs::ScheduleResultCode::Cancel) {
            rc = d->m_myMoneyView->enterSchedule(schedule, true, true);
            schedule = file->schedule((*it).id()); // get a copy of the modified schedule
          }
        } catch (const MyMoneyException &) {
        }
      }
      if (rc == eDialogs::ScheduleResultCode::Ignore) {
        // if the current schedule was ignored then we must make sure that the user can still enter the next scheduled transaction
        rc = eDialogs::ScheduleResultCode::Enter;
      }
    }
    updateCaption();
  }
}

void KMyMoneyApp::writeLastUsedDir(const QString& directory)
{
  //get global config object for our app.
  KSharedConfigPtr kconfig = KSharedConfig::openConfig();
  if (kconfig) {
    KConfigGroup grp = kconfig->group("General Options");

    //write path entry, no error handling since its void.
    grp.writeEntry("LastUsedDirectory", directory);
  }
}

void KMyMoneyApp::writeLastUsedFile(const QString& fileName)
{
  //get global config object for our app.
  KSharedConfigPtr kconfig = KSharedConfig::openConfig();
  if (kconfig) {
    KConfigGroup grp = d->m_config->group("General Options");

    // write path entry, no error handling since its void.
    // use a standard string, as fileName could contain a protocol
    // e.g. file:/home/thb/....
    grp.writeEntry("LastUsedFile", fileName);
  }
}

QString KMyMoneyApp::readLastUsedDir() const
{
  QString str;

  //get global config object for our app.
  KSharedConfigPtr kconfig = KSharedConfig::openConfig();
  if (kconfig) {
    KConfigGroup grp = d->m_config->group("General Options");

    //read path entry.  Second parameter is the default if the setting is not found, which will be the default document path.
    str = grp.readEntry("LastUsedDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    // if the path stored is empty, we use the default nevertheless
    if (str.isEmpty())
      str = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  }

  return str;
}

QString KMyMoneyApp::readLastUsedFile() const
{
  QString str;

  // get global config object for our app.
  KSharedConfigPtr kconfig = KSharedConfig::openConfig();
  if (kconfig) {
    KConfigGroup grp = d->m_config->group("General Options");

    // read filename entry.
    str = grp.readEntry("LastUsedFile", "");
  }

  return str;
}

QString KMyMoneyApp::filename() const
{
  return d->m_fileName.url();
}

WebConnect* KMyMoneyApp::webConnect() const
{
  return d->m_webConnect;
}

QList<QString> KMyMoneyApp::instanceList() const
{
  QList<QString> list;
#ifdef KMM_DBUS
  QDBusReply<QStringList> reply = QDBusConnection::sessionBus().interface()->registeredServiceNames();

  if (reply.isValid()) {
    QStringList apps = reply.value();
    QStringList::ConstIterator it;

    // build a list of service names of all running kmymoney applications without this one
    for (it = apps.constBegin(); it != apps.constEnd(); ++it) {
      // please change this method of creating a list of 'all the other kmymoney instances that are running on the system'
      // since assuming that D-Bus creates service names with org.kde.kmymoney-PID is an observation I don't think that it's documented somwhere
      if ((*it).indexOf("org.kde.kmymoney-") == 0) {
        uint thisProcPid = platformTools::processId();
        if ((*it).indexOf(QString("org.kde.kmymoney-%1").arg(thisProcPid)) != 0)
          list += (*it);
      }
    }
  } else {
    qDebug("D-Bus returned the following error while obtaining instances: %s", qPrintable(reply.error().message()));
  }
#endif
  return list;
}

void KMyMoneyApp::slotEquityPriceUpdate()
{
  QPointer<KEquityPriceUpdateDlg> dlg = new KEquityPriceUpdateDlg(this);
  if (dlg->exec() == QDialog::Accepted && dlg != 0)
    dlg->storePrices();
  delete dlg;
}

void KMyMoneyApp::webConnect(const QString& sourceUrl, const QByteArray& asn_id)
{
  //
  // Web connect attempts to go through the known importers and see if the file
  // can be importing using that method.  If so, it will import it using that
  // plugin
  //

  Q_UNUSED(asn_id)

  d->m_importUrlsQueue.enqueue(sourceUrl);

  // only start processing if this is the only import so far
  if (d->m_importUrlsQueue.count() == 1) {
    while (!d->m_importUrlsQueue.isEmpty()) {
      // get the value of the next item from the queue
      // but leave it on the queue for now
      QString url = d->m_importUrlsQueue.head();

      // Bring this window to the forefront.  This method was suggested by
      // Lubos Lunak <l.lunak@suse.cz> of the KDE core development team.
      // TODO: port KF5 (WebConnect)
      //KStartupInfo::setNewStartupId(this, asn_id);

      // Make sure we have an open file
      if (! d->m_myMoneyView->fileOpen() &&
          KMessageBox::warningContinueCancel(kmymoney, i18n("You must first select a KMyMoney file before you can import a statement.")) == KMessageBox::Continue)
        kmymoney->slotFileOpen();

      // only continue if the user really did open a file.
      if (d->m_myMoneyView->fileOpen()) {
        KMSTATUS(i18n("Importing a statement via Web Connect"));

        // remove the statement files
        d->unlinkStatementXML();

        QMap<QString, KMyMoneyPlugin::ImporterPlugin*>::const_iterator it_plugin = d->m_plugins.importer.constBegin();
        while (it_plugin != d->m_plugins.importer.constEnd()) {
          if ((*it_plugin)->isMyFormat(url)) {
            QList<MyMoneyStatement> statements;
            if (!(*it_plugin)->import(url)) {
              KMessageBox::error(this, i18n("Unable to import %1 using %2 plugin.  The plugin returned the following error: %3", url, (*it_plugin)->formatName(), (*it_plugin)->lastError()), i18n("Importing error"));
            }

            break;
          }
          ++it_plugin;
        }

        // If we did not find a match, try importing it as a KMM statement file,
        // which is really just for testing.  the statement file is not exposed
        // to users.
        if (it_plugin == d->m_plugins.importer.constEnd())
          if (MyMoneyStatement::isStatementFile(url))
            MyMoneyStatementReader::importStatement(url, false, &progressCallback);

      }
      // remove the current processed item from the queue
      d->m_importUrlsQueue.dequeue();
    }
  }
}

void KMyMoneyApp::slotEnableMessages()
{
  KMessageBox::enableAllMessages();
  KMessageBox::information(this, i18n("All messages have been enabled."), i18n("All messages"));
}

void KMyMoneyApp::createInterfaces()
{
  // Sets up the plugin interface
  KMyMoneyPlugin::pluginInterfaces().importInterface = new KMyMoneyPlugin::KMMImportInterface(this);
  KMyMoneyPlugin::pluginInterfaces().statementInterface = new KMyMoneyPlugin::KMMStatementInterface(this);
  KMyMoneyPlugin::pluginInterfaces().viewInterface = new KMyMoneyPlugin::KMMViewInterface(d->m_myMoneyView, this);

  // setup the calendar interface for schedules
  MyMoneySchedule::setProcessingCalendar(this);
}

void KMyMoneyApp::slotAutoSave()
{
  if (!d->m_inAutoSaving) {
    // store the focus widget so we can restore it after save
    QPointer<QWidget> focusWidget = qApp->focusWidget();
    d->m_inAutoSaving = true;
    KMSTATUS(i18n("Auto saving..."));

    //calls slotFileSave if needed, and restart the timer
    //it the file is not saved, reinitializes the countdown.
    if (d->m_myMoneyView->dirty() && d->m_autoSaveEnabled) {
      if (!slotFileSave() && d->m_autoSavePeriod > 0) {
        d->m_autoSaveTimer->setSingleShot(true);
        d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);
      }
    }

    d->m_inAutoSaving = false;
    if (focusWidget && focusWidget != qApp->focusWidget()) {
      // we have a valid focus widget so restore it
      focusWidget->setFocus();
    }
  }
}

void KMyMoneyApp::slotDateChanged()
{
  QDateTime dt = QDateTime::currentDateTime();
  QDateTime nextDay(QDate(dt.date().addDays(1)), QTime(0, 0, 0));

  // +1 is to make sure that we're already in the next day when the
  // signal is sent (this way we also avoid setting the timer to 0)
  QTimer::singleShot((dt.secsTo(nextDay) + 1)*1000, this, SLOT(slotDateChanged()));
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoneyApp::slotOnlineAccountRequested(const MyMoneyAccount& acc, eMenu::Action action)
{
  d->m_selectedAccount = acc;
  switch (action) {
    case eMenu::Action::UnmapOnlineAccount:
      slotAccountUnmapOnline();
      break;
    case eMenu::Action::MapOnlineAccount:
      slotAccountMapOnline();
      break;
    case eMenu::Action::UpdateAccount:
      slotAccountUpdateOnline();
      break;
    case eMenu::Action::UpdateAllAccounts:
      slotAccountUpdateOnlineAll();
      break;
    default:
      break;
  }
}

void KMyMoneyApp::slotAccountUnmapOnline()
{
  // no account selected
  if (d->m_selectedAccount.id().isEmpty())
    return;

  // not a mapped account
  if (!d->m_selectedAccount.hasOnlineMapping())
    return;

  if (KMessageBox::warningYesNo(this, QString("<qt>%1</qt>").arg(i18n("Do you really want to remove the mapping of account <b>%1</b> to an online account? Depending on the details of the online banking method used, this action cannot be reverted.", d->m_selectedAccount.name())), i18n("Remove mapping to online account")) == KMessageBox::Yes) {
    MyMoneyFileTransaction ft;
    try {
      d->m_selectedAccount.setOnlineBankingSettings(MyMoneyKeyValueContainer());
      // delete the kvp that is used in MyMoneyStatementReader too
      // we should really get rid of it, but since I don't know what it
      // is good for, I'll keep it around. (ipwizard)
      d->m_selectedAccount.deletePair("StatementKey");
      MyMoneyFile::instance()->modifyAccount(d->m_selectedAccount);
      ft.commit();
      // The mapping could disable the online task system
      onlineJobAdministration::instance()->updateOnlineTaskProperties();
    } catch (const MyMoneyException &e) {
      KMessageBox::error(this, i18n("Unable to unmap account from online account: %1", e.what()));
    }
  }
}

void KMyMoneyApp::slotAccountMapOnline()
{
  // no account selected
  if (d->m_selectedAccount.id().isEmpty())
    return;

  // already an account mapped
  if (d->m_selectedAccount.hasOnlineMapping())
    return;

  // check if user tries to map a brokerageAccount
  if (d->m_selectedAccount.name().contains(i18n(" (Brokerage)"))) {
    if (KMessageBox::warningContinueCancel(this, i18n("You try to map a brokerage account to an online account. This is usually not advisable. In general, the investment account should be mapped to the online account. Please cancel if you intended to map the investment account, continue otherwise"), i18n("Mapping brokerage account")) == KMessageBox::Cancel) {
      return;
    }
  }

  // if we have more than one provider let the user select the current provider
  QString provider;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  switch (d->m_plugins.online.count()) {
    case 0:
      break;
    case 1:
      provider = d->m_plugins.online.begin().key();
      break;
    default: {
        QMenu popup(this);
        popup.setTitle(i18n("Select online banking plugin"));

        // Populate the pick list with all the provider
        for (it_p = d->m_plugins.online.constBegin(); it_p != d->m_plugins.online.constEnd(); ++it_p) {
          popup.addAction(it_p.key())->setData(it_p.key());
        }

        QAction *item = popup.actions()[0];
        if (item) {
          popup.setActiveAction(item);
        }

        // cancelled
        if ((item = popup.exec(QCursor::pos(), item)) == 0) {
          return;
        }

        provider = item->data().toString();
      }
      break;
  }

  if (provider.isEmpty())
    return;

  // find the provider
  it_p = d->m_plugins.online.constFind(provider.toLower());
  if (it_p != d->m_plugins.online.constEnd()) {
    // plugin found, call it
    MyMoneyKeyValueContainer settings;
    if ((*it_p)->mapAccount(d->m_selectedAccount, settings)) {
      settings["provider"] = provider.toLower();
      MyMoneyAccount acc(d->m_selectedAccount);
      acc.setOnlineBankingSettings(settings);
      MyMoneyFileTransaction ft;
      try {
        MyMoneyFile::instance()->modifyAccount(acc);
        ft.commit();
        // The mapping could enable the online task system
        onlineJobAdministration::instance()->updateOnlineTaskProperties();
      } catch (const MyMoneyException &e) {
        KMessageBox::error(this, i18n("Unable to map account to online account: %1", e.what()));
      }
    }
  }
}

void KMyMoneyApp::slotAccountUpdateOnlineAll()
{
  QList<MyMoneyAccount> accList;
  MyMoneyFile::instance()->accountList(accList);
  QList<MyMoneyAccount>::iterator it_a;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  d->m_statementResults.clear();
  d->m_collectingStatements = true;

  // remove all those from the list, that don't have a 'provider' or the
  // provider is not currently present
  for (it_a = accList.begin(); it_a != accList.end();) {
    if (!(*it_a).hasOnlineMapping()
        || d->m_plugins.online.find((*it_a).onlineBankingSettings().value("provider").toLower()) == d->m_plugins.online.end()) {
      it_a = accList.erase(it_a);
    } else
      ++it_a;
  }
  const QVector<Action> disabledActions {Action::UpdateAccount, Action::UpdateAllAccounts};
  for (const auto& a : disabledActions)
    pActions[a]->setEnabled(false);

  // now work on the remaining list of accounts
  int cnt = accList.count() - 1;
  for (it_a = accList.begin(); it_a != accList.end(); ++it_a) {
    it_p = d->m_plugins.online.constFind((*it_a).onlineBankingSettings().value("provider").toLower());
    (*it_p)->updateAccount(*it_a, cnt != 0);
    --cnt;
  }

  d->m_collectingStatements = false;
  if (!d->m_statementResults.isEmpty())
    KMessageBox::informationList(this, i18n("The statements have been processed with the following results:"), d->m_statementResults, i18n("Statement stats"));

  // re-enable the disabled actions
  slotUpdateActions();
}

void KMyMoneyApp::slotAccountUpdateOnline()
{
  // no account selected
  if (d->m_selectedAccount.id().isEmpty())
    return;

  // no online account mapped
  if (!d->m_selectedAccount.hasOnlineMapping())
    return;

  const QVector<Action> disabledActions {Action::UpdateAccount, Action::UpdateAllAccounts};
  for (const auto& a : disabledActions)
    pActions[a]->setEnabled(false);

  // find the provider
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  it_p = d->m_plugins.online.constFind(d->m_selectedAccount.onlineBankingSettings().value("provider").toLower());
  if (it_p != d->m_plugins.online.constEnd()) {
    // plugin found, call it
    d->m_collectingStatements = true;
    d->m_statementResults.clear();
    (*it_p)->updateAccount(d->m_selectedAccount);
    d->m_collectingStatements = false;
    if (!d->m_statementResults.isEmpty())
      KMessageBox::informationList(this, i18n("The statements have been processed with the following results:"), d->m_statementResults, i18n("Statement stats"));
  }

  // re-enable the disabled actions
  slotUpdateActions();
}

void KMyMoneyApp::setHolidayRegion(const QString& holidayRegion)
{
#ifdef KF5Holidays_FOUND
  //since the cost of updating the cache is now not negligible
  //check whether the region has been modified
  if (!d->m_holidayRegion || d->m_holidayRegion->regionCode() != holidayRegion) {
    // Delete the previous holidayRegion before creating a new one.
    delete d->m_holidayRegion;
    // Create a new holidayRegion.
    d->m_holidayRegion = new KHolidays::HolidayRegion(holidayRegion);

    //clear and update the holiday cache
    preloadHolidays();
  }
#else
  Q_UNUSED(holidayRegion);
#endif
}

bool KMyMoneyApp::isProcessingDate(const QDate& date) const
{
#ifdef KF5Holidays_FOUND
  if (!d->m_processingDays.testBit(date.dayOfWeek()))
    return false;
  if (!d->m_holidayRegion || !d->m_holidayRegion->isValid())
    return true;

  //check first whether it's already in cache
  if (d->m_holidayMap.contains(date)) {
    return d->m_holidayMap.value(date, true);
  } else {
    bool processingDay = !d->m_holidayRegion->isHoliday(date);
    d->m_holidayMap.insert(date, processingDay);
    return processingDay;
  }
#else
  Q_UNUSED(date);
  return true;
#endif
}

void KMyMoneyApp::preloadHolidays()
{
#ifdef KF5Holidays_FOUND
  //clear the cache before loading
  d->m_holidayMap.clear();
  //only do this if it is a valid region
  if (d->m_holidayRegion && d->m_holidayRegion->isValid()) {
    //load holidays for the forecast days plus 1 cycle, to be on the safe side
    int forecastDays = KMyMoneyGlobalSettings::forecastDays() + KMyMoneyGlobalSettings::forecastAccountCycle();
    QDate endDate = QDate::currentDate().addDays(forecastDays);

    //look for holidays for the next 2 years as a minimum. That should give a good margin for the cache
    if (endDate < QDate::currentDate().addYears(2))
      endDate = QDate::currentDate().addYears(2);

    KHolidays::Holiday::List holidayList = d->m_holidayRegion->holidays(QDate::currentDate(), endDate);
    KHolidays::Holiday::List::const_iterator holiday_it;
    for (holiday_it = holidayList.constBegin(); holiday_it != holidayList.constEnd(); ++holiday_it) {
      for (QDate holidayDate = (*holiday_it).observedStartDate(); holidayDate <= (*holiday_it).observedEndDate(); holidayDate = holidayDate.addDays(1))
        d->m_holidayMap.insert(holidayDate, false);
    }

    for (QDate date = QDate::currentDate(); date <= endDate; date = date.addDays(1)) {
      //if it is not a processing day, set it to false
      if (!d->m_processingDays.testBit(date.dayOfWeek())) {
        d->m_holidayMap.insert(date, false);
      } else if (!d->m_holidayMap.contains(date)) {
        //if it is not a holiday nor a weekend, it is a processing day
        d->m_holidayMap.insert(date, true);
      }
    }
  }
#endif
}

KMStatus::KMStatus(const QString &text)
{
  m_prevText = kmymoney->slotStatusMsg(text);
}

KMStatus::~KMStatus()
{
  kmymoney->slotStatusMsg(m_prevText);
}

void KMyMoneyApp::Private::unlinkStatementXML()
{
  QDir d(KMyMoneySettings::logPath(), "kmm-statement*");
  for (uint i = 0; i < d.count(); ++i) {
    qDebug("Remove %s", qPrintable(d[i]));
    d.remove(KMyMoneySettings::logPath() + QString("/%1").arg(d[i]));
  }
  m_statementXMLindex = 0;
}

void KMyMoneyApp::Private::closeFile()
{
  q->slotSelectAccount(MyMoneyAccount());
  q->d->m_myMoneyView->slotObjectSelected(MyMoneyAccount());
  q->d->m_myMoneyView->slotObjectSelected(MyMoneyInstitution());
  q->d->m_myMoneyView->slotObjectSelected(MyMoneySchedule());
  q->d->m_myMoneyView->slotObjectSelected(MyMoneyTag());
  q->d->m_myMoneyView->slotTransactionsSelected(KMyMoneyRegister::SelectedTransactions());
//  q->slotSelectTransactions(KMyMoneyRegister::SelectedTransactions());

  m_reconciliationAccount = MyMoneyAccount();
  m_myMoneyView->finishReconciliation(MyMoneyAccount());

  m_myMoneyView->closeFile();
  m_fileName = QUrl();
  q->updateCaption();

  // just create a new balance warning object
  delete m_balanceWarning;
  m_balanceWarning = new KBalanceWarning(q);

  emit q->fileLoaded(m_fileName);
}
