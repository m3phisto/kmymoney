/***************************************************************************
                             kinstitutionssview.h
                             -------------------
    copyright            : (C) 2007 by Thomas Baumgart <ipwizard@users.sourceforge.net>
                           (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KINSTITUTIONSVIEW_H
#define KINSTITUTIONSVIEW_H

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "kmymoneyaccountsviewbase.h"

class MyMoneyInstitution;
class MyMoneyMoney;

/**
  * @author Thomas Baumgart
  */
/**
  * This class implements the institutions hierarchical 'view'.
  */
class KInstitutionsViewPrivate;
class KInstitutionsView : public KMyMoneyAccountsViewBase
{
  Q_OBJECT

public:
  explicit KInstitutionsView(QWidget *parent = nullptr);
  ~KInstitutionsView();

  void setDefaultFocus() override;
  void refresh() override;
  void updateActions(const MyMoneyObject &obj) override;

public Q_SLOTS:
  void slotNetWorthChanged(const MyMoneyMoney &);
  void slotShowInstitutionsMenu(const MyMoneyInstitution& inst);
  void slotEditInstitution();

Q_SIGNALS:
  void objectSelected(const MyMoneyObject& obj);

protected:
  void showEvent(QShowEvent * event) override;

private:
  Q_DECLARE_PRIVATE(KInstitutionsView)

private Q_SLOTS:
  void slotNewInstitution();
  void slotDeleteInstitution();
};

#endif
