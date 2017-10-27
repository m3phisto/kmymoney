/***************************************************************************
                          mymoneyschedule.cpp
                             -------------------
    copyright            : (C) 2000-2002 by Michael Edwardes <mte@users.sourceforge.net>
                           (C) 2007 by Thomas Baumgart <ipwizard@users.sourceforge.net>

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mymoneyschedule.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QList>
#include <QMap>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyutils.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "imymoneyprocessingcalendar.h"
#include "mymoneystoragenames.h"

using namespace MyMoneyStorageNodes;
using namespace eMyMoney;

static IMyMoneyProcessingCalendar* processingCalendarPtr = 0;

MyMoneySchedule::MyMoneySchedule() :
    MyMoneyObject()
{
  // Set up the default values
  m_occurrence = Schedule::Occurrence::Any;
  m_occurrenceMultiplier = 1;
  m_type = Schedule::Type::Any;
  m_paymentType = Schedule::PaymentType::Any;
  m_fixed = false;
  m_autoEnter = false;
  m_startDate = QDate();
  m_endDate = QDate();
  m_lastPayment = QDate();
  m_weekendOption = Schedule::WeekendOption::MoveNothing;
}

MyMoneySchedule::MyMoneySchedule(const QString& name, Schedule::Type type,
                                 Schedule::Occurrence occurrence, int occurrenceMultiplier,
                                 Schedule::PaymentType paymentType,
                                 const QDate& /* startDate */,
                                 const QDate& endDate,
                                 bool fixed, bool autoEnter) :
    MyMoneyObject()
{
  // Set up the default values
  m_name = name;
  m_occurrence = occurrence;
  m_occurrenceMultiplier = occurrenceMultiplier;
  simpleToCompoundOccurrence(m_occurrenceMultiplier, m_occurrence);
  m_type = type;
  m_paymentType = paymentType;
  m_fixed = fixed;
  m_autoEnter = autoEnter;
  m_startDate = QDate();
  m_endDate = endDate;
  m_lastPayment = QDate();
  m_weekendOption = Schedule::WeekendOption::MoveNothing;
}

MyMoneySchedule::MyMoneySchedule(const QDomElement& node) :
    MyMoneyObject(node)
{
  if (nodeNames[nnScheduleTX] != node.tagName())
    throw MYMONEYEXCEPTION("Node was not SCHEDULED_TX");

  m_name = node.attribute(getAttrName(anName));
  m_startDate = stringToDate(node.attribute(getAttrName(anStartDate)));
  m_endDate = stringToDate(node.attribute(getAttrName(anEndDate)));
  m_lastPayment = stringToDate(node.attribute(getAttrName(anLastPayment)));

  m_type = static_cast<Schedule::Type>(node.attribute(getAttrName(anType)).toInt());
  m_paymentType = static_cast<Schedule::PaymentType>(node.attribute(getAttrName(anPaymentType)).toInt());
  m_occurrence = static_cast<Schedule::Occurrence>(node.attribute(getAttrName(anOccurence)).toInt()); // krazy:exclude=spelling
  m_occurrenceMultiplier = node.attribute(getAttrName(anOccurenceMultiplier), "1").toInt(); // krazy:exclude=spelling
  // Convert to compound occurrence
  simpleToCompoundOccurrence(m_occurrenceMultiplier, m_occurrence);
  m_autoEnter = static_cast<bool>(node.attribute(getAttrName(anAutoEnter)).toInt());
  m_fixed = static_cast<bool>(node.attribute(getAttrName(anFixed)).toInt());
  m_weekendOption = static_cast<Schedule::WeekendOption>(node.attribute(getAttrName(anWeekendOption)).toInt());

  // read in the associated transaction
  QDomNodeList nodeList = node.elementsByTagName(nodeNames[nnTransaction]);
  if (nodeList.count() == 0)
    throw MYMONEYEXCEPTION("SCHEDULED_TX has no TRANSACTION node");

  setTransaction(MyMoneyTransaction(nodeList.item(0).toElement(), false), true);

  // some old versions did not remove the entry date and post date fields
  // in the schedule. So if this is the case, we deal with a very old transaction
  // and can't use the post date field as next due date. Hence, we wipe it out here
  if (m_transaction.entryDate().isValid()) {
    m_transaction.setPostDate(QDate());
    m_transaction.setEntryDate(QDate());
  }

  // readin the recorded payments
  nodeList = node.elementsByTagName(getElName(enPayments));
  if (nodeList.count() > 0) {
    nodeList = nodeList.item(0).toElement().elementsByTagName(getElName(enPayment));
    for (int i = 0; i < nodeList.count(); ++i) {
      m_recordedPayments << stringToDate(nodeList.item(i).toElement().attribute(getAttrName(anDate)));
    }
  }

  // if the next due date is not set (comes from old version)
  // then set it up the old way
  if (!nextDueDate().isValid() && !m_lastPayment.isValid()) {
    m_transaction.setPostDate(m_startDate);
    // clear it, because the schedule has never been used
    m_startDate = QDate();
  }

  // There are reports that lastPayment and nextDueDate are identical or
  // that nextDueDate is older than lastPayment. This could
  // be caused by older versions of the application. In this case, we just
  // clear out the nextDueDate and let it calculate from the lastPayment.
  if (nextDueDate().isValid() && nextDueDate() <= m_lastPayment) {
    m_transaction.setPostDate(QDate());
  }

  if (!nextDueDate().isValid()) {
    m_transaction.setPostDate(m_startDate);
    m_transaction.setPostDate(nextPayment(m_lastPayment.addDays(1)));
  }
}

MyMoneySchedule::MyMoneySchedule(const QString& id, const MyMoneySchedule& right) :
    MyMoneyObject(id)
{
  *this = right;
  setId(id);
}

Schedule::Occurrence MyMoneySchedule::occurrence() const
{
  Schedule::Occurrence occ = m_occurrence;
  int mult = m_occurrenceMultiplier;
  compoundToSimpleOccurrence(mult, occ);
  return occ;
}

void MyMoneySchedule::setStartDate(const QDate& date)
{
  m_startDate = date;
}

void MyMoneySchedule::setPaymentType(Schedule::PaymentType type)
{
  m_paymentType = type;
}

void MyMoneySchedule::setFixed(bool fixed)
{
  m_fixed = fixed;
}

void MyMoneySchedule::setTransaction(const MyMoneyTransaction& transaction)
{
  setTransaction(transaction, false);
}

void MyMoneySchedule::setTransaction(const MyMoneyTransaction& transaction, bool noDateCheck)
{
  MyMoneyTransaction t = transaction;
  if (!noDateCheck) {
    // don't allow a transaction that has no due date
    // if we get something like that, then we use the
    // the current next due date. If that is also invalid
    // we can't help it.
    if (!t.postDate().isValid()) {
      t.setPostDate(m_transaction.postDate());
    }

    if (!t.postDate().isValid())
      return;
  }

  // make sure to clear out some unused information in scheduled transactions
  // we need to do this for the case that the transaction passed as argument
  // is a matched or imported transaction.
  QList<MyMoneySplit> splits = t.splits();
  if (splits.count() > 0) {
    QList<MyMoneySplit>::const_iterator it_s;
    for (it_s = splits.constBegin(); it_s != splits.constEnd(); ++it_s) {
      MyMoneySplit s = *it_s;
      // clear out the bankID
      if (!(*it_s).bankID().isEmpty()) {
        s.setBankID(QString());
        t.modifySplit(s);
      }

      // only clear payees from second split onwards
      if (it_s == splits.constBegin())
        continue;

      if (!(*it_s).payeeId().isEmpty()) {
        // but only if the split references an income/expense category
        MyMoneyFile* file = MyMoneyFile::instance();
        // some unit tests don't have a storage attached, so we
        // simply skip the test
        // Don't check for accounts with an id of 'Phony-ID' which is used
        // internally for non-existing accounts (during creation of accounts)
        if (file->storageAttached() && s.accountId() != QString("Phony-ID")) {
          MyMoneyAccount acc = file->account(s.accountId());
          if (acc.isIncomeExpense()) {
            s.setPayeeId(QString());
            t.modifySplit(s);
          }
        }
      }
    }
  }

  m_transaction = t;
  // make sure that the transaction does not have an id so that we can enter
  // it into the engine
  m_transaction.clearId();
}

void MyMoneySchedule::setEndDate(const QDate& date)
{
  m_endDate = date;
}

void MyMoneySchedule::setAutoEnter(bool autoenter)
{
  m_autoEnter = autoenter;
}

const QDate& MyMoneySchedule::startDate() const
{
  if (m_startDate.isValid())
    return m_startDate;
  return nextDueDate();
}

const QDate& MyMoneySchedule::nextDueDate() const
{
  return m_transaction.postDate();
}

QDate MyMoneySchedule::adjustedNextDueDate() const
{
  if (isFinished())
    return QDate();

  return adjustedDate(nextDueDate(), weekendOption());
}

QDate MyMoneySchedule::adjustedDate(QDate date, Schedule::WeekendOption option) const
{
  if (!date.isValid() || option == Schedule::WeekendOption::MoveNothing || isProcessingDate(date))
    return date;

  int step = 1;
  if (option == Schedule::WeekendOption::MoveBefore)
    step = -1;

  while (!isProcessingDate(date))
    date = date.addDays(step);

  return date;
}

void MyMoneySchedule::setNextDueDate(const QDate& date)
{
  if (date.isValid()) {
    m_transaction.setPostDate(date);
    // m_startDate = date;
  }
}

void MyMoneySchedule::setLastPayment(const QDate& date)
{
  // Delete all payments older than date
  QList<QDate>::Iterator it;
  QList<QDate> delList;

  for (it = m_recordedPayments.begin(); it != m_recordedPayments.end(); ++it) {
    if (*it < date || !date.isValid())
      delList.append(*it);
  }

  for (it = delList.begin(); it != delList.end(); ++it) {
    m_recordedPayments.removeAll(*it);
  }

  m_lastPayment = date;
  if (!m_startDate.isValid())
    m_startDate = date;
}

void MyMoneySchedule::setName(const QString& nm)
{
  m_name = nm;
}

void MyMoneySchedule::setOccurrence(Schedule::Occurrence occ)
{
  Schedule::Occurrence occ2 = occ;
  int mult = 1;
  simpleToCompoundOccurrence(mult, occ2);
  setOccurrencePeriod(occ2);
  setOccurrenceMultiplier(mult);
}

void MyMoneySchedule::setOccurrencePeriod(Schedule::Occurrence occ)
{
  m_occurrence = occ;
}

void MyMoneySchedule::setOccurrenceMultiplier(int occmultiplier)
{
  m_occurrenceMultiplier = occmultiplier < 1 ? 1 : occmultiplier;
}

void MyMoneySchedule::setType(Schedule::Type type)
{
  m_type = type;
}

void MyMoneySchedule::validate(bool id_check) const
{
  /* Check the supplied instance is valid...
   *
   * To be valid it must not have the id set and have the following fields set:
   *
   * m_occurrence
   * m_type
   * m_startDate
   * m_paymentType
   * m_transaction
   *   the transaction must contain at least one split (two is better ;-)  )
   */
  if (id_check && !m_id.isEmpty())
    throw MYMONEYEXCEPTION("ID for schedule not empty when required");

  if (m_occurrence == Schedule::Occurrence::Any)
    throw MYMONEYEXCEPTION("Invalid occurrence type for schedule");

  if (m_type == Schedule::Type::Any)
    throw MYMONEYEXCEPTION("Invalid type for schedule");

  if (!nextDueDate().isValid())
    throw MYMONEYEXCEPTION("Invalid next due date for schedule");

  if (m_paymentType == Schedule::PaymentType::Any)
    throw MYMONEYEXCEPTION("Invalid payment type for schedule");

  if (m_transaction.splitCount() == 0)
    throw MYMONEYEXCEPTION("Scheduled transaction does not contain splits");

  // Check the payment types
  switch (m_type) {
    case Schedule::Type::Bill:
      if (m_paymentType == Schedule::PaymentType::DirectDeposit || m_paymentType == Schedule::PaymentType::ManualDeposit)
        throw MYMONEYEXCEPTION("Invalid payment type for bills");
      break;

    case Schedule::Type::Deposit:
      if (m_paymentType == Schedule::PaymentType::DirectDebit || m_paymentType == Schedule::PaymentType::WriteChecque)
        throw MYMONEYEXCEPTION("Invalid payment type for deposits");
      break;

    case Schedule::Type::Any:
      throw MYMONEYEXCEPTION("Invalid type ANY");
      break;

    case Schedule::Type::Transfer:
//        if (m_paymentType == DirectDeposit || m_paymentType == ManualDeposit)
//          return false;
      break;

    case Schedule::Type::LoanPayment:
      break;
  }
}

QDate MyMoneySchedule::adjustedNextPayment(const QDate& refDate) const
{
  return nextPaymentDate(true, refDate);
}

QDate MyMoneySchedule::nextPayment(const QDate& refDate) const
{
  return nextPaymentDate(false, refDate);
}

QDate MyMoneySchedule::nextPaymentDate(const bool& adjust, const QDate& refDate) const
{
  Schedule::WeekendOption option(adjust ? weekendOption() :
                        Schedule::WeekendOption::MoveNothing);

  QDate adjEndDate(adjustedDate(m_endDate, option));

  // if the enddate is valid and it is before the reference date,
  // then there will be no more payments.
  if (adjEndDate.isValid() && adjEndDate < refDate) {
    return QDate();
  }

  QDate dueDate(nextDueDate());
  QDate paymentDate(adjustedDate(dueDate, option));

  if (paymentDate.isValid() &&
      (paymentDate <= refDate || m_recordedPayments.contains(dueDate))) {
    switch (m_occurrence) {
      case Schedule::Occurrence::Once:
        // If the lastPayment is already set or the payment should have been
        // prior to the reference date then invalidate the payment date.
        if (m_lastPayment.isValid() || paymentDate <= refDate)
          paymentDate = QDate();
        break;

      case Schedule::Occurrence::Daily: {
          int step = m_occurrenceMultiplier;
          do {
            dueDate = dueDate.addDays(step);
            paymentDate = adjustedDate(dueDate, option);
          } while (paymentDate.isValid() &&
                   (paymentDate <= refDate ||
                    m_recordedPayments.contains(dueDate)));
        }
        break;

      case Schedule::Occurrence::Weekly: {
          int step = 7 * m_occurrenceMultiplier;
          do {
            dueDate = dueDate.addDays(step);
            paymentDate = adjustedDate(dueDate, option);
          } while (paymentDate.isValid() &&
                   (paymentDate <= refDate ||
                    m_recordedPayments.contains(dueDate)));
        }
        break;

      case Schedule::Occurrence::EveryHalfMonth:
        do {
          dueDate = addHalfMonths(dueDate, m_occurrenceMultiplier);
          paymentDate = adjustedDate(dueDate, option);
        } while (paymentDate.isValid() &&
                 (paymentDate <= refDate ||
                  m_recordedPayments.contains(dueDate)));
        break;

      case Schedule::Occurrence::Monthly:
        do {
          dueDate = dueDate.addMonths(m_occurrenceMultiplier);
          fixDate(dueDate);
          paymentDate = adjustedDate(dueDate, option);
        } while (paymentDate.isValid() &&
                 (paymentDate <= refDate ||
                  m_recordedPayments.contains(dueDate)));
        break;

      case Schedule::Occurrence::Yearly:
        do {
          dueDate = dueDate.addYears(m_occurrenceMultiplier);
          fixDate(dueDate);
          paymentDate = adjustedDate(dueDate, option);
        } while (paymentDate.isValid() &&
                 (paymentDate <= refDate ||
                  m_recordedPayments.contains(dueDate)));
        break;

      case Schedule::Occurrence::Any:
      default:
        paymentDate = QDate();
        break;
    }
  }
  if (paymentDate.isValid() && adjEndDate.isValid() && paymentDate > adjEndDate)
    paymentDate = QDate();

  return paymentDate;
}

QList<QDate> MyMoneySchedule::paymentDates(const QDate& _startDate, const QDate& _endDate) const
{
  QDate paymentDate(nextDueDate());
  QList<QDate> theDates;

  Schedule::WeekendOption option(weekendOption());

  QDate endDate(_endDate);
  if (willEnd() && m_endDate < endDate) {
    // consider the adjusted end date instead of the plain end date
    endDate = adjustedDate(m_endDate, option);
  }

  QDate start_date(adjustedDate(startDate(), option));
  // if the period specified by the parameters and the adjusted period
  // defined for this schedule don't overlap, then the list remains empty
  if ((willEnd() && adjustedDate(m_endDate, option) < _startDate)
      || start_date > endDate)
    return theDates;

  QDate date(adjustedDate(paymentDate, option));

  switch (m_occurrence) {
    case Schedule::Occurrence::Once:
      if (start_date >= _startDate && start_date <= endDate)
        theDates.append(start_date);
      break;

    case Schedule::Occurrence::Daily:
      while (date.isValid() && (date <= endDate)) {
        if (date >= _startDate)
          theDates.append(date);
        paymentDate = paymentDate.addDays(m_occurrenceMultiplier);
        date = adjustedDate(paymentDate, option);
      }
      break;

    case Schedule::Occurrence::Weekly: {
        int step = 7 * m_occurrenceMultiplier;
        while (date.isValid() && (date <= endDate)) {
          if (date >= _startDate)
            theDates.append(date);
          paymentDate = paymentDate.addDays(step);
          date = adjustedDate(paymentDate, option);
        }
      }
      break;

    case Schedule::Occurrence::EveryHalfMonth:
      while (date.isValid() && (date <= endDate)) {
        if (date >= _startDate)
          theDates.append(date);
        paymentDate = addHalfMonths(paymentDate, m_occurrenceMultiplier);
        date = adjustedDate(paymentDate, option);
      }
      break;

    case Schedule::Occurrence::Monthly:
      while (date.isValid() && (date <= endDate)) {
        if (date >= _startDate)
          theDates.append(date);
        paymentDate = paymentDate.addMonths(m_occurrenceMultiplier);
        fixDate(paymentDate);
        date = adjustedDate(paymentDate, option);
      }
      break;

    case Schedule::Occurrence::Yearly:
      while (date.isValid() && (date <= endDate)) {
        if (date >= _startDate)
          theDates.append(date);
        paymentDate = paymentDate.addYears(m_occurrenceMultiplier);
        fixDate(paymentDate);
        date = adjustedDate(paymentDate, option);
      }
      break;

    case Schedule::Occurrence::Any:
    default:
      break;
  }

  return theDates;
}

bool MyMoneySchedule::operator <(const MyMoneySchedule& right) const
{
  return adjustedNextDueDate() < right.adjustedNextDueDate();
}

bool MyMoneySchedule::operator ==(const MyMoneySchedule& right) const
{
  if (MyMoneyObject::operator==(right) &&
      m_occurrence == right.m_occurrence &&
      m_occurrenceMultiplier == right.m_occurrenceMultiplier &&
      m_type == right.m_type &&
      m_startDate == right.m_startDate &&
      m_paymentType == right.m_paymentType &&
      m_fixed == right.m_fixed &&
      m_transaction == right.m_transaction &&
      m_endDate == right.m_endDate &&
      m_autoEnter == right.m_autoEnter &&
      m_lastPayment == right.m_lastPayment &&
      ((m_name.length() == 0 && right.m_name.length() == 0) || (m_name == right.m_name)))
    return true;
  return false;
}

int MyMoneySchedule::transactionsRemaining() const
{
  return transactionsRemainingUntil(adjustedDate(m_endDate, weekendOption()));
}

int MyMoneySchedule::transactionsRemainingUntil(const QDate& endDate) const
{
  int counter = 0;

  QDate startDate = m_lastPayment.isValid() ? m_lastPayment : m_startDate;
  if (startDate.isValid() && endDate.isValid()) {
    QList<QDate> dates = paymentDates(startDate, endDate);
    counter = dates.count();
  }
  return counter;
}

MyMoneyAccount MyMoneySchedule::account(int cnt) const
{
  QList<MyMoneySplit> splits = m_transaction.splits();
  QList<MyMoneySplit>::ConstIterator it;
  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyAccount acc;

  // search the first asset or liability account
  for (it = splits.constBegin(); it != splits.constEnd() && (acc.id().isEmpty() || cnt); ++it) {
    try {
      acc = file->account((*it).accountId());
      if (acc.isAssetLiability())
        --cnt;

      if (!cnt)
        return acc;
    } catch (const MyMoneyException &) {
      qWarning("Schedule '%s' references unknown account '%s'", qPrintable(id()),   qPrintable((*it).accountId()));
      return MyMoneyAccount();
    }
  }

  return MyMoneyAccount();
}

QDate MyMoneySchedule::dateAfter(int transactions) const
{
  int counter = 1;
  QDate paymentDate(startDate());

  if (transactions <= 0)
    return paymentDate;

  switch (m_occurrence) {
    case Schedule::Occurrence::Once:
      break;

    case Schedule::Occurrence::Daily:
      while (counter++ < transactions)
        paymentDate = paymentDate.addDays(m_occurrenceMultiplier);
      break;

    case Schedule::Occurrence::Weekly: {
        int step = 7 * m_occurrenceMultiplier;
        while (counter++ < transactions)
          paymentDate = paymentDate.addDays(step);
      }
      break;

    case Schedule::Occurrence::EveryHalfMonth:
      paymentDate = addHalfMonths(paymentDate, m_occurrenceMultiplier * (transactions - 1));
      break;

    case Schedule::Occurrence::Monthly:
      while (counter++ < transactions)
        paymentDate = paymentDate.addMonths(m_occurrenceMultiplier);
      break;

    case Schedule::Occurrence::Yearly:
      while (counter++ < transactions)
        paymentDate = paymentDate.addYears(m_occurrenceMultiplier);
      break;

    case Schedule::Occurrence::Any:
    default:
      break;
  }

  return paymentDate;
}

bool MyMoneySchedule::isOverdue() const
{
  if (isFinished())
    return false;

  if (adjustedNextDueDate() >= QDate::currentDate())
    return false;

  return true;
}

bool MyMoneySchedule::isFinished() const
{
  if (!m_lastPayment.isValid())
    return false;

  if (m_endDate.isValid()) {
    if (m_lastPayment >= m_endDate
        || !nextDueDate().isValid()
        || nextDueDate() > m_endDate)
      return true;
  }

  // Check to see if its a once off payment
  if (m_occurrence == Schedule::Occurrence::Once)
    return true;

  return false;
}

bool MyMoneySchedule::hasRecordedPayment(const QDate& date) const
{
  // m_lastPayment should always be > recordedPayments()
  if (m_lastPayment.isValid() && m_lastPayment >= date)
    return true;

  if (m_recordedPayments.contains(date))
    return true;

  return false;
}

void MyMoneySchedule::recordPayment(const QDate& date)
{
  m_recordedPayments.append(date);
}

void MyMoneySchedule::setWeekendOption(const Schedule::WeekendOption option)
{
  // make sure only valid values are used. Invalid defaults to MoveNothing.
  switch (option) {
    case Schedule::WeekendOption::MoveBefore:
    case Schedule::WeekendOption::MoveAfter:
      m_weekendOption = option;
      break;

    default:
      m_weekendOption = Schedule::WeekendOption::MoveNothing;
      break;
  }
}

void MyMoneySchedule::fixDate(QDate& date) const
{
  QDate fixDate(m_startDate);
  if (fixDate.isValid()
      && date.day() != fixDate.day()
      && QDate::isValid(date.year(), date.month(), fixDate.day())) {
    date = QDate(date.year(), date.month(), fixDate.day());
  }
}

void MyMoneySchedule::writeXML(QDomDocument& document, QDomElement& parent) const
{
  QDomElement el = document.createElement(nodeNames[nnScheduleTX]);

  writeBaseXML(document, el);

  el.setAttribute(getAttrName(anName), m_name);
  el.setAttribute(getAttrName(anType), (int)m_type);
  el.setAttribute(getAttrName(anOccurence), (int)m_occurrence); // krazy:exclude=spelling
  el.setAttribute(getAttrName(anOccurenceMultiplier), m_occurrenceMultiplier);
  el.setAttribute(getAttrName(anPaymentType), (int)m_paymentType);
  el.setAttribute(getAttrName(anStartDate), dateToString(m_startDate));
  el.setAttribute(getAttrName(anEndDate), dateToString(m_endDate));
  el.setAttribute(getAttrName(anFixed), m_fixed);
  el.setAttribute(getAttrName(anAutoEnter), m_autoEnter);
  el.setAttribute(getAttrName(anLastPayment), dateToString(m_lastPayment));
  el.setAttribute(getAttrName(anWeekendOption), (int)m_weekendOption);

  //store the payment history for this scheduled task.
  QList<QDate> payments = recordedPayments();
  QList<QDate>::ConstIterator it;
  QDomElement paymentsElement = document.createElement(getElName(enPayments));
  for (it = payments.constBegin(); it != payments.constEnd(); ++it) {
    QDomElement paymentEntry = document.createElement(getElName(enPayment));
    paymentEntry.setAttribute(getAttrName(anDate), dateToString(*it));
    paymentsElement.appendChild(paymentEntry);
  }
  el.appendChild(paymentsElement);

  //store the transaction data for this task.
  m_transaction.writeXML(document, el);

  parent.appendChild(el);
}

bool MyMoneySchedule::hasReferenceTo(const QString& id) const
{
  return m_transaction.hasReferenceTo(id);
}

QString MyMoneySchedule::occurrenceToString() const
{
  return occurrenceToString(occurrenceMultiplier(), occurrencePeriod());
}

QString MyMoneySchedule::occurrenceToString(Schedule::Occurrence occurrence)
{
  QString occurrenceString = I18N_NOOP2("Frequency of schedule", "Any");

  if (occurrence == Schedule::Occurrence::Once)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Once");
  else if (occurrence == Schedule::Occurrence::Daily)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Daily");
  else if (occurrence == Schedule::Occurrence::Weekly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Weekly");
  else if (occurrence == Schedule::Occurrence::Fortnightly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Fortnightly");
  else if (occurrence == Schedule::Occurrence::EveryOtherWeek)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every other week");
  else if (occurrence == Schedule::Occurrence::EveryHalfMonth)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every half month");
  else if (occurrence == Schedule::Occurrence::EveryThreeWeeks)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every three weeks");
  else if (occurrence == Schedule::Occurrence::EveryFourWeeks)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every four weeks");
  else if (occurrence == Schedule::Occurrence::EveryThirtyDays)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every thirty days");
  else if (occurrence == Schedule::Occurrence::Monthly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Monthly");
  else if (occurrence == Schedule::Occurrence::EveryEightWeeks)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every eight weeks");
  else if (occurrence == Schedule::Occurrence::EveryOtherMonth)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every two months");
  else if (occurrence == Schedule::Occurrence::EveryThreeMonths)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every three months");
  else if (occurrence == Schedule::Occurrence::Quarterly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Quarterly");
  else if (occurrence == Schedule::Occurrence::EveryFourMonths)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every four months");
  else if (occurrence == Schedule::Occurrence::TwiceYearly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Twice yearly");
  else if (occurrence == Schedule::Occurrence::Yearly)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Yearly");
  else if (occurrence == Schedule::Occurrence::EveryOtherYear)
    occurrenceString = I18N_NOOP2("Frequency of schedule", "Every other year");
  return occurrenceString;
}

QString MyMoneySchedule::occurrenceToString(int mult, Schedule::Occurrence type)
{
  QString occurrenceString = I18N_NOOP2("Frequency of schedule", "Any");

  if (type == Schedule::Occurrence::Once)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Once");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("%1 times").arg(mult));
    }
  else if (type == Schedule::Occurrence::Daily)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Daily");
        break;
      case 30:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every thirty days");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("Every %1 days").arg(mult));
    }
  else if (type == Schedule::Occurrence::Weekly)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Weekly");
        break;
      case 2:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every other week");
        break;
      case 3:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every three weeks");
        break;
      case 4:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every four weeks");
        break;
      case 8:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every eight weeks");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("Every %1 weeks").arg(mult));
    }
  else if (type == Schedule::Occurrence::EveryHalfMonth)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every half month");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("Every %1 half months").arg(mult));
    }
  else if (type == Schedule::Occurrence::Monthly)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Monthly");
        break;
      case 2:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every two months");
        break;
      case 3:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every three months");
        break;
      case 4:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every four months");
        break;
      case 6:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Twice yearly");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("Every %1 months").arg(mult));
    }
  else if (type == Schedule::Occurrence::Yearly)
    switch (mult) {
      case 1:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Yearly");
        break;
      case 2:
        occurrenceString = I18N_NOOP2("Frequency of schedule", "Every other year");
        break;
      default:
        occurrenceString = I18N_NOOP2("Frequency of schedule", QString("Every %1 years").arg(mult));
    }
  return occurrenceString;
}

QString MyMoneySchedule::occurrencePeriodToString(Schedule::Occurrence type)
{
  QString occurrenceString = I18N_NOOP2("Schedule occurrence period", "Any");

  if (type == Schedule::Occurrence::Once)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Once");
  else if (type == Schedule::Occurrence::Daily)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Day");
  else if (type == Schedule::Occurrence::Weekly)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Week");
  else if (type == Schedule::Occurrence::EveryHalfMonth)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Half-month");
  else if (type == Schedule::Occurrence::Monthly)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Month");
  else if (type == Schedule::Occurrence::Yearly)
    occurrenceString = I18N_NOOP2("Schedule occurrence period", "Year");
  return occurrenceString;
}

QString MyMoneySchedule::scheduleTypeToString(Schedule::Type type)
{
  QString text;

  switch (type) {
    case Schedule::Type::Bill:
      text = I18N_NOOP2("Scheduled transaction type", "Bill");
      break;
    case Schedule::Type::Deposit:
      text = I18N_NOOP2("Scheduled transaction type", "Deposit");
      break;
    case Schedule::Type::Transfer:
      text = I18N_NOOP2("Scheduled transaction type", "Transfer");
      break;
    case Schedule::Type::LoanPayment:
      text = I18N_NOOP2("Scheduled transaction type", "Loan payment");
      break;
    case Schedule::Type::Any:
    default:
      text = I18N_NOOP2("Scheduled transaction type", "Unknown");
  }
  return text;
}


QString MyMoneySchedule::paymentMethodToString(Schedule::PaymentType paymentType)
{
  QString text;

  switch (paymentType) {
    case Schedule::PaymentType::DirectDebit:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Direct debit");
      break;
    case Schedule::PaymentType::DirectDeposit:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Direct deposit");
      break;
    case Schedule::PaymentType::ManualDeposit:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Manual deposit");
      break;
    case Schedule::PaymentType::Other:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Other");
      break;
    case Schedule::PaymentType::WriteChecque:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Write check");
      break;
    case Schedule::PaymentType::StandingOrder:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Standing order");
      break;
    case Schedule::PaymentType::BankTransfer:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Bank transfer");
      break;
    case Schedule::PaymentType::Any:
      text = I18N_NOOP2("Scheduled Transaction payment type", "Any (Error)");
      break;
  }
  return text;
}

QString MyMoneySchedule::weekendOptionToString(Schedule::WeekendOption weekendOption)
{
  QString text;

  switch (weekendOption) {
    case Schedule::WeekendOption::MoveBefore:
      text = I18N_NOOP("Change the date to the previous processing day");
      break;
    case Schedule::WeekendOption::MoveAfter:
      text = I18N_NOOP("Change the date to the next processing day");
      break;
    case Schedule::WeekendOption::MoveNothing:
      text = I18N_NOOP("Do Nothing");
      break;
  }
  return text;
}

// until we don't have the means to store the value
// of the variation, we default to 10% in case this
// scheduled transaction is marked 'not fixed'.
//
// ipwizard 2009-04-18

int MyMoneySchedule::variation() const
{
  int rc = 0;
  if (!isFixed()) {
    rc = 10;
#if 0
    QString var = value("kmm-variation");
    if (!var.isEmpty())
      rc = var.toInt();
#endif
  }
  return rc;
}

void MyMoneySchedule::setVariation(int var)
{
  Q_UNUSED(var)
#if 0
  deletePair("kmm-variation");
  if (var != 0)
    setValue("kmm-variation", QString("%1").arg(var));
#endif
}

int MyMoneySchedule::eventsPerYear(Schedule::Occurrence occurrence)
{
  int rc = 0;

  switch (occurrence) {
    case Schedule::Occurrence::Daily:
      rc = 365;
      break;
    case Schedule::Occurrence::Weekly:
      rc = 52;
      break;
    case Schedule::Occurrence::Fortnightly:
      rc = 26;
      break;
    case Schedule::Occurrence::EveryOtherWeek:
      rc = 26;
      break;
    case Schedule::Occurrence::EveryHalfMonth:
      rc = 24;
      break;
    case Schedule::Occurrence::EveryThreeWeeks:
      rc = 17;
      break;
    case Schedule::Occurrence::EveryFourWeeks:
      rc = 13;
      break;
    case Schedule::Occurrence::Monthly:
    case Schedule::Occurrence::EveryThirtyDays:
      rc = 12;
      break;
    case Schedule::Occurrence::EveryEightWeeks:
      rc = 6;
      break;
    case Schedule::Occurrence::EveryOtherMonth:
      rc = 6;
      break;
    case Schedule::Occurrence::EveryThreeMonths:
    case Schedule::Occurrence::Quarterly:
      rc = 4;
      break;
    case Schedule::Occurrence::EveryFourMonths:
      rc = 3;
      break;
    case Schedule::Occurrence::TwiceYearly:
      rc = 2;
      break;
    case Schedule::Occurrence::Yearly:
      rc = 1;
      break;
    default:
      qWarning("Occurrence not supported by financial calculator");
  }

  return rc;
}

int MyMoneySchedule::daysBetweenEvents(Schedule::Occurrence occurrence)
{
  int rc = 0;

  switch (occurrence) {
    case Schedule::Occurrence::Daily:
      rc = 1;
      break;
    case Schedule::Occurrence::Weekly:
      rc = 7;
      break;
    case Schedule::Occurrence::Fortnightly:
      rc = 14;
      break;
    case Schedule::Occurrence::EveryOtherWeek:
      rc = 14;
      break;
    case Schedule::Occurrence::EveryHalfMonth:
      rc = 15;
      break;
    case Schedule::Occurrence::EveryThreeWeeks:
      rc = 21;
      break;
    case Schedule::Occurrence::EveryFourWeeks:
      rc = 28;
      break;
    case Schedule::Occurrence::EveryThirtyDays:
      rc = 30;
      break;
    case Schedule::Occurrence::Monthly:
      rc = 30;
      break;
    case Schedule::Occurrence::EveryEightWeeks:
      rc = 56;
      break;
    case Schedule::Occurrence::EveryOtherMonth:
      rc = 60;
      break;
    case Schedule::Occurrence::EveryThreeMonths:
    case Schedule::Occurrence::Quarterly:
      rc = 90;
      break;
    case Schedule::Occurrence::EveryFourMonths:
      rc = 120;
      break;
    case Schedule::Occurrence::TwiceYearly:
      rc = 180;
      break;
    case Schedule::Occurrence::Yearly:
      rc = 360;
      break;
    default:
      qWarning("Occurrence not supported by financial calculator");
  }

  return rc;
}

QDate MyMoneySchedule::addHalfMonths(QDate date, int mult) const
{
  QDate newdate = date;
  int d, dm;
  if (mult > 0) {
    d = newdate.day();
    if (d <= 12) {
      if (mult % 2 == 0)
        newdate = newdate.addMonths(mult >> 1);
      else
        newdate = newdate.addMonths(mult >> 1).addDays(15);
    } else
      for (int i = 0; i < mult; i++) {
        if (d <= 13)
          newdate = newdate.addDays(15);
        else {
          dm = newdate.daysInMonth();
          if (d == 14)
            newdate = newdate.addDays((dm < 30) ? dm - d : 15);
          else if (d == 15)
            newdate = newdate.addDays(dm - d);
          else if (d == dm)
            newdate = newdate.addDays(15 - d).addMonths(1);
          else
            newdate = newdate.addDays(-15).addMonths(1);
        }
        d = newdate.day();
      }
  } else if (mult < 0)  // Go backwards
    for (int i = 0; i > mult; i--) {
      d = newdate.day();
      dm = newdate.daysInMonth();
      if (d > 15) {
        dm = newdate.daysInMonth();
        newdate = newdate.addDays((d == dm) ? 15 - dm : -15);
      } else if (d <= 13)
        newdate = newdate.addMonths(-1).addDays(15);
      else if (d == 15)
        newdate = newdate.addDays(-15);
      else { // 14
        newdate = newdate.addMonths(-1);
        dm = newdate.daysInMonth();
        newdate = newdate.addDays((dm < 30) ? dm - d : 15);
      }
    }
  return newdate;
}

/**
  * Helper method to convert simple occurrence to compound occurrence + multiplier
  *
  * @param multiplier Returned by reference.  Adjusted multiplier
  * @param occurrence Returned by reference.  Occurrence type
  */
void MyMoneySchedule::simpleToCompoundOccurrence(int& multiplier, Schedule::Occurrence& occurrence)
{
  Schedule::Occurrence newOcc = occurrence;
  int newMulti = 1;
  if (occurrence == Schedule::Occurrence::Once ||
      occurrence == Schedule::Occurrence::Daily ||
      occurrence == Schedule::Occurrence::Weekly ||
      occurrence == Schedule::Occurrence::EveryHalfMonth ||
      occurrence == Schedule::Occurrence::Monthly ||
      occurrence == Schedule::Occurrence::Yearly) { // Already a base occurrence and multiplier
  } else if (occurrence == Schedule::Occurrence::Fortnightly ||
             occurrence == Schedule::Occurrence::EveryOtherWeek) {
    newOcc    = Schedule::Occurrence::Weekly;
    newMulti  = 2;
  } else if (occurrence == Schedule::Occurrence::EveryThreeWeeks) {
    newOcc    = Schedule::Occurrence::Weekly;
    newMulti  = 3;
  } else if (occurrence == Schedule::Occurrence::EveryFourWeeks) {
    newOcc    = Schedule::Occurrence::Weekly;
    newMulti  = 4;
  } else if (occurrence == Schedule::Occurrence::EveryThirtyDays) {
    newOcc    = Schedule::Occurrence::Daily;
    newMulti  = 30;
  } else if (occurrence == Schedule::Occurrence::EveryEightWeeks) {
    newOcc    = Schedule::Occurrence::Weekly;
    newMulti  = 8;
  } else if (occurrence == Schedule::Occurrence::EveryOtherMonth) {
    newOcc    = Schedule::Occurrence::Monthly;
    newMulti  = 2;
  } else if (occurrence == Schedule::Occurrence::EveryThreeMonths ||
             occurrence == Schedule::Occurrence::Quarterly) {
    newOcc    = Schedule::Occurrence::Monthly;
    newMulti  = 3;
  } else if (occurrence == Schedule::Occurrence::EveryFourMonths) {
    newOcc    = Schedule::Occurrence::Monthly;
    newMulti  = 4;
  } else if (occurrence == Schedule::Occurrence::TwiceYearly) {
    newOcc    = Schedule::Occurrence::Monthly;
    newMulti  = 6;
  } else if (occurrence == Schedule::Occurrence::EveryOtherYear) {
    newOcc    = Schedule::Occurrence::Yearly;
    newMulti  = 2;
  } else { // Unknown
    newOcc    = Schedule::Occurrence::Any;
    newMulti  = 1;
  }
  if (newOcc != occurrence) {
    occurrence   = newOcc;
    multiplier  = newMulti == 1 ? multiplier : newMulti * multiplier;
  }
}

/**
  * Helper method to convert compound occurrence + multiplier to simple occurrence
  *
  * @param multiplier Returned by reference.  Adjusted multiplier
  * @param occurrence Returned by reference.  Occurrence type
  */
void MyMoneySchedule::compoundToSimpleOccurrence(int& multiplier, Schedule::Occurrence& occurrence)
{
  Schedule::Occurrence newOcc = occurrence;
  if (occurrence == Schedule::Occurrence::Once) { // Nothing to do
  } else if (occurrence == Schedule::Occurrence::Daily) {
    switch (multiplier) {
      case 1:
        break;
      case 30:
        newOcc = Schedule::Occurrence::EveryThirtyDays;
        break;
    }
  } else if (newOcc == Schedule::Occurrence::Weekly) {
    switch (multiplier) {
      case 1:
        break;
      case 2:
        newOcc = Schedule::Occurrence::EveryOtherWeek;
        break;
      case 3:
        newOcc = Schedule::Occurrence::EveryThreeWeeks;
        break;
      case 4:
        newOcc = Schedule::Occurrence::EveryFourWeeks;
        break;
      case 8:
        newOcc = Schedule::Occurrence::EveryEightWeeks;
        break;
    }
  } else if (occurrence == Schedule::Occurrence::Monthly)
    switch (multiplier) {
      case 1:
        break;
      case 2:
        newOcc = Schedule::Occurrence::EveryOtherMonth;
        break;
      case 3:
        newOcc = Schedule::Occurrence::EveryThreeMonths;
        break;
      case 4:
        newOcc = Schedule::Occurrence::EveryFourMonths;
        break;
      case 6:
        newOcc = Schedule::Occurrence::TwiceYearly;
        break;
    }
  else if (occurrence == Schedule::Occurrence::EveryHalfMonth)
    switch (multiplier) {
      case 1:
        break;
    }
  else if (occurrence == Schedule::Occurrence::Yearly) {
    switch (multiplier) {
      case 1:
        break;
      case 2:
        newOcc = Schedule::Occurrence::EveryOtherYear;
        break;
    }
  }
  if (occurrence != newOcc) { // Changed to derived type
    occurrence = newOcc;
    multiplier = 1;
  }
}

void MyMoneySchedule::setProcessingCalendar(IMyMoneyProcessingCalendar* pc)
{
  processingCalendarPtr = pc;
}

bool MyMoneySchedule::isProcessingDate(const QDate& date) const
{
  if (processingCalendarPtr)
    return processingCalendarPtr->isProcessingDate(date);

  return date.dayOfWeek() < Qt::Saturday;
}

IMyMoneyProcessingCalendar* MyMoneySchedule::processingCalendar() const
{
  return processingCalendarPtr;
}

bool MyMoneySchedule::replaceId(const QString& newId, const QString& oldId)
{
  return m_transaction.replaceId(newId, oldId);
}

const QString MyMoneySchedule::getElName(const elNameE _el)
{
  static const QMap<elNameE, QString> elNames = {
    {enPayment, QStringLiteral("PAYMENT")},
    {enPayments, QStringLiteral("PAYMENTS")}
  };
  return elNames[_el];
}

const QString MyMoneySchedule::getAttrName(const attrNameE _attr)
{
  static const QHash<attrNameE, QString> attrNames = {
    {anName, QStringLiteral("name")},
    {anType, QStringLiteral("type")},
    {anOccurence, QStringLiteral("occurence")},
    {anOccurenceMultiplier, QStringLiteral("occurenceMultiplier")},
    {anPaymentType, QStringLiteral("paymentType")},
    {anFixed, QStringLiteral("fixed")},
    {anAutoEnter, QStringLiteral("autoEnter")},
    {anLastPayment, QStringLiteral("lastPayment")},
    {anWeekendOption, QStringLiteral("weekendOption")},
    {anDate, QStringLiteral("date")},
    {anStartDate, QStringLiteral("startDate")},
    {anEndDate, QStringLiteral("endDate")}
  };
  return attrNames[_attr];
}
