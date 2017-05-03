/*******************************************************************************
*                                 priceswizardpage.h
*                              ------------------
* begin                       : Sat Jan 21 2017
* copyright                   : (C) 2016 by Łukasz Wojniłowicz
* email                       : lukasz.wojnilowicz@gmail.com
********************************************************************************/

/*******************************************************************************
*                                                                              *
*   This program is free software; you can redistribute it and/or modify       *
*   it under the terms of the GNU General Public License as published by       *
*   the Free Software Foundation; either version 2 of the License, or          *
*   (at your option) any later version.                                        *
*                                                                              *
********************************************************************************/

#ifndef PRICESWIZARDPAGE_H
#define PRICESWIZARDPAGE_H

// ----------------------------------------------------------------------------
// QT Includes

#include <QtCore/QFile>
#include <QPointer>
#include <QVBoxLayout>

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include <mymoneystatement.h>
#include <csvimporter.h>

// ----------------------------------------------------------------------------

class PricesProfile;
class SecurityDlg;
class CurrenciesDlg;

namespace Ui
{
class PricesPage;
}

class PricesPage : public CSVWizardPage
{
  Q_OBJECT

public:
  explicit PricesPage(CSVWizard *dlg, CSVImporter *imp);
  ~PricesPage();

  Ui::PricesPage      *ui;

  QVBoxLayout         *m_pageLayout;

private:
  void                initializePage();
  bool                isComplete() const;
  bool                validatePage();

  void                initializeComboBoxes();

  void                resetComboBox(const columnTypeE comboBox);
  /**
  * This method is called column on prices page is selected.
  * It sets m_colTypeNum, m_colNumType and runs column validation.
  */
  bool                validateSelectedColumn(const int col, const columnTypeE type);

  /**
  * This method ensures that there is security for price import.
  */
  bool                validateSecurity();

  /**
  * This method ensures that there are currencies for price import.
  */
  bool                validateCurrencies();

  PricesProfile      *m_profile;

  QPointer<SecurityDlg>    m_securityDlg;
  QPointer<CurrenciesDlg>  m_currenciesDlg;

private slots:
  void                dateColSelected(int col);
  void                priceColSelected(int col);
  void                fractionChanged(int col);
  void                clearColumns();
};

#endif // PRICESWIZARDPAGE_H
