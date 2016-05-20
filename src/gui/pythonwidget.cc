#include "gepetto/gui/pythonwidget.hh"

#include <QFileDialog>
#include <PythonQt/PythonQtClassInfo.h>

#include "gepetto/gui/osgwidget.hh"
#include "gepetto/gui/mainwindow.hh"

namespace gepetto {
    namespace gui {
      namespace {
        void addSignalHandler (PythonQtObjectPtr obj, const QString& callable,
            QObject* sender, const char* signal) {
          PythonQt* pqt = PythonQt::self();
          PythonQtObjectPtr call = pqt->lookupCallable(obj, callable);
          if (call.isNull()) {
            qDebug() << "Callable" << callable << "not found.";
            return;
          }
          if (!pqt->addSignalHandler(sender, signal, call)) {
            qDebug() << "Signal" << signal << "not found in object"
              << sender->objectName();
          } else {
            qDebug() << "Connected"
              << signal << "of" << sender->objectName()
              << "to" << callable;
          }
        }
      }

        PythonWidget::PythonWidget(QWidget *parent) :
            QDockWidget("PythonQt console", parent)
        {
            PythonQt::init(PythonQt::RedirectStdOut);
            PythonQt_QtAll::init();
            mainContext_ = PythonQt::self()->getMainModule();
	    PythonQtObjectPtr sys = PythonQt::self()->importModule ("sys");
	    sys.addVariable ("argv", QVariant(QStringList () << "toto.py"));
            console_ = new PythonQtScriptingConsole(NULL, mainContext_);
            mainContext_.addObject("mainWindow", MainWindow::instance());

            QWidget* widget = new QWidget;
            QVBoxLayout* layout = new QVBoxLayout;
            button_ = new QPushButton;

            button_->setText("Choose file");
            layout->addWidget(console_);
            layout->addWidget(button_);
            widget->setLayout(layout);
            this->setWidget(widget);

            connect(button_, SIGNAL(clicked()), SLOT(browseFile()));
        }

      PythonWidget::~PythonWidget()
      {
	std::cout << "begin cleanup" << std::endl;
        foreach (const PythonQtObjectPtr& m, modules_)
          unloadModulePlugin(m);
	PythonQt::cleanup();
	std::cout << "end cleanup" << std::endl;
      }

        void PythonWidget::browseFile() {
            QFileDialog* fd = new QFileDialog;

            fd->setFileMode(QFileDialog::ExistingFile);
            fd->setNameFilter("All python file (*.py)");
            if (fd->exec() == QDialog::Accepted) {
                QStringList file = fd->selectedFiles();

                mainContext_.evalFile(file.at(0));
            }
            fd->close();
            fd->deleteLater();
        }

        void PythonWidget::loadModulePlugin(QString moduleName) {
          PythonQt* pqt = PythonQt::self();
          PythonQtObjectPtr module = pqt->importModule (moduleName);
          if (module.isNull()) {
            qDebug() << "Enable to load module" << moduleName;
            return;
          }
          module.addObject("mainWindow", MainWindow::instance());
          QString var = "pluginInstance";
          module.evalScript (var + " = Plugin(mainWindow)");
          PythonQtObjectPtr dockPyObj = pqt->lookupObject(module,var);
          PythonQtInstanceWrapper* wrap = (PythonQtInstanceWrapper*) dockPyObj.object();
          if (wrap->classInfo()->className() == "QDockWidget") {
//            This solution would be better, but when deleting this dock widget,
//            the program ends with a SEGV.
//            QDockWidget* dock = (QDockWidget*)wrap->_obj.data();
//            MainWindow::instance()->insertDockWidget(dock, Qt::RightDockWidgetArea);
            module.evalScript ("mainWindow.addDockWidget (1, " + var + ")");
            module.evalScript (var + ".visible = False");
            module.evalScript (var + ".toggleViewAction().setIcon(QtGui.QIcon.fromTheme('window-new'))");
          }
          addSignalHandlersToPlugin(dockPyObj);
          modules_[moduleName] = module;
        }

        void PythonWidget::unloadModulePlugin(QString moduleName) {
          if (modules_.contains(moduleName)) {
            PythonQtObjectPtr module = modules_.value(moduleName);
            unloadModulePlugin(module);
            modules_.remove(moduleName);
          }
        }

        void PythonWidget::unloadModulePlugin(PythonQtObjectPtr module ) {
          PythonQt* pqt = PythonQt::self();
          QString var = "pluginInstance";
          PythonQtObjectPtr dockPyObj = pqt->lookupObject(module,var);
          PythonQtInstanceWrapper* wrap = (PythonQtInstanceWrapper*) dockPyObj.object();
          if (wrap->classInfo()->className() == "QDockWidget") {
//            this generates SEGV
//            QDockWidget* dock = (QDockWidget*)wrap->_obj.data();
//            MainWindow::instance()->removeDockWidget(dock);
            module.evalScript ("mainWindow.removeDockWidget (" + var + ")");
          }
          module.evalScript ("del " + var);
        }

        void PythonWidget::addToContext(QString const& name, QObject* obj) {
            mainContext_.addObject(name, obj);
        }

      void PythonWidget::addSignalHandlersToPlugin(PythonQtObjectPtr plugin)
      {
        MainWindow* main = MainWindow::instance();
        addSignalHandler(plugin, "osgWidget",
            main, SIGNAL(viewCreated(OSGWidget*)));
        QAction* reconnect = main->findChild<QAction*>("actionReconnect");
        if (reconnect)
          addSignalHandler(plugin, "resetConnection",
            reconnect, SIGNAL(triggered()));
        else
          qDebug() << "Could not find actionReconnect button. The plugin will"
            << "not be able to reset CORBA connections";
      }
    }
}
