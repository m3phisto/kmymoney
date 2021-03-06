/***************************************************************************
                            csvimporter.cpp
                             -------------------
    begin                : Sat Jan 01 2010
    copyright            : (C) 2010 by Allan Anderson
    email                : agander93@gmail.com
    copyright            : (C) 2016 by Łukasz Wojniłowicz
    email                : lukasz.wojnilowicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "csvimporter.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QFile>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KPluginFactory>
#include <KActionCollection>
#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "core/csvimportercore.h"
#include "csvwizard.h"
#include "statementinterface.h"
#include "kmymoneyglobalsettings.h"

CSVImporter::CSVImporter(QObject *parent, const QVariantList &args) :
    KMyMoneyPlugin::Plugin(parent, "csvimporter"/*must be the same as X-KDE-PluginInfo-Name*/)
{
  Q_UNUSED(args);
  setComponentName("csvimporter", i18n("CSV importer"));
  setXMLFile("csvimporter.rc");
  createActions();
  // For information, announce that we have been loaded.
  qDebug("Plugins: csvimporter loaded");
}

CSVImporter::~CSVImporter()
{
  qDebug("Plugins: csvimporter unloaded");
}

void CSVImporter::injectExternalSettings(KMyMoneySettings* p)
{
  KMyMoneyGlobalSettings::injectExternalSettings(p);
}

void CSVImporter::createActions()
{
  m_action = actionCollection()->addAction("file_import_csv");
  m_action->setText(i18n("CSV..."));
  connect(m_action, &QAction::triggered, this, &CSVImporter::startWizardRun);
}

void CSVImporter::startWizardRun()
{
  m_action->setEnabled(false);
  m_importer = new CSVImporterCore;
  m_wizard = new CSVWizard(this, m_importer);
  m_silent = false;
  connect(m_wizard, &CSVWizard::statementReady, this, &CSVImporter::slotGetStatement);
  m_action->setEnabled(false);//  don't allow further plugins to start while this is open
}

bool CSVImporter::slotGetStatement(MyMoneyStatement& s)
{
  bool ret = statementInterface()->import(s, m_silent);
  delete m_importer;
  return ret;
}

QString CSVImporter::formatName() const
{
  return QLatin1String("CSV");
}

QString CSVImporter::formatFilenameFilter() const
{
  return "*.csv";
}

bool CSVImporter::isMyFormat(const QString& filename) const
{
  // filename is considered a CSV file if it can be opened
  // and the filename ends in ".csv".
  bool result = false;

  QFile f(filename);
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    result = f.fileName().endsWith(QLatin1String(".csv"));
    f.close();
  }

  return result;
}

bool CSVImporter::import(const QString& filename)
{
  bool rc = true;
  m_importer = new CSVImporterCore;
  m_wizard = new CSVWizard(this, m_importer);
  m_wizard->presetFilename(filename);
  m_silent = false;
  connect(m_wizard, &CSVWizard::statementReady, this, &CSVImporter::slotGetStatement);
  return rc;
}

QString CSVImporter::lastError() const
{
  return QString();
}

K_PLUGIN_FACTORY_WITH_JSON(CSVImporterFactory, "csvimporter.json", registerPlugin<CSVImporter>();)

#include "csvimporter.moc"
