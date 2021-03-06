/***************************************************************************
                         kofxdirectconnectdlg.h
                             -------------------
    begin                : Sat Nov 13 2004
    copyright            : (C) 2002 by Ace Jones
    email                : acejones@users.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOFXDIRECTCONNECTDLG_H
#define KOFXDIRECTCONNECTDLG_H

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

class QTemporaryFile;
class KJob;

namespace KIO
{
class Job;
class TransferJob;
}

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyofxconnector.h"
#include "ui_kofxdirectconnectdlgdecl.h"

/**
@author ace jones
*/

class KOfxDirectConnectDlgDecl : public QDialog, public Ui::KOfxDirectConnectDlgDecl
{
public:
  explicit KOfxDirectConnectDlgDecl(QWidget *parent) : QDialog(parent) {
    setupUi(this);
  }
};

class KOfxDirectConnectDlg : public KOfxDirectConnectDlgDecl
{
  Q_OBJECT
public:
  explicit KOfxDirectConnectDlg(const MyMoneyAccount&, QWidget *parent = 0);
  ~KOfxDirectConnectDlg();

  /**
    * Initializes the download of OFX statement data.
    *
    * @returns true if download was initialized
    * @returns false if download was not started
    */
  bool init();

Q_SIGNALS:
  /**
    * This signal is emitted when the statement is downloaded
    * and stored in file @a fname.
    */
  void statementReady(const QString& fname);

protected Q_SLOTS:
  void slotOfxFinished(KJob*);
  void slotOfxData(KIO::Job*, const QByteArray&);
  virtual void reject();

protected:
  void setStatus(const QString& _status);
  void setDetails(const QString& _details);

private:
  /// \internal d-pointer class.
  class Private;
  /// \internal d-pointer instance.
  Private* const d;

  QTemporaryFile* m_tmpfile;
  MyMoneyOfxConnector m_connector;
  KIO::TransferJob* m_job;
};

#endif // KOFXDIRECTCONNECTDLG_H
