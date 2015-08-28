/*
 *  qRAE - Un cliente del diccionario castellano de la RAE
 *
 *  Copyright (C) 2012-2015 Javier Llorente <javier@opensuse.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtCore/QDebug>
#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtSql/QSqlQuery>

#if QT_VERSION >= 0x050000
#include <QtCore/QStandardPaths>
#include <QtCore/QStringListModel>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>
#include <QtWebKitWidgets/QWebView>
#else
#include <QtGui/QDesktopServices>
#include <QtGui/QStringListModel>
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#include <QtGui/QTextEdit>
#include <QtWebKit/QWebView>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ayudaAbreviaturasYsignos = "qrc:/html/abreviaturas_y_signos_empleados.html";
    ayudaCastellano = "qrc:/html/castellano.html";

    createTrayIcon();
    ui->setupUi(this);

    QLocale castellano(QLocale::Spanish, QLocale::Spain);
    ui->webView->setLocale(castellano);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
            SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));

    connect(ui->webView, SIGNAL(loadProgress(int)), this, SLOT(progresoCarga(int)));
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(resultadoCarga(bool)));
    connect(ui->webView, SIGNAL(customContextMenuRequested(const QPoint&)),this, SLOT(showContextMenu(const QPoint&)));

    createMenuEditarActions();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(errorAlCargar()));

    ui->webView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->webView->show();

    proxySettings = new ProxySettings();
    readSettings();

    m_drae = new DRAE();

    history = new History();
    inicializarAutocompletado();

    connect(ui->lineEditConsultar, SIGNAL(returnPressed()), this, SLOT(consultar()));
    connect(ui->pushButtonConsultar, SIGNAL(clicked(bool)), this, SLOT(consultar()));
}

MainWindow::~MainWindow()
{
    delete proxySettings;
    writeSettings();
    delete ui;
}

void MainWindow::createTrayIcon()
{
    actionRestore = new QAction(tr("&Ocultar"), this);
    connect(actionRestore, SIGNAL(triggered()), this, SLOT(toggleVisibility()));

    actionQuit = new QAction(tr("&Salir"), this);
    actionQuit->setIcon(QIcon(":/iconos/16x16/application-exit.png"));
    connect(actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));

    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(actionRestore);
    trayIconMenu->addAction(actionQuit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/iconos/qrae_72x72.png"));
    trayIcon->setToolTip("Diccionario de la RAE");
    trayIcon->setContextMenu(trayIconMenu);

    trayIcon->show();
}

void MainWindow::createMenuEditarActions()
{
    actionDeshacer = new QAction(ui->menuEditar);
    actionDeshacer->setIcon(QIcon(":/iconos/16x16/edit-undo.png"));
    actionDeshacer->setText("Deshacer");
    actionDeshacer->setShortcut(QKeySequence::Undo);
    ui->menuEditar->addAction(actionDeshacer);
    connect(actionDeshacer, SIGNAL(triggered(bool)), ui->lineEditConsultar, SLOT(undo()));

    actionRehacer = new QAction(ui->menuEditar);
    actionRehacer->setIcon(QIcon(":/iconos/16x16/edit-redo.png"));
    actionRehacer->setText("Rehacer");
    actionRehacer->setShortcut(QKeySequence::Redo);
    ui->menuEditar->addAction(actionRehacer);
    connect(actionRehacer, SIGNAL(triggered(bool)), ui->lineEditConsultar, SLOT(redo()));

    actionCopiar = ui->webView->pageAction((QWebPage::Copy));
    actionCopiar->setIcon(QIcon(":/iconos/16x16/edit-copy.png"));
    actionCopiar->setText("Copiar");
    actionCopiar->setShortcut(QKeySequence::Copy);
    ui->menuEditar->addAction(actionCopiar);

    actionPegar = new QAction(ui->menuEditar);
    actionPegar->setIcon(QIcon(":/iconos/16x16/edit-paste.png"));
    actionPegar->setText("Pegar");
    actionPegar->setShortcut(QKeySequence::Paste);
    ui->menuEditar->addAction(actionPegar);
    connect(actionPegar, SIGNAL(triggered(bool)), ui->lineEditConsultar, SLOT(paste()));

    actionSeleccionarTodo = ui->webView->pageAction((QWebPage::SelectAll));
    actionSeleccionarTodo->setIcon(QIcon(":/iconos/16x16/edit-select-all.png"));
    actionSeleccionarTodo->setText("Seleccionar todo");
    actionSeleccionarTodo->setShortcut(QKeySequence::SelectAll);
    ui->menuEditar->addAction(actionSeleccionarTodo);

    ui->menuEditar->addSeparator();

    actionAjustes = new QAction(ui->menuEditar);
    actionAjustes->setIcon(QIcon(":/iconos/16x16/configure.png"));
    actionAjustes->setText("Ajustes");
    actionAjustes->setShortcut(QKeySequence::Preferences);
    ui->menuEditar->addAction(actionAjustes);
    connect(actionAjustes, SIGNAL(triggered(bool)), this, SLOT(showSettings()));
}

void MainWindow::inicializarAutocompletado()
{
    QStringList stringList = history->get();

    QStringListModel *model = new QStringListModel(stringList);
    completer = new QCompleter(model, this);

    ui->lineEditConsultar->setCompleter(completer);

    connect(ui->lineEditConsultar, SIGNAL(textEdited(const QString&)),
            this, SLOT(actualizarAutocompletado(const QString&)));

    connect(completer, SIGNAL(activated(const QString&)),
            this, SLOT(consultar()));
}

void MainWindow::actualizarAutocompletado(const QString&)
{
    QStringListModel *model = (QStringListModel*)(completer->model());
    QStringList stringList = history->get();
    model->setStringList(stringList);
}

void MainWindow::progresoCarga(int progreso)
{

    statusBar()->showMessage("Cargando...");

    if(progreso!=100 && !timer->isActive()) {

        // El termporizador se activará sólo una vez
        timer->setSingleShot(true);

        // Se inicia con el valor de 30 segundos como tiempo de espera
        // para cargar el resultado de la consulta
        timer->start(30000);

    } else if(progreso==100) {

        timer->stop();
    }

    qDebug() << "progreso: " << progreso;

    if (progreso==100) {

        qDebug() << "(cargado)";
        ui->lineEditConsultar->setText("");

    }
}

void MainWindow::resultadoCarga(bool ok)
{

    statusBar()->showMessage("Cargado", 5000);

    if(!ok) {
        errorAlCargar();
        qDebug() <<  "resultadoCarga(bool): !ok";
    }
}

void MainWindow::errorAlCargar()
{

    ui->webView->setHtml( m_drae->getErrorMsg() );

    qDebug() << "Ha fallado la carga";
}

void MainWindow::consultar()
{
    if (ui->lineEditConsultar->text()!="") {

        ui->webView->load( QUrl( m_drae->consultar( ui->lineEditConsultar->text() ) ));
        history->update(ui->lineEditConsultar->text());

    }
}

void MainWindow::ocultarVentana()
{
    hide();
    actionRestore->setText("&Mostrar");
}

void MainWindow::mostrarVentana()
{
    showNormal();
    actionRestore->setText("&Ocultar");
}

void MainWindow::toggleVisibility()
{
    if (this->isVisible()) {
        ocultarVentana();

    } else {
        mostrarVentana();
    }
}

void MainWindow::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    if (reason==QSystemTrayIcon::Trigger) {
        toggleVisibility();
    }
}

void MainWindow::showContextMenu(const QPoint& position)
{

    QPoint globalPosition = ui->webView->mapToGlobal(position);

    QMenu webViewMenu;

    QAction *actionCopy = ui->webView->pageAction((QWebPage::Copy));
    actionCopy->setIcon(QIcon(":/iconos/16x16/edit-copy.png"));
    actionCopy->setText("Copiar");

    QAction *actionBack = ui->webView->pageAction((QWebPage::Back));
    actionBack->setText(QString::fromUtf8("Atrás"));

    QAction *actionForward = ui->webView->pageAction((QWebPage::Forward));
    actionForward->setText("Adelante");

    webViewMenu.addAction(actionCopy);
    webViewMenu.addAction(actionBack);
    webViewMenu.addAction(actionForward);
    webViewMenu.exec(globalPosition);

}

void MainWindow::on_actionAcerca_de_triggered()
{

    QMessageBox::about(this,"Acerca de qRAE",
                       "<h2 align=\"left\">qRAE</h2>"\
                       "Diccionario castellano de la RAE<br>"\
                       "Versi&oacute;n: " + QString(QRAE_VERSION) +
                       "<div align=\"left\">"
                       "<p>"
                       "&copy; 2013-2015 Javier Llorente <br>"
                       "<a href=\"http://www.javierllorente.com/qrae/\">www.javierllorente.com/qrae/</a><br><br>"
                       "Proyecto Oxygen (iconos de acciones)<br>"
                       "<a href=\"http://www.oxygen-icons.org/\">www.oxygen-icons.org</a><br><br>"
                       "<b>Licencia:</b> <br>"
                       "<nobr>Este programa est&aacute; bajo la GPL v3</nobr>"
                       "</p>"
                       "</div>"
                       );
}

void MainWindow::on_actionSalir_triggered()
{
    qApp->quit();
}

void MainWindow::writeSettings()
{
    qDebug() << "Escribiendo ajustes...";
    QSettings settings("qRAE","Diccionario castellano de la RAE");
    settings.beginGroup("MainWindow");

    settings.setValue("Maximized", isMaximized());
    settings.setValue("Visibility", this->isVisible());

    if (!isMaximized()) {
        settings.setValue("pos", pos());
        settings.setValue("Geometry", saveGeometry());
    }

    settings.endGroup();
}

void MainWindow::readSettings()
{
    qDebug() << "Leyendo ajustes...";
    QSettings settings("qRAE","Diccionario castellano de la RAE");
    settings.beginGroup("MainWindow");

    move(settings.value("pos", QPoint(200, 200)).toPoint());
    restoreGeometry(settings.value("Geometry", saveGeometry()).toByteArray());

    if (settings.value(("Maximized")).toBool()) {
        showMaximized();

    } else if (settings.value("Visibility").toBool()) {
        show();

    } else if (settings.value("Visibility").toString().isNull()) {
        // Valor por defecto
        show();

    } else {
        ocultarVentana();

    }

    settings.endGroup();
}

void MainWindow::showSettings()
{
    Settings *settings = new Settings(this, history, proxySettings);
    if (settings->exec()) {

    }
    delete settings;
}

void MainWindow::on_actionAbreviaturas_y_signos_triggered()
{
    ui->webView->load(QUrl(ayudaAbreviaturasYsignos));
}

void MainWindow::on_actionAlgunos_datos_triggered()
{
    ui->webView->load(QUrl(ayudaCastellano));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    toggleVisibility();
}
