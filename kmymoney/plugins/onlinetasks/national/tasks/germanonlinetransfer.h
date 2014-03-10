/*
  This file is part of KMyMoney, A Personal Finance Manager for KDE
  Copyright (C) 2013 Christian Dávid <christian-david@web.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef GERMANONLINETRANSFER_H
#define GERMANONLINETRANSFER_H

#include "onlinetransfer.h"

#include <QtCore/QSharedPointer>

#include "kmm_mymoney_export.h"
#include "germanaccountidentifier.h"
#include "onlinetasks/interfaces/tasks/onlinetask.h"
#include "onlinetasks/interfaces/tasks/credittransfer.h"
#include "../../sepa/credittransfersettingsbase.h"

/**
 * @brief Online Banking national transfer
 *
 * Right now this is a mix of national transfer and national german transfer
 */
class KMM_MYMONEY_EXPORT germanOnlineTransfer : public onlineTask, public creditTransfer
{
  KMM_MYMONEY_UNIT_TESTABLE
  Q_INTERFACES(creditTransfer);
  
public:
  ONLINETASK_META(germanOnlineTransfer, "org.kmymoney.creditTransfer.germany");

  germanOnlineTransfer();
  germanOnlineTransfer(const germanOnlineTransfer &other );

  QString responsibleAccount() const { return _originAccount; };
  void setOriginAccount( const QString& accountId );
  
  MyMoneyMoney value() const { return _value; }
  virtual void setValue(MyMoneyMoney value) { _value = value; }

  void setRecipient ( const germanAccountIdentifier& accountIdentifier ) { _remoteAccount = accountIdentifier; }
  const germanAccountIdentifier& getRecipient() const { return _remoteAccount; }

  virtual void setPurpose( const QString purpose ) { _purpose = purpose; }
  QString purpose() const { return _purpose; }

  virtual QString jobTypeName() const { return creditTransfer::jobTypeName(); }
  
  /**
   * @brief Returns the origin account identifier
   * @return you are owner of the object
   */
  germanAccountIdentifier* originAccountIdentifier() const;

  /**
   * National account can handle the currency of the related account only.
   */
  MyMoneySecurity currency() const;

  bool isValid() const;

  germanOnlineTransfer* clone() const;

  unsigned short int textKey() const { return _textKey; }
  unsigned short int subTextKey() const { return _subTextKey; }

  class settings : public creditTransferSettingsBase
  {
  public:
    lengthStatus checkRecipientAccountNumber( const QString& accountNumber ) const
    {
      const int length = accountNumber.length();
      if (length == 0)
        return tooShort;
      else if ( length > 10 )
        return tooLong;
      return ok;
    }

    lengthStatus checkRecipientBankCode( const QString& bankCode ) const
    {
      const int length = bankCode.length();
      if (length < 8)
        return tooShort;
      else if ( length > 8 )
        return tooLong;
      return ok;
    }
  };

  QSharedPointer<const settings> getSettings() const;
  
  virtual bool hasReferenceTo(const QString& id) const;

protected:
  virtual void writeXML(QDomDocument& document, QDomElement& parent) const;
  virtual germanOnlineTransfer* createFromXml(const QDomElement &element) const;
  
private:
  mutable QSharedPointer<const settings> _settings;
  MyMoneyMoney _value;
  QString _purpose;

  QString _originAccount;
  germanAccountIdentifier _remoteAccount;
  
  unsigned short int _textKey;
  unsigned short int _subTextKey;

};

#endif // GERMANONLINETRANSFER_H
