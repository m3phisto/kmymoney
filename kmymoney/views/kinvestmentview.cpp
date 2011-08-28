/***************************************************************************
                          kinvestmentview.cpp  -  description
                             -------------------
    begin                : Mon Mar 12 2007
    copyright            : (C) 2007 by Thomas Baumgart
    email                : Thomas Baumgart <ipwizard@users.sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kinvestmentview.h"

#include <typeinfo>

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes


#include <klocale.h>
#include <KToggleAction>
#include <KMessageBox>

// ----------------------------------------------------------------------------
// Project Includes

#include <mymoneyfile.h>
#include <mymoneyutils.h>
#include <mymoneysecurity.h>
#include <mymoneytransaction.h>
#include <mymoneyinvesttransaction.h>
#include <mymoneyaccount.h>
#include <kmymoneyglobalsettings.h>
#include <kmymoneyaccountcombo.h>
#include <kmymoneycurrencyselector.h>
#include <knewinvestmentwizard.h>
#include "kmymoney.h"
#include "models.h"

class KInvestmentView::Private
{
public:
  Private() :
      m_newAccountLoaded(false),
      m_recursion(false),
      m_precision(2),
      m_filterProxyModel(0) {}

  MyMoneyAccount    m_account;
  bool              m_needReload[MaxViewTabs];
  bool              m_newAccountLoaded;
  bool              m_recursion;
  int               m_precision;
  AccountNamesFilterProxyModel *m_filterProxyModel;

};

KInvestmentView::KInvestmentView(QWidget *parent) :
    QWidget(parent),
    d(new Private),
    m_currencyMarket("ISO 4217")
{
  setupUi(this);

  //first set up everything for the equities tab
  d->m_filterProxyModel = new AccountNamesFilterProxyModel(this);
  d->m_filterProxyModel->addAccountType(MyMoneyAccount::Investment);
  d->m_filterProxyModel->setHideEquityAccounts(false);
  d->m_filterProxyModel->setSourceModel(Models::instance()->accountsModel());
  d->m_filterProxyModel->sort(0);
  m_accountComboBox->setModel(d->m_filterProxyModel);

  m_investmentsList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_investmentsList->setSortingEnabled(true);
  //KConfigGroup grp = KGlobal::config()->group("Investment Settings");
  //m_table->restoreLayout(grp);

  for (int i = 0; i < MaxViewTabs; ++i)
    d->m_needReload[i] = false;

  connect(m_tab, SIGNAL(currentChanged(QWidget*)), this, SLOT(slotTabCurrentChanged(QWidget*)));

  connect(m_investmentsList, SIGNAL(customContextMenuRequested(const QPoint&)),
          this, SLOT(slotInvestmentContextMenu(const QPoint&)));
  connect(m_investmentsList, SIGNAL(itemSelectionChanged()), this, SLOT(slotInvestmentSelectionChanged()));
  connect(m_accountComboBox, SIGNAL(accountSelected(const QString&)),
          this, SLOT(slotSelectAccount(const QString&)));
  connect(m_investmentsList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), kmymoney->action("investment_edit"), SLOT(trigger()));
  connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotLoadView()));

  // create the searchline widget
  // and insert it into the existing layout
  m_searchSecuritiesWidget = new KTreeWidgetSearchLineWidget(this, m_securitiesList);
  m_searchSecuritiesWidget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
  m_securitiesLayout->insertWidget(0, m_searchSecuritiesWidget);

  KGuiItem removeButtonItem(i18n("&Delete"),
                            KIcon("edit-delete"),
                            i18n("Delete this entry"),
                            i18n("Remove this security item from the file"));
  m_deleteSecurityButton->setGuiItem(removeButtonItem);

  KGuiItem editButtonItem(i18n("&Edit"),
                          KIcon("document-edit"),
                          i18n("Modify the selected entry"),
                          i18n("Change the security information of the selected entry."));
  m_editSecurityButton->setGuiItem(editButtonItem);

  connect(m_showCurrencyButton, SIGNAL(toggled(bool)), this, SLOT(slotLoadView()));
  connect(m_securitiesList, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateSecuritiesButtons()));
  connect(m_editSecurityButton, SIGNAL(clicked()), this, SLOT(slotEditSecurity()));
  connect(m_deleteSecurityButton, SIGNAL(clicked()), this, SLOT(slotDeleteSecurity()));
}

KInvestmentView::~KInvestmentView()
{
  KConfigGroup grp = KGlobal::config()->group("Investment Settings");
  //m_table->saveLayout(grp);
  delete d;
}

void KInvestmentView::loadView(InvestmentsViewTab tab)
{
  if (d->m_needReload[tab]) {
    switch (tab) {
      case EquitiesTab:
        loadInvestmentTab();
        // force a new account if the current one is empty
        d->m_newAccountLoaded = d->m_account.id().isEmpty();
        break;
      case SecuritiesTab:
        loadSecuritiesList();
        break;
      default:
        break;
    }
    d->m_needReload[tab] = false;
  }
}

void KInvestmentView::slotInvestmentSelectionChanged()
{
  kmymoney->slotSelectInvestment();

  QTreeWidgetItem *item = m_investmentsList->currentItem();
  if (item) {
    try {
      MyMoneyAccount account = MyMoneyFile::instance()->account(item->data(0, Qt::UserRole).value<MyMoneyAccount>().id());
      kmymoney->slotSelectInvestment(account);

    } catch (MyMoneyException *e) {
      delete e;
    }
  }
}

void KInvestmentView::slotInvestmentContextMenu(const QPoint& /*point*/)
{
  kmymoney->slotSelectInvestment();
  QTreeWidgetItem *item = m_investmentsList->currentItem();
  if (item) {
    kmymoney->slotSelectInvestment(MyMoneyFile::instance()->account(item->data(0, Qt::UserRole).value<MyMoneyAccount>().id()));
  }
  emit investmentRightMouseClick();
}

void KInvestmentView::slotLoadView(void)
{
  d->m_needReload[EquitiesTab] = true;
  d->m_needReload[SecuritiesTab] = true;
  if (isVisible())
    slotTabCurrentChanged(m_tab->currentWidget());
}

void KInvestmentView::slotTabCurrentChanged(QWidget* _tab)
{
  InvestmentsViewTab tab = static_cast<InvestmentsViewTab>(m_tab->indexOf(_tab));

  loadView(tab);
}

void KInvestmentView::loadAccounts(void)
{
  MyMoneyFile* file = MyMoneyFile::instance();

  // check if the current account still exists and make it the
  // current account
  if (!d->m_account.id().isEmpty()) {
    try {
      d->m_account = file->account(d->m_account.id());
    } catch (MyMoneyException *e) {
      delete e;
      d->m_account = MyMoneyAccount();
    }
  }

  d->m_filterProxyModel->invalidate();
  m_accountComboBox->expandAll();

  if (d->m_account.id().isEmpty()) {
    // there are no favorite accounts find any account
    QModelIndexList list = d->m_filterProxyModel->match(d->m_filterProxyModel->index(0, 0),
                           Qt::DisplayRole,
                           QVariant(QString("*")),
                           -1,
                           Qt::MatchFlags(Qt::MatchWildcard | Qt::MatchRecursive));
    for (QModelIndexList::ConstIterator it = list.constBegin(); it != list.constEnd(); ++it) {
      if (!it->parent().isValid())
        continue; // skip the top level accounts
      QVariant accountId = d->m_filterProxyModel->data(*it, AccountsModel::AccountIdRole);
      if (accountId.isValid()) {
        MyMoneyAccount a = file->account(accountId.toString());
        if (a.value("PreferredAccount") == "Yes") {
          d->m_account = a;
          break;
        } else if (d->m_account.id().isEmpty()) {
          d->m_account = a;
        }
      }
    }
  }

  if (!d->m_account.id().isEmpty()) {
    m_accountComboBox->setSelected(d->m_account.id());
    try {
      d->m_precision = MyMoneyMoney::denomToPrec(d->m_account.fraction());
    } catch (MyMoneyException *e) {
      qDebug("Security %s for account %s not found", qPrintable(d->m_account.currencyId()), qPrintable(d->m_account.name()));
      delete e;
      d->m_precision = 2;
    }
  }
}


bool KInvestmentView::slotSelectAccount(const MyMoneyObject& obj)
{
  if (typeid(obj) != typeid(MyMoneyAccount))
    return false;

  if (d->m_recursion)
    return false;

  d->m_recursion = true;
  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
  bool rc = slotSelectAccount(acc.id());
  d->m_recursion = false;
  return rc;
}

bool KInvestmentView::slotSelectAccount(const QString& id, const QString& transactionId, const bool /* reconciliation*/)
{
  bool    rc = true;

  if (!id.isEmpty()) {
    // if the account id differs, then we have to do something
    if (d->m_account.id() != id) {
      try {
        d->m_account = MyMoneyFile::instance()->account(id);
        // if a stock account is selected, we show the
        // the corresponding parent (investment) account
        if (d->m_account.isInvest()) {
          d->m_account = MyMoneyFile::instance()->account(d->m_account.parentAccountId());
        }
        // TODO if we don't have an investment account, then we should switch to the ledger view
        d->m_newAccountLoaded = true;
        if (d->m_account.accountType() == MyMoneyAccount::Investment) {
          slotLoadView();
        } else {
          emit accountSelected(id, transactionId);
          d->m_account = MyMoneyAccount();
          d->m_needReload[EquitiesTab] = true;
          rc = false;
        }

      } catch (MyMoneyException* e) {
        qDebug("Unable to retrieve account %s", qPrintable(id));
        delete e;
        rc = false;
      }
    } else {
      emit accountSelected(d->m_account);
    }
  }

  return rc;
}

void KInvestmentView::clear(void)
{
  // setup header font
  QFont font = KMyMoneyGlobalSettings::listHeaderFont();
  QFontMetrics fm(font);
  int height = fm.lineSpacing() + 6;
  m_investmentsList->header()->setMinimumHeight(height);
  m_investmentsList->header()->setMaximumHeight(height);
  m_investmentsList->header()->setFont(font);

  // setup cell font
  font = KMyMoneyGlobalSettings::listCellFont();
  m_investmentsList->setFont(font);

  // clear the table
  m_investmentsList->clear();

  // and the selected account in the combo box
  m_accountComboBox->setSelected(QString());

  // right align col headers for quantity, price and value
  for (int i = 2; i < 5; ++i) {
    m_investmentsList->headerItem()->setTextAlignment(i, Qt::AlignRight | Qt::AlignVCenter);
  }
}

void KInvestmentView::loadInvestmentTab(void)
{
  // no account selected
  emit accountSelected(MyMoneyAccount());

  // clear the current contents ...
  clear();

  // ... load the combobox widget and select current account ...
  loadAccounts();

  if (d->m_account.id().isEmpty()) {
    // if we don't have an account we bail out
    setEnabled(false);
    return;
  }
  setEnabled(true);

  MyMoneyFile* file = MyMoneyFile::instance();
  bool showClosedAccounts = kmymoney->toggleAction("view_show_all_accounts")->isChecked()
                            || !KMyMoneyGlobalSettings::hideClosedAccounts();
  try {
    d->m_account = file->account(d->m_account.id());
    QStringList securities = d->m_account.accountList();

    for (QStringList::ConstIterator it = securities.constBegin(); it != securities.constEnd(); ++it) {
      MyMoneyAccount acc = file->account(*it);
      if (!acc.isClosed() || showClosedAccounts)
        loadInvestmentItem(acc);
    }
  } catch (MyMoneyException* e) {
    qDebug("KInvestmentView::loadView() - selected account does not exist anymore");
    d->m_account = MyMoneyAccount();
    delete e;
  }

  //resize the column width
  m_investmentsList->resizeColumnToContents(0);
  m_investmentsList->resizeColumnToContents(1);
  m_investmentsList->resizeColumnToContents(2);
  m_investmentsList->resizeColumnToContents(3);

  // and tell everyone what's selected
  emit accountSelected(d->m_account);
}

void KInvestmentView::loadInvestmentItem(const MyMoneyAccount& account)
{
  QTreeWidgetItem* item = new QTreeWidgetItem(m_investmentsList);
  MyMoneySecurity security;
  MyMoneyFile* file = MyMoneyFile::instance();

  security = file->security(account.currencyId());
  MyMoneySecurity tradingCurrency = file->security(security.tradingCurrency());

  int prec = MyMoneyMoney::denomToPrec(tradingCurrency.smallestAccountFraction());

  //column 0 (COLUMN_NAME_INDEX) is the name of the stock
  item->setText(eInvestmentNameColumn, account.name());
  item->setData(eInvestmentNameColumn, Qt::UserRole, QVariant::fromValue(account));

  //column 1 (COLUMN_SYMBOL_INDEX) is the ticker symbol
  item->setText(eInvestmentSymbolColumn, security.tradingSymbol());

  //column 2 is the net value (price * quantity owned)
  MyMoneyPrice price = file->price(account.currencyId(), tradingCurrency.id());
  if (price.isValid()) {
    item->setText(eValueColumn, (file->balance(account.id()) * price.rate(tradingCurrency.id())).formatMoney(tradingCurrency.tradingSymbol(), prec));
  } else {
    item->setText(eValueColumn, "---");
  }
  item->setTextAlignment(eValueColumn, Qt::AlignRight | Qt::AlignVCenter);

  //column 3 (COLUMN_QUANTITY_INDEX) is the quantity of shares owned
  prec = MyMoneyMoney::denomToPrec(security.smallestAccountFraction());
  item->setText(eQuantityColumn, file->balance(account.id()).formatMoney("", prec));
  item->setTextAlignment(eQuantityColumn, Qt::AlignRight | Qt::AlignVCenter);

  //column 4 is the current price
  // Get the price precision from the configuration
  prec = KMyMoneyGlobalSettings::pricePrecision();

  // prec = MyMoneyMoney::denomToPrec(m_tradingCurrency.smallestAccountFraction());
  if (price.isValid()) {
    item->setText(ePriceColumn, price.rate(tradingCurrency.id()).formatMoney(tradingCurrency.tradingSymbol(), prec));
  } else {
    item->setText(ePriceColumn, "---");
  }
  item->setTextAlignment(ePriceColumn, Qt::AlignRight | Qt::AlignVCenter);
}

void KInvestmentView::showEvent(QShowEvent* event)
{
  emit aboutToShow();

  /*if (d->m_needReload) {
    loadInvestmentTab();
    d->m_needReload = false;
    d->m_newAccountLoaded = false;

  } else {
    emit accountSelected(d->m_account);
  }*/

  slotTabCurrentChanged(m_tab->currentWidget());

  // don't forget base class implementation
  QWidget::showEvent(event);
}

void KInvestmentView::loadSecuritiesList(void)
{
  m_securitiesList->setColumnWidth(eIdColumn, 0);
  m_securitiesList->setSortingEnabled(false);
  m_securitiesList->clear();

  QList<MyMoneySecurity> list = MyMoneyFile::instance()->securityList();
  QList<MyMoneySecurity>::ConstIterator it;
  if (m_showCurrencyButton->isChecked()) {
    list += MyMoneyFile::instance()->currencyList();
  }
  for (it = list.constBegin(); it != list.constEnd(); ++it) {
    QTreeWidgetItem* newItem = new QTreeWidgetItem(m_securitiesList);
    loadSecurityItem(newItem, *it);

  }
  m_securitiesList->setSortingEnabled(true);

  //resize column width
  m_securitiesList->resizeColumnToContents(1);
  m_securitiesList->resizeColumnToContents(2);
  m_securitiesList->resizeColumnToContents(3);
  m_securitiesList->resizeColumnToContents(4);
  m_securitiesList->resizeColumnToContents(5);
  m_securitiesList->resizeColumnToContents(6);

  slotUpdateSecuritiesButtons();
}

void KInvestmentView::loadSecurityItem(QTreeWidgetItem* item, const MyMoneySecurity& security)
{
  QString market = security.tradingMarket();
  MyMoneySecurity tradingCurrency;
  if (security.isCurrency())
    market = m_currencyMarket;
  else
    tradingCurrency = MyMoneyFile::instance()->security(security.tradingCurrency());

  item->setText(eIdColumn, security.id());
  item->setText(eTypeColumn, KMyMoneyUtils::securityTypeToString(security.securityType()));
  item->setText(eSecurityNameColumn, security.name());
  item->setText(eSecuritySymbolColumn, security.tradingSymbol());
  item->setText(eMarketColumn, market);
  item->setText(eCurrencyColumn, tradingCurrency.tradingSymbol());
  item->setTextAlignment(eCurrencyColumn, Qt::AlignHCenter);
  item->setText(eAcctFractionColumn, QString::number(security.smallestAccountFraction()));

  // smallestCashFraction is only applicable for currencies
  if (security.isCurrency())
    item->setText(eCashFractionColumn, QString::number(security.smallestCashFraction()));
}

void KInvestmentView::slotUpdateSecuritiesButtons(void)
{
  QTreeWidgetItem* item = m_securitiesList->currentItem();

  if (item) {
    MyMoneySecurity security = MyMoneyFile::instance()->security(item->text(eIdColumn).toLatin1());
    m_editSecurityButton->setEnabled(item->text(eMarketColumn) != m_currencyMarket);
    m_deleteSecurityButton->setEnabled(!MyMoneyFile::instance()->isReferenced(security));

  } else {
    m_editSecurityButton->setEnabled(false);
    m_deleteSecurityButton->setEnabled(false);
  }
}

void KInvestmentView::slotEditSecurity(void)
{
  QTreeWidgetItem* item = m_securitiesList->currentItem();
  if (item) {
    MyMoneySecurity security = MyMoneyFile::instance()->security(item->text(eIdColumn).toLatin1());

    QPointer<KNewInvestmentWizard> dlg = new KNewInvestmentWizard(security, this);
    dlg->setObjectName("KNewInvestmentWizard");
    if (dlg->exec() == QDialog::Accepted) {
      dlg->createObjects(QString());
      try {
        // For some reason, the item gets deselected, and the pointer
        // invalidated. So fix it here before continuing.
        item = m_securitiesList->findItems(security.id(), Qt::MatchExactly).at(0);
        m_securitiesList->setCurrentItem(item);
        if (item) {
          security = MyMoneyFile::instance()->security(item->text(eIdColumn).toLatin1());
          loadSecurityItem(item, security);
        }
      } catch (MyMoneyException* e) {
        KMessageBox::error(this, i18n("Failed to edit security: %1", e->what()));
        delete e;
      }
    }
    delete dlg;
  }
}

void KInvestmentView::slotDeleteSecurity(void)
{
  QTreeWidgetItem* item = m_securitiesList->currentItem();
  if (item) {
    MyMoneySecurity security = MyMoneyFile::instance()->security(item->text(eIdColumn).toLatin1());
    QString msg;
    QString dontAsk;
    if (security.isCurrency()) {
      msg = i18n("<p>Do you really want to remove the currency <b>%1</b> from the file?</p><p><i>Note: adding currencies is not currently supported.</i></p>", security.name());
      dontAsk = "DeleteCurrency";
    } else {
      msg = i18n("<p>Do you really want to remove the %1 <b>%2</b> from the file?</p>", KMyMoneyUtils::securityTypeToString(security.securityType()), security.name());
      dontAsk = "DeleteSecurity";
    }
    if (KMessageBox::questionYesNo(this, msg, i18n("Delete security"), KStandardGuiItem::yes(), KStandardGuiItem::no(), dontAsk) == KMessageBox::Yes) {
      MyMoneyFileTransaction ft;
      try {
        if (security.isCurrency())
          MyMoneyFile::instance()->removeCurrency(security);
        else
          MyMoneyFile::instance()->removeSecurity(security);
        ft.commit();
      } catch (MyMoneyException *e) {
        delete e;
      }
    }
  }
}

#include "kinvestmentview.moc"
