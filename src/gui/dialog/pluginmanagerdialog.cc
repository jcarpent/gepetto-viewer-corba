// Copyright (c) 2015-2018, LAAS-CNRS
// Authors: Joseph Mirabel (joseph.mirabel@laas.fr)
//
// This file is part of gepetto-viewer-corba.
// gepetto-viewer-corba is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// gepetto-viewer-corba is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// gepetto-viewer-corba. If not, see <http://www.gnu.org/licenses/>.

#include "gepetto/gui/dialog/pluginmanagerdialog.hh"
#include "ui_pluginmanagerdialog.h"

#include <QDebug>
#include <QMenu>

#include "gepetto/gui/plugin-interface.hh"
#include "gepetto/gui/mainwindow.hh"

#include <iostream>

namespace gepetto {
  namespace gui {
    QList <QDir> PluginManager::pluginDirs_;

    QIcon PluginManager::icon(const QPluginLoader *pl)
    {
      if (pl->isLoaded()) {
        const PluginInterface* pi = const_instance_cast <PluginInterface> (pl);
        if (pi && pi->isInit ()) {
          return QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
        }
        return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
      }
      return QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
    }

    QString PluginManager::status(const QPluginLoader *pl)
    {
      if (pl->isLoaded()) {
        const PluginInterface* pi = const_instance_cast <PluginInterface> (pl);
        if (pi) {
          if (pi->isInit ())
            return QString ("Plugin loaded correctly");
          else
            return pi->errorMsg ();
        } else
          return QString ("Wrong interface");
      } else
        return pl->errorString();
    }

    void PluginManager::addPluginDir(const QString &path)
    {
      QDir dir (QDir::cleanPath(path));
      QDir can (dir.canonicalPath());
      if (can.exists() && can.isReadable())
	{
	  pluginDirs_.append (can);
	}
    }

    void PluginManager::declareAllPlugins (QWidget *parent)
    {
      foreach (const QDir& dir, pluginDirs_) {
        qDebug() << "Looking for plugins into" << dir.absolutePath();
        QStringList soFiles = dir.entryList(QStringList() << "*.so", QDir::Files);
        foreach (const QString& soFile, soFiles) {
          qDebug() << "Found" << soFile;
          if (!plugins_.contains(soFile)) {
            plugins_[soFile] = new QPluginLoader (dir.absoluteFilePath(soFile), parent);
          }
        }
      }
    }

    bool PluginManager::declarePlugin(const QString &name, QWidget *parent)
    {
      if (!plugins_.contains(name)) {
        QString filename = name;
        if (!QDir::isAbsolutePath(name)) {
            foreach (QDir dir, pluginDirs_) {
                if (dir.exists(name)) {
                    filename = dir.absoluteFilePath(name);
                    break;
                  }
              }
          }
        plugins_[name] = new QPluginLoader (filename, parent);
        return true;
      }
      qDebug () << "Plugin" << name << "already declared.";
      return false;
    }

    bool PluginManager::loadPlugin(const QString &name)
    {
      if (!plugins_.contains(name)) {
        qDebug () << "Plugin" << name << "not declared.";
        return false;
      }
      if (!plugins_[name]->load()) {
        qDebug() << name << ": " << plugins_[name]->errorString();
        return false;
      }
      return true;
    }

    bool PluginManager::initPlugin(const QString &name)
    {
      if (!plugins_.contains(name)) {
        qDebug () << "Plugin" << name << "not declared.";
        return false;
      }
      QPluginLoader* p = plugins_[name];
      if (!p->isLoaded()) {
        qDebug () << "Plugin" << name << "not loaded:" << p->errorString();
        return false;
      }
      PluginInterface* pi = qobject_cast <PluginInterface*> (p->instance());
      if (!pi) {
        qDebug() << name << ": Wrong interface.";
        return false;
      }
      if (!pi->isInit()) pi->doInit();
      return pi->isInit ();
    }

    bool PluginManager::unloadPlugin(const QString &name)
    {
      return plugins_[name]->unload();
    }

    void PluginManager::clearPlugins()
    {
      foreach (QPluginLoader* p, plugins_) {
        p->unload();
      }
    }

    PluginManagerDialog::PluginManagerDialog(PluginManager *pm, QWidget *parent) :
      QDialog(parent),
      ui_(new ::Ui::PluginManagerDialog),
      pm_ (pm)
    {
      ui_->setupUi(this);

      updateList ();

      ui_->pluginList->setColumnHidden(P_FILE, true);

      connect(ui_->pluginList, SIGNAL (currentItemChanged (QTableWidgetItem*,QTableWidgetItem*)),
          SLOT (onItemChanged(QTableWidgetItem*,QTableWidgetItem*)));
      connect(ui_->pluginList, SIGNAL(customContextMenuRequested(QPoint)),
          SLOT(contextMenu(QPoint)));
      connect(ui_->declareAllPluginsButton, SIGNAL(clicked()),
          SLOT(declareAll()));
      connect(ui_->saveButton, SIGNAL(clicked()),
          SLOT(save()));
    }

    PluginManagerDialog::~PluginManagerDialog()
    {
      delete ui_;
    }

    void PluginManagerDialog::onItemChanged(QTableWidgetItem *current,
        QTableWidgetItem* /*previous*/)
    {
      if (!current) return;
      QString key = ui_->pluginList->item(current->row(), P_FILE)->text();
      const QPluginLoader* pl = pm_->plugins()[key];
      ui_->pluginMessage->setText(pm_->status (pl));
    }

    void PluginManagerDialog::contextMenu(const QPoint &pos)
    {
      int row = ui_->pluginList->rowAt(pos.y());
      if (row == -1) return;
      QString key = ui_->pluginList->item(row, P_FILE)->text();
      QMenu contextMenu (tr("Plugin"), ui_->pluginList);
      QSignalMapper sm;
      if (pm_->plugins()[key]->isLoaded()) {
        QAction* unload = contextMenu.addAction("&Unload", &sm, SLOT(map()));
        sm.setMapping (unload, key);
        connect(&sm, SIGNAL (mapped(QString)), this, SLOT(unload(QString)));
        contextMenu.exec(ui_->pluginList->mapToGlobal(pos));
      } else {
        QAction* load = contextMenu.addAction("&Load", &sm, SLOT(map()));
        sm.setMapping (load, key);
        connect(&sm, SIGNAL (mapped(QString)), this, SLOT(load(QString)));
        contextMenu.exec(ui_->pluginList->mapToGlobal(pos));
      }
    }

    void PluginManagerDialog::declareAll()
    {
      pm_->declareAllPlugins();
      updateList();
    }

    void PluginManagerDialog::load(const QString &name)
    {
      pm_->loadPlugin(name);
      pm_->initPlugin(name);
      updateList ();
    }

    void PluginManagerDialog::unload(const QString &name)
    {
      pm_->unloadPlugin (name);
      updateList ();
    }

    void PluginManagerDialog::save ()
    {
      MainWindow::instance()->settings_->writeSettingFile();
    }

    const int PluginManagerDialog::P_NAME = 0;
    const int PluginManagerDialog::P_FILE = 1;
    const int PluginManagerDialog::P_VERSION = 2;
    const int PluginManagerDialog::P_FULLPATH = 3;

    void PluginManagerDialog::updateList()
    {
      while (ui_->pluginList->rowCount() > 0)
        ui_->pluginList->removeRow(0);
      for (PluginManager::Map::const_iterator p = pm_->plugins ().constBegin();
          p != pm_->plugins().constEnd(); p++) {
        QString name = p.key(),
                filename = p.key(),
                fullpath = p.value()->fileName(),
                version = "";
        if (p.value ()->isLoaded ()) {
          PluginInterface* pi = qobject_cast <PluginInterface*> (p.value()->instance());
          name = pi->name();
          // version = pi->version();
        }
        QIcon icon = pm_->icon (p.value());

        ui_->pluginList->insertRow(ui_->pluginList->rowCount());
        ui_->pluginList->setItem(ui_->pluginList->rowCount() - 1, P_NAME, new QTableWidgetItem (icon, name));
        ui_->pluginList->setItem(ui_->pluginList->rowCount() - 1, P_FILE, new QTableWidgetItem (filename));
        ui_->pluginList->setItem(ui_->pluginList->rowCount() - 1, P_VERSION, new QTableWidgetItem (version));
        ui_->pluginList->setItem(ui_->pluginList->rowCount() - 1, P_FULLPATH, new QTableWidgetItem (fullpath));
      }
      ui_->pluginList->resizeColumnsToContents();
    }
  } // namespace gui
} // namespace gepetto
