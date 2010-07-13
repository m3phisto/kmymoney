/***************************************************************************
                          kbudgetview.cpp
                          ---------------
    begin                : Thu Jan 10 2006
    copyright            : (C) 2006 by Darren Gould
    email                : darren_gould@gmx.de
                           Alvaro Soliverez <asoliverez@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kbudgetview.h"

#include <typeinfo>

// ----------------------------------------------------------------------------
// QT Includes

#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QTabWidget>
#include <QCheckBox>
#include <QToolTip>
#include <QList>
#include <QResizeEvent>
#include <QTreeWidgetItemIterator>
#include <QTimer>

// ----------------------------------------------------------------------------
// KDE Includes

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <kiconloader.h>
#include <kguiitem.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kcalendarsystem.h>
#include <kcombobox.h>

// ----------------------------------------------------------------------------
// Project Includes

#include <mymoneyfile.h>
#include <kmymoneyglobalsettings.h>
#include <kmymoneytitlelabel.h>
#include <kmymoneyedit.h>
#include <kbudgetvalues.h>
#include "knewbudgetdlg.h"
#include "kmymoney.h"
#include "models.h"

/**
  * @author Darren Gould
  * @author Thomas Baumgart
  *
  * This class represents an item in the budgets list view.
  */
class KBudgetListItem : public QTreeWidgetItem
{
public:
  /**
    * Constructor to be used to construct a budget entry object.
    *
    * @param parent pointer to the QTreeWidget object this entry should be
    *               added to.
    * @param budget const reference to MyMoneyBudget for which
    *               the QTreeWidget entry is constructed
    */
  KBudgetListItem(QTreeWidget *parent, const MyMoneyBudget& budget);
  ~KBudgetListItem();

  /**
    * This method is re-implemented from QListViewItem::paintCell().
    * Besides the standard implementation, the QPainter is set
    * according to the applications settings.
    */
  void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int align);

  const MyMoneyBudget& budget(void) {
    return m_budget;
  };
  void setBudget(const MyMoneyBudget& budget) {
    m_budget = budget;
  }

private:
  MyMoneyBudget  m_budget;
};

// *** KBudgetListItem Implementation ***
KBudgetListItem::KBudgetListItem(QTreeWidget *parent, const MyMoneyBudget& budget) :
    QTreeWidgetItem(parent),
    m_budget(budget)
{
  setText(0, budget.name());
  setText(1, QString("%1").arg(budget.budgetStart().year()));
  setFlags(flags() | Qt::ItemIsEditable);
  // allow in column rename
  //setRenameEnabled(0, true);
}

KBudgetListItem::~KBudgetListItem()
{
}

// *** KBudgetView Implementation ***
//TODO: This has to go to user settings
const int KBudgetView::m_iBudgetYearsAhead = 5;
const int KBudgetView::m_iBudgetYearsBack = 3;

BudgetAccountsProxyModel::BudgetAccountsProxyModel(QObject *parent/* = 0*/) :
    AccountsViewFilterProxyModel(parent)
{
  addAccountGroup(MyMoneyAccount::Income);
  addAccountGroup(MyMoneyAccount::Expense);
}

/**
  * This function was reimplemented to add the data needed by the other columns that this model
  * is adding besides the columns of the @ref AccountsModel.
  */
QVariant BudgetAccountsProxyModel::data(const QModelIndex &index, int role) const
{
  QModelIndex normalizedIndex = BudgetAccountsProxyModel::index(index.row(), 0, index.parent());
  if (role == AccountsModel::AccountBalanceRole ||
      role == AccountsModel::AccountBalanceDisplayRole ||
      role == AccountsModel::AccountValueRole ||
      role == AccountsModel::AccountValueDisplayRole) {
    QVariant accountData = data(normalizedIndex, AccountsModel::AccountRole);
    if (accountData.canConvert<MyMoneyAccount>()) {
      MyMoneyAccount account = accountData.value<MyMoneyAccount>();
      MyMoneyMoney balance = accountBalance(account.id());
      MyMoneyMoney value = accountValue(account, balance);
      switch (role) {
        case AccountsModel::AccountBalanceRole:
          return QVariant::fromValue(balance);
        case AccountsModel::AccountBalanceDisplayRole:
          if (MyMoneyFile::instance()->security(account.currencyId()) != MyMoneyFile::instance()->baseCurrency()) {
            return balance.formatMoney(MyMoneyFile::instance()->security(account.currencyId()));
          } else {
            return QVariant();
          }
        case AccountsModel::AccountValueRole:
          return QVariant::fromValue(value);
        case AccountsModel::AccountValueDisplayRole:
          return value.formatMoney(MyMoneyFile::instance()->baseCurrency());
      }
    }
  }
  if (role == AccountsModel::AccountTotalValueRole ||
      role == AccountsModel::AccountTotalValueDisplayRole) {
    MyMoneyMoney totalValue = computeTotalValue(mapToSource(normalizedIndex));
    switch (role) {
      case AccountsModel::AccountTotalValueRole:
        return QVariant::fromValue(totalValue);
      case AccountsModel::AccountTotalValueDisplayRole:
        return totalValue.formatMoney(MyMoneyFile::instance()->baseCurrency());
    }
  }
  return AccountsViewFilterProxyModel::data(index, role);
}

Qt::ItemFlags BudgetAccountsProxyModel::flags(const QModelIndex &index) const
{
  Qt::ItemFlags flags = AccountsViewFilterProxyModel::flags(index);
  if (!index.parent().isValid())
    return flags & ~Qt::ItemIsSelectable;
  return flags;
}


void BudgetAccountsProxyModel::setBudget(const MyMoneyBudget& budget)
{
  m_budget = budget;
  invalidate();
}

bool BudgetAccountsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
  if (hideUnusedIncomeExpenseAccounts()) {
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    QVariant accountData = sourceModel()->data(index, AccountsModel::AccountRole);
    if (accountData.canConvert<MyMoneyAccount>()) {
      MyMoneyAccount account = accountData.value<MyMoneyAccount>();
      MyMoneyMoney balance(0);
      // find out if the account is budgeted
      MyMoneyBudget::AccountGroup budgetAccount = m_budget.account(account.id());
      if (budgetAccount.id() == account.id()) {
        balance = budgetAccount.balance();
        switch (budgetAccount.budgetLevel()) {
          case MyMoneyBudget::AccountGroup::eMonthly:
            balance = balance * 12;
            break;
          default:
            break;
        }
      }
      if (!balance.isZero())
        return AccountsFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }
    for (int i = 0; i < sourceModel()->rowCount(index); ++i) {
      if (filterAcceptsRow(i, index))
        return AccountsFilterProxyModel::filterAcceptsRow(i, index);
    }
    return false;
  }
  return AccountsFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool BudgetAccountsProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
  if (source_column == AccountsModel::Tax || source_column == AccountsModel::VAT)
    return false;
  return AccountsFilterProxyModel::filterAcceptsColumn(source_column, source_parent);
}

MyMoneyMoney BudgetAccountsProxyModel::accountBalance(const QString &accountId) const
{
  MyMoneyMoney balance(0);
  // find out if the account is budgeted
  MyMoneyBudget::AccountGroup budgetAccount = m_budget.account(accountId);
  if (budgetAccount.id() == accountId) {
    balance = budgetAccount.balance();
    switch (budgetAccount.budgetLevel()) {
      case MyMoneyBudget::AccountGroup::eMonthly:
        balance = balance * 12;
        break;
      default:
        break;
    }
  }
  return balance;
}

MyMoneyMoney BudgetAccountsProxyModel::accountValue(const MyMoneyAccount &account, const MyMoneyMoney &balance) const
{
  return Models::instance()->accountsModel()->accountValue(account, balance);
}

MyMoneyMoney BudgetAccountsProxyModel::computeTotalValue(const QModelIndex &source_index) const
{
  MyMoneyMoney totalValue(0);
  QVariant accountData = sourceModel()->data(source_index, AccountsModel::AccountRole);
  if (accountData.canConvert<MyMoneyAccount>()) {
    MyMoneyAccount account = accountData.value<MyMoneyAccount>();
    totalValue = accountValue(account, accountBalance(account.id()));
    for (int i = 0; i < sourceModel()->rowCount(source_index); ++i) {
      totalValue += computeTotalValue(sourceModel()->index(i, 0, source_index));
    }
  }
  return totalValue;
}

KBudgetView::KBudgetView(QWidget *parent) :
    QWidget(parent),
    m_needReload(false),
    m_inSelection(false),
    m_budgetInEditing(false)
{
  setupUi(this);

  m_budgetList->setRootIsDecorated(false);
  m_budgetList->setContextMenuPolicy(Qt::CustomContextMenu);

  KGuiItem newButtonItem(QString(""),
                         KIcon("budget-add"),
                         i18n("Creates a new budget"),
                         i18n("Use this to create a new empty budget."));
  m_newButton->setGuiItem(newButtonItem);
  m_newButton->setToolTip(newButtonItem.toolTip());

  KGuiItem renameButtonItem(QString(""),
                            KIcon("budget-edit"),
                            i18n("Rename the current selected budget"),
                            i18n("Use this to start renaming the selected budget."));
  m_renameButton->setGuiItem(renameButtonItem);
  m_renameButton->setToolTip(renameButtonItem.toolTip());

  KGuiItem deleteButtonItem(QString(""),
                            KIcon("budget-delete"),
                            i18n("Delete the current selected budget"),
                            i18n("Use this to delete the selected budget."));
  m_deleteButton->setGuiItem(deleteButtonItem);
  m_deleteButton->setToolTip(deleteButtonItem.toolTip());

  KGuiItem updateButtonItem(QString(""),
                            KIcon("document-save"),
                            i18n("Accepts the entered values and stores the budget"),
                            i18n("Use this to store the modified data."));
  m_updateButton->setGuiItem(updateButtonItem);
  m_updateButton->setToolTip(updateButtonItem.toolTip());

  KGuiItem resetButtonItem(QString(""),
                           KIcon("edit-undo"),
                           i18n("Revert budget to last saved state"),
                           i18n("Use this to discard the modified data."));
  m_resetButton->setGuiItem(resetButtonItem);
  m_resetButton->setToolTip(resetButtonItem.toolTip());

  KGuiItem collapseGuiItem("",
                           KIcon("zoom-out"),
                           QString(),
                           QString());
  KGuiItem expandGuiItem("",
                         KIcon("zoom-in"),
                         QString(),
                         QString());
  m_collapseButton->setGuiItem(collapseGuiItem);
  m_expandButton->setGuiItem(expandGuiItem);

  m_filterProxyModel = new BudgetAccountsProxyModel(this);
  m_filterProxyModel->setSourceModel(Models::instance()->accountsModel());
  m_filterProxyModel->setFilterKeyColumn(-1);
  m_accountTree->setConfigGroupName("KBudgetsView");
  m_accountTree->setAlternatingRowColors(true);
  m_accountTree->setIconSize(QSize(22, 22));
  m_accountTree->setSortingEnabled(true);
  m_accountTree->setModel(m_filterProxyModel);

  // let the model know if the item is expanded or collapsed
  connect(m_accountTree, SIGNAL(collapsed(const QModelIndex &)), m_filterProxyModel, SLOT(collapsed(const QModelIndex &)));
  connect(m_accountTree, SIGNAL(expanded(const QModelIndex &)), m_filterProxyModel, SLOT(expanded(const QModelIndex &)));

  connect(m_accountTree, SIGNAL(selectObject(const MyMoneyObject&)), this, SLOT(slotSelectAccount(const MyMoneyObject&)));
  connect(m_accountTree, SIGNAL(selectObject(const MyMoneyObject&)), kmymoney, SLOT(slotSelectAccount(const MyMoneyObject&)));
  connect(m_accountTree, SIGNAL(selectObject(const MyMoneyObject&)), kmymoney, SLOT(slotSelectInstitution(const MyMoneyObject&)));
  connect(m_accountTree, SIGNAL(selectObject(const MyMoneyObject&)), kmymoney, SLOT(slotSelectInvestment(const MyMoneyObject&)));
  connect(m_accountTree, SIGNAL(openContextMenu(const MyMoneyObject&)), kmymoney, SLOT(slotShowAccountContextMenu(const MyMoneyObject&)));
  connect(m_accountTree, SIGNAL(openObject(const MyMoneyObject&)), kmymoney, SLOT(slotAccountOpen(const MyMoneyObject&)));

  connect(m_budgetList, SIGNAL(customContextMenuRequested(const QPoint&)),
          this, SLOT(slotOpenContextMenu(const QPoint&)));
  connect(m_budgetList, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(slotRenameBudget(QTreeWidgetItem*)));
  connect(m_budgetList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(slotSelectBudget()));

  connect(m_cbBudgetSubaccounts, SIGNAL(clicked()), this, SLOT(cb_includesSubaccounts_clicked()));

  // connect the buttons to the actions. Make sure the enabled state
  // of the actions is reflected by the buttons
  connect(m_renameButton, SIGNAL(clicked()), kmymoney->action("budget_rename"), SLOT(trigger()));
  connect(m_deleteButton, SIGNAL(clicked()), kmymoney->action("budget_delete"), SLOT(trigger()));

  connect(m_budgetValue, SIGNAL(valuesChanged()), this, SLOT(slotBudgetedAmountChanged()));

  connect(m_newButton, SIGNAL(clicked()), this, SLOT(slotNewBudget()));
  connect(m_updateButton, SIGNAL(pressed()), this, SLOT(slotUpdateBudget()));
  connect(m_resetButton, SIGNAL(pressed()), this, SLOT(slotResetBudget()));

  connect(m_hideUnusedButton, SIGNAL(toggled(bool)), this, SLOT(slotHideUnused(bool)));

  connect(m_collapseButton, SIGNAL(clicked()), this, SLOT(slotExpandCollapse()));
  connect(m_expandButton, SIGNAL(clicked()), this, SLOT(slotExpandCollapse()));

  // connect the two buttons to all required slots
  connect(m_collapseButton, SIGNAL(clicked()), this, SLOT(slotExpandCollapse()));
  connect(m_collapseButton, SIGNAL(clicked()), m_accountTree, SLOT(collapseAll()));
  connect(m_accountTree, SIGNAL(collapsedAll()), m_filterProxyModel, SLOT(collapseAll()));
  connect(m_expandButton, SIGNAL(clicked()), this, SLOT(slotExpandCollapse()));
  connect(m_expandButton, SIGNAL(clicked()), m_accountTree, SLOT(expandAll()));
  connect(m_accountTree, SIGNAL(expandedAll()), m_filterProxyModel, SLOT(expandAll()));

  connect(m_searchWidget, SIGNAL(textChanged(const QString &)), m_filterProxyModel, SLOT(setFilterFixedString(const QString &)));

  // setup initial state
  m_newButton->setEnabled(kmymoney->action("budget_new")->isEnabled());
  m_renameButton->setEnabled(kmymoney->action("budget_rename")->isEnabled());
  m_deleteButton->setEnabled(kmymoney->action("budget_delete")->isEnabled());

  connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotRefreshView()));

  KConfigGroup grp = KGlobal::config()->group("Last Use Settings");
  QList<int> sizes = grp.readEntry("KBudgetViewSplitterSize", QList<int>());
  if (sizes.size() == 2) {
    if ((sizes[0] == 0) || (sizes[1] == 0)) {
      sizes[0] = 1;
      sizes[1] = 3;
    }
    m_splitter->setSizes(sizes);
  }
}

KBudgetView::~KBudgetView()
{
  // remember the splitter settings for startup
  KConfigGroup grp = KGlobal::config()->group("Last Use Settings");
  grp.writeEntry("KBudgetViewSplitterSize", m_splitter->sizes());
}

void KBudgetView::showEvent(QShowEvent * event)
{
  QTimer::singleShot(50, this, SLOT(slotRearrange()));
  if (m_needReload) {
    slotRefreshView();
  }
  QWidget::showEvent(event);
}

void KBudgetView::slotRearrange(void)
{
  resizeEvent(0);
}

void KBudgetView::resizeEvent(QResizeEvent* ev)
{
  // resize the register
  QWidget::resizeEvent(ev);
}

void KBudgetView::slotReloadView(void)
{
  ::timetrace("Start KBudgetView::slotReloadView");
  slotRearrange();
  ::timetrace("Done KBudgetView::slotReloadView");
}

void KBudgetView::loadBudgets(void)
{
  ::timetrace("Start KBudgetView::loadBudgets");

  m_filterProxyModel->invalidate();

  // remember which item is currently selected
  QString id = m_budget.id();

  // clear the budget list
  m_budgetList->clear();
  m_budgetValue->clear();

  // add the correct years to the drop down list
  QDate date = QDate::currentDate();
  int iStartYear = date.year() - m_iBudgetYearsBack;

  m_yearList.clear();
  for (int i = 0; i < m_iBudgetYearsAhead + m_iBudgetYearsBack; i++)
    m_yearList += QString::number(iStartYear + i);

  KBudgetListItem* currentItem = 0;

  QList<MyMoneyBudget> list = MyMoneyFile::instance()->budgetList();
  QList<MyMoneyBudget>::ConstIterator it;
  for (it = list.constBegin(); it != list.constEnd(); ++it) {
    KBudgetListItem* item = new KBudgetListItem(m_budgetList, *it);

    // create a list of unique years
    if (m_yearList.indexOf(QString::number((*it).budgetStart().year())) == -1)
      m_yearList += QString::number((*it).budgetStart().year());

    //sort the list by name
    m_budgetList->sortItems(0, Qt::AscendingOrder);

    if (item->budget().id() == id) {
      m_budget = (*it);
      currentItem = item;
      item->setSelected(true);
    }
  }
  m_yearList.sort();

  if (currentItem) {
    m_budgetList->setCurrentItem(currentItem);
  }

  // reset the status of the buttons
  m_updateButton->setEnabled(false);
  m_resetButton->setEnabled(false);

  // make sure the world around us knows what we have selected
  slotSelectBudget();

  ::timetrace("End KBudgetView::loadBudgets");
}

void KBudgetView::ensureBudgetVisible(const QString& id)
{
  QTreeWidgetItemIterator widgetIt = QTreeWidgetItemIterator(m_budgetList);
  while (*widgetIt) {
    KBudgetListItem* p = dynamic_cast<KBudgetListItem*>(*widgetIt);
    if ((p)->budget().id() == id) {
      m_budgetList->scrollToItem((p), QAbstractItemView::PositionAtCenter);
      m_budgetList->setCurrentItem(p, 0, QItemSelectionModel::ClearAndSelect);      // active item and deselect all others
    }
  }
}

void KBudgetView::slotRefreshView(void)
{
  if (isVisible()) {
    if (m_inSelection)
      QTimer::singleShot(0, this, SLOT(slotRefreshView()));
    else {
      loadBudgets();
      m_needReload = false;
    }
  } else {
    m_needReload = true;
  }
}

void KBudgetView::loadAccounts(void)
{
  ::timetrace("start load budget account view");

  // if no budgets are selected, don't load the accounts
  // and clear out the previously shown list
  if (m_budget.id().isEmpty()) {
    m_budgetValue->clear();
    m_updateButton->setEnabled(false);
    m_resetButton->setEnabled(false);
    ::timetrace("done load budgets view");
    return;
  }
  m_updateButton->setEnabled(!(selectedBudget() == m_budget));
  m_resetButton->setEnabled(!(selectedBudget() == m_budget));

  m_filterProxyModel->setBudget(m_budget);

  // and in case we need to show things expanded, we'll do so
  if (KMyMoneyGlobalSettings::showAccountsExpanded()) {
    m_accountTree->expandAll();
  }

  ::timetrace("done load budgets view");
}

void KBudgetView::askSave(void)
{
  // check if the content of a currently selected budget was modified
  // and ask to store the data
  if (m_updateButton->isEnabled()) {
    if (KMessageBox::questionYesNo(this, QString("<qt>%1</qt>").arg(
                                     i18n("Do you want to save the changes for <b>%1</b>", m_budget.name())),
                                   i18n("Save changes")) == KMessageBox::Yes) {
      m_inSelection = true;
      slotUpdateBudget();
      m_inSelection = false;
    }
  }
}

void KBudgetView::slotRefreshHideUnusedButton(void)
{
  m_hideUnusedButton->setDisabled(m_budget.getaccounts().isEmpty());
}

void KBudgetView::slotSelectBudget(void)
{
  askSave();
  KBudgetListItem* item;
  m_budgetInEditing = false;

  QTreeWidgetItemIterator widgetIt = QTreeWidgetItemIterator(m_budgetList);
  if (m_budget.id().isEmpty()) {
    item = dynamic_cast<KBudgetListItem*>(*widgetIt);
    if (item) {
      m_budgetList->blockSignals(true);
      m_budgetList->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
      m_budgetList->blockSignals(false);
    }
  }

  m_accountTree->setEnabled(false);
  m_assignmentBox->setEnabled(false);
  m_budget = MyMoneyBudget();

  QTreeWidgetItemIterator it_l(m_budgetList, QTreeWidgetItemIterator::Selected);
  item = dynamic_cast<KBudgetListItem*>(*it_l);
  if (item) {
    m_budget = item->budget();
    m_accountTree->setEnabled(true);
  }

  slotRefreshHideUnusedButton();
  loadAccounts();

  QList<MyMoneyBudget> budgetList;
  if (!m_budget.id().isEmpty())
    budgetList << m_budget;
  emit selectObjects(budgetList);
}

void KBudgetView::slotHideUnused(bool toggled)
{
  // make sure we show all items for an empty budget
  bool prevState = !toggled;
  slotRefreshHideUnusedButton();
  if (prevState != m_hideUnusedButton->isChecked())
    m_filterProxyModel->setHideUnusedIncomeExpenseAccounts(m_hideUnusedButton->isChecked());
}

const MyMoneyBudget& KBudgetView::selectedBudget(void) const
{
  static MyMoneyBudget nullBudget;

  QTreeWidgetItemIterator it_l(m_budgetList, QTreeWidgetItemIterator::Selected);
  KBudgetListItem* item = dynamic_cast<KBudgetListItem*>(*it_l);
  if (item) {
    return item->budget();
  }
  return nullBudget;
}

void KBudgetView::slotOpenContextMenu(const QPoint& p)
{
  KBudgetListItem* item = dynamic_cast<KBudgetListItem*>(m_budgetList->itemAt(p));
  if (item)
    emit openContextMenu(item->budget());
  else
    emit openContextMenu(MyMoneyBudget());
}

void KBudgetView::slotStartRename(void)
{
  m_budgetInEditing = true;
  QTreeWidgetItemIterator it_l(m_budgetList, QTreeWidgetItemIterator::Selected);
  QTreeWidgetItem* it_v;
  if ((it_v = *it_l) != 0) {
    m_budgetList->editItem(it_v, 0);
  }
}

// This variant is only called when a single budget is selected and renamed.
void KBudgetView::slotRenameBudget(QTreeWidgetItem* p)
{
  KBudgetListItem *pBudget = dynamic_cast<KBudgetListItem*>(p);

  //if there is no current item selected, exit
  if (m_budgetInEditing == false || !m_budgetList->currentItem() || p != m_budgetList->currentItem())
    return;

  m_budgetInEditing = false;

  //kDebug() << "[KPayeesView::slotRenamePayee]";
  // create a copy of the new name without appended whitespaces
  QString new_name = p->text(0);

  if (pBudget->budget().name() != new_name) {
    MyMoneyFileTransaction ft;
    try {
      // check if we already have a budget with the new name
      try {
        // this function call will throw an exception, if the budget
        // hasn't been found.
        MyMoneyFile::instance()->budgetByName(new_name);
        // the name already exists, ask the user whether he's sure to keep the name
        if (KMessageBox::questionYesNo(this,
                                       i18n("A budget with the name '%1' already exists. It is not advisable to have "
                                            "multiple budgets with the same identification name. Are you sure you would like "
                                            "to rename the budget?", new_name)) != KMessageBox::Yes) {
          p->setText(0, pBudget->budget().name());
          return;
        }
      } catch (MyMoneyException *e) {
        // all ok, the name is unique
        delete e;
      }

      MyMoneyBudget b = pBudget->budget();
      b.setName(new_name);
      // don't use pBudget beyond this point as it will change due to call to modifyBudget
      pBudget = 0;

      MyMoneyFile::instance()->modifyBudget(b);

      // the above call to modifyBudget will reload the view so
      // all references and pointers to the view have to be
      // re-established. You cannot use pBudget beyond this point!!!
      ft.commit();

    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(0, i18n("Unable to modify budget"),
                                 (e->what() + ' ' + i18n("thrown in") + ' ' + e->file() + ":%1").arg(e->line()));
      delete e;
    }
  } else {
    pBudget->setText(0, new_name);
  }
}

void KBudgetView::slotSelectAccount(const MyMoneyObject &obj)
{
  m_assignmentBox->setEnabled(false);
  if (typeid(obj) != typeid(MyMoneyAccount))
    return;

  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
  m_assignmentBox->setEnabled(true);

  if (m_budget.id().isEmpty())
    return;

  QString id = acc.id();
  m_leAccounts->setText(MyMoneyFile::instance()->accountToCategory(id));
  m_cbBudgetSubaccounts->setChecked(m_budget.account(id).budgetSubaccounts());
  m_accountTotal->setValue(m_budget.account(id).totalBalance());
  MyMoneyBudget::AccountGroup budgetAccount = m_budget.account(id);
  if (id != budgetAccount.id()) {
    budgetAccount.setBudgetLevel(MyMoneyBudget::AccountGroup::eMonthly);
  }
  m_budgetValue->setBudgetValues(m_budget, budgetAccount);
}

void KBudgetView::slotExpandCollapse(void)
{
  if (sender()) {
    KMyMoneyGlobalSettings::setShowAccountsExpanded(sender() == m_expandButton);
  }
}

void KBudgetView::slotBudgetedAmountChanged(void)
{
  if (m_budget.id().isEmpty())
    return;

  QModelIndexList indexes = m_accountTree->selectionModel()->selectedIndexes();
  if (indexes.empty())
    return;
  QString accountID = m_accountTree->model()->data(indexes.front(), AccountsModel::AccountIdRole).toString();

  MyMoneyBudget::AccountGroup accountGroup = m_budget.account(accountID);
  accountGroup.setId(accountID);
  m_budgetValue->budgetValues(m_budget, accountGroup);
  m_budget.setAccount(accountGroup, accountID);

  m_filterProxyModel->setBudget(m_budget);
  m_accountTotal->setValue(accountGroup.totalBalance());

  m_updateButton->setEnabled(!(selectedBudget() == m_budget));
  m_resetButton->setEnabled(!(selectedBudget() == m_budget));
}

void KBudgetView::AccountEnter()
{
  if (m_budget.id().isEmpty())
    return;
}

void KBudgetView::cb_includesSubaccounts_clicked()
{
  if (m_budget.id().isEmpty())
    return;

  QModelIndexList indexes = m_accountTree->selectionModel()->selectedIndexes();
  if (!indexes.empty()) {
    QString accountID = m_accountTree->model()->data(indexes.front(), AccountsModel::AccountIdRole).toString();
    // now, we get a reference to the accountgroup, to mofify its atribute,
    // and then put the resulting account group instead of the original

    MyMoneyBudget::AccountGroup auxAccount = m_budget.account(accountID);
    auxAccount.setBudgetSubaccounts(m_cbBudgetSubaccounts->isChecked());
    m_budget.setAccount(auxAccount, accountID);

    loadAccounts();
  }
}

void KBudgetView::slotNewBudget(void)
{
  askSave();
  kmymoney->action("budget_new")->trigger();
}

void KBudgetView::slotResetBudget(void)
{
  try {
    m_budget = MyMoneyFile::instance()->budget(m_budget.id());
    loadAccounts();
  } catch (MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Unable to reset budget"),
                               (e->what() + ' ' + i18n("thrown in") + ' ' + e->file() + ":%1").arg(e->line()));
    delete e;
  }
}

void KBudgetView::slotUpdateBudget(void)
{
  MyMoneyFileTransaction ft;
  try {
    MyMoneyFile::instance()->modifyBudget(m_budget);
    ft.commit();
    slotRefreshHideUnusedButton();
  } catch (MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Unable to modify budget"),
                               (e->what() + ' ' + i18n("thrown in") + ' ' + e->file() + ":%1").arg(e->line()));
    delete e;
  }
}

void KBudgetView::languageChange(void)
{
  QWidget::languageChange();

  m_newButton->setText(QString());
  m_renameButton->setText(QString());
  m_deleteButton->setText(QString());
  m_updateButton->setText(QString());
  m_resetButton->setText(QString());
}

#include "kbudgetview.moc"
