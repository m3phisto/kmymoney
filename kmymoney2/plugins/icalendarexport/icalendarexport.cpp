/***************************************************************************
 *   Copyright (C) 2008 by Cristian Onet                                   *
 *   onet.cristian@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "config-kmymoney.h"

// KDE includes
#include <kgenericfactory.h>
#include <kaction.h>
#include <kfiledialog.h>
#include <kplugininfo.h>
#include <kurl.h>
#include <kactioncollection.h>

// KMyMoney includes
#include <kmymoney/mymoneyfile.h>
#include <kmymoney/pluginloader.h>

#include "icalendarexport.h"
#include "schedulestoicalendar.h"
#include "pluginsettings.h"

typedef KGenericFactory<KMMiCalendarExportPlugin> icalendarexportFactory;

K_EXPORT_COMPONENT_FACTORY(icalendarexport, icalendarexportFactory( "kmm_icalendarexport" ))

struct KMMiCalendarExportPlugin::Private {
  KAction* m_action;
  QString  m_profileName;
  QString  m_iCalendarFileEntryName;
  QString  m_name;
  KMMSchedulesToiCalendar m_exporter;
};

KMMiCalendarExportPlugin::KMMiCalendarExportPlugin(QObject *parent, const QStringList&)
    : KMyMoneyPlugin::Plugin(parent, "iCalendar"),
      d(new Private)
{
  d->m_profileName = "iCalendarPlugin";
  d->m_iCalendarFileEntryName = "iCalendarFile";
  d->m_name = "iCalendar";

  // Tell the host application to load my GUI component
  setComponentData(icalendarexportFactory::componentData());
  setXMLFile("kmm_icalendarexport.rc");

  // For ease announce that we have been loaded.
 qDebug("KMyMoney iCalendar plugin loaded");

  // Create the actions of this plugin
  QString actionName = i18n("Schedules to icalendar");
  QString icalFilePath;
  // Note the below code only exists to move existing settings to the new plugin specific config
  KConfigGroup config = KGlobal::config()->group(d->m_profileName);
  icalFilePath = config.readEntry(d->m_iCalendarFileEntryName, icalFilePath);

  // read the settings
  PluginSettings::self()->readConfig();

  if (!icalFilePath.isEmpty()) {
    // move the old setting to the new config
    PluginSettings::setIcalendarFile(icalFilePath);
    PluginSettings::self()->writeConfig();
    KGlobal::config()->deleteGroup(d->m_profileName);
  } else {
    // read it from the new config
    icalFilePath = PluginSettings::icalendarFile();
  }

  if (!icalFilePath.isEmpty())
    actionName = i18n("Schedules to icalendar [%1]").arg(icalFilePath);

  d->m_action = actionCollection()->addAction("file_export_icalendar");
  d->m_action->setText(actionName);

  connect(KMyMoneyPlugin::PluginLoader::instance(), SIGNAL(plug(KPluginInfo*)), this, SLOT(slotPlug(KPluginInfo*)));
  connect(KMyMoneyPlugin::PluginLoader::instance(), SIGNAL(unplug(KPluginInfo*)), this, SLOT(slotUnplug(KPluginInfo*)));
  connect(KMyMoneyPlugin::PluginLoader::instance(), SIGNAL(configChanged(Plugin*)), this, SLOT(slotUpdateConfig()));
}

KMMiCalendarExportPlugin::~KMMiCalendarExportPlugin()
{
  delete d;
}

void KMMiCalendarExportPlugin::slotFirstExport(void)
{
  KFileDialog fileDialog(KUrl(), QString("%1|%2\n").arg("*.ics").arg(i18nc("ICS (Filefilter)", "iCalendar files")), d->m_action->parentWidget());

  fileDialog.setOperationMode( KFileDialog::Saving );
  fileDialog.setCaption(i18n("Export as"));

  if(fileDialog.exec() == QDialog::Accepted) 
  {
    KUrl newURL = fileDialog.selectedUrl();
    if (!newURL.isLocalFile())
      return;

    PluginSettings::setIcalendarFile(newURL.path());

    slotExport();
  }
}

void KMMiCalendarExportPlugin::slotExport(void)
{
  QString icalFilePath = PluginSettings::icalendarFile();
  if (!icalFilePath.isEmpty())
    d->m_exporter.exportToFile(icalFilePath);
}

void KMMiCalendarExportPlugin::slotPlug(KPluginInfo* info)
{
  if (info->name() == d->m_name) {
    connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotExport()));
  }
}

void KMMiCalendarExportPlugin::slotUnplug(KPluginInfo* info)
{
  if (info->name() == d->m_name) {
    disconnect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotExport()));
  }
}

void KMMiCalendarExportPlugin::slotUpdateConfig(void) {
  PluginSettings::self()->readConfig();
  // export the schedules because the configuration has changed
  QString icalFilePath = PluginSettings::icalendarFile();
  if (!icalFilePath.isEmpty())
    d->m_exporter.exportToFile(icalFilePath);
}

#include "icalendarexport.moc"
