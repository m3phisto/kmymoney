/***************************************************************************
                          stdtransactionmatched.h
                             -------------------
    begin                : Sat May 31 2008
    copyright            : (C) 2008 by Thomas Baumgart
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

#ifndef STDTRANSACTIONMATCHED_H
#define STDTRANSACTIONMATCHED_H

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "stdtransaction.h"

class MyMoneySplit;
class MyMoneyTransaction;

namespace KMyMoneyRegister
{

  class Register;
  class StdTransactionMatched : public StdTransaction
  {
    static const int m_additionalRows = 3;

  public:
    explicit StdTransactionMatched(Register* getParent, const MyMoneyTransaction& transaction, const MyMoneySplit& split, int uniqueId);
    ~StdTransactionMatched() override;

    const char* className() override;

    bool paintRegisterCellSetup(QPainter *painter, QStyleOptionViewItem &option, const QModelIndex &index) override;

    void registerCellText(QString& txt, Qt::Alignment& align, int row, int col, QPainter* painter = 0) override;

    /**
    * Provided for internal reasons. No API change. See RegisterItem::numRowsRegister(bool)
    */
    int numRowsRegister(bool expanded) const override;

    /**
    * Provided for internal reasons. No API change. See RegisterItem::numRowsRegister()
    */
    int numRowsRegister() const override;
  };

} // namespace

#endif
