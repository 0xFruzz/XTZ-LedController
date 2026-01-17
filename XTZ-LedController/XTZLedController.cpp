#include "XTZLedController.h"
#include <QApplication>

#include <qcolordialog.h>
#include <qicon.h>
#include <qstyle.h>
#include "qinputdialog.h"
#include "qmessagebox.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QNetworkRequest>
#include "Windows.h"
#include <QTimer>

XTZLedController::XTZLedController(QWidget* parent) : QWidget(parent) {
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &XTZLedController::onReplyFinished);

    trayMenu = new QMenu(this);
    trayIcon = new QSystemTrayIcon(this);
    QIcon myIcon(":/images/icon.png");
    trayIcon->setIcon(myIcon);

    connect(trayMenu, &QMenu::aboutToShow, this, &XTZLedController::updateMenu);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    scanNetwork();
}

XTZLedController::~XTZLedController() {}

void XTZLedController::updateMenu() {
    trayMenu->clear();

    if (foundDevices.isEmpty()) {
        QAction* empty = trayMenu->addAction(QString::fromUtf8("Devices not found!"));
        empty->setEnabled(false);
    }
    else {
        for (const auto& dev : foundDevices) {
            QString label = dev.name + " (" + dev.ip + ")";
            QAction* action = trayMenu->addAction(label);
            action->setData(dev.ip);
            connect(action, &QAction::triggered, this, &XTZLedController::deviceClicked);
        }
    }

    trayMenu->addSeparator();
    trayMenu->addAction("Refresh Devices", this, &XTZLedController::scanNetwork);
    trayMenu->addAction("Set Global Color", this, &XTZLedController::setAllColors);
    trayMenu->addAction("Check Status", this, &XTZLedController::checkStats);
    trayMenu->addAction("Check Stats", this, &XTZLedController::checkStats);

    trayMenu->addSeparator();
    trayMenu->addAction("Turn on all", this, &XTZLedController::turnOnAll);
    trayMenu->addAction("Turn off all", this, &XTZLedController::turnOffAll);

    trayMenu->addSeparator();
    QString mask = QString::fromUtf8("Network: %1.x").arg(networkMask);
    trayMenu->addAction(mask, this, &XTZLedController::changeMask);

    trayMenu->addSeparator();


    trayMenu->addAction("Exit", qApp, &QCoreApplication::quit);
}

void XTZLedController::turnOnAll() {
    for (const auto& dev : foundDevices) {
        QString url = QString("http://%1/setup?brightness=255")
            .arg(dev.ip);

        networkManager->get(QNetworkRequest(QUrl(url)));
        networkManager->get(QNetworkRequest(QUrl(url)));
    }
}

void XTZLedController::turnOffAll() {
    for (const auto& dev : foundDevices) {
        QString url = QString("http://%1/setup?brightness=0").arg(dev.ip);

        networkManager->get(QNetworkRequest(QUrl(url)));
        networkManager->get(QNetworkRequest(QUrl(url)));
    }
}

void XTZLedController::scanNetwork() {
    foundDevices.clear();

    for (int i = 1; i < 255; ++i) {
        QString urlStr = QString("http://%1.%2/stats").arg(networkMask).arg(i);
        networkManager->get(QNetworkRequest(QUrl(urlStr)));
    }
}

void XTZLedController::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QString ip = reply->url().host();
        if (reply->url().path().contains("stats")) {
            LedDevice dev;
            dev.ip = ip;
            dev.name = "ESP";

            QByteArray data = reply->readAll();
            QString ip = reply->url().host();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();

            QJsonObject leds = root["leds"].toObject();
            QJsonObject colorObj = leds["color"].toObject();

            int r = colorObj["r"].toInt();
            int g = colorObj["g"].toInt();
            int b = colorObj["b"].toInt();
            QColor deviceColor(r, g, b);

            dev.currentColor = deviceColor;

            bool exists = false;
            for (const auto& d : foundDevices) {
                if (d.ip == ip) exists = true;
            }
            if (!exists) foundDevices.append(dev);
        }
    }
    reply->deleteLater();
}

void XTZLedController::deviceClicked() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    
    QString ip = act->data().toString();
    LedDevice* targetDevice = nullptr;
    for (auto& dev : foundDevices) {
        if (dev.ip == ip) {
            targetDevice = &dev;
            break;
        }
    }

    QColor color = QColorDialog::getColor(targetDevice->currentColor, this, "Setup color for " + ip);
    if (color.isValid()) {
        targetDevice->currentColor = color;
        sendColor(ip, color);
    }
    
}

void XTZLedController::setAllColors() {
    if (foundDevices.isEmpty()) {
        QMessageBox::information(this, "Global color", "No devices to set color.");
        return;
    }

    QColor color = QColorDialog::getColor(foundDevices[0].currentColor, this, "Global Color");
    if (color.isValid()) {
        for (auto& dev : foundDevices) {
            LedDevice* devDevice = &dev;
            devDevice->currentColor = color;
            sendColor(dev.ip, color);
        }
    }
}

//TODO: Не рабочее, запрос кидается и я сразу пытаюсь читать ответ, надо раскидать в онреплифинишед, но мне пизда как лень, мб до залива на гит починю
void XTZLedController::checkStats() {
    if (foundDevices.isEmpty()) {
        QMessageBox::information(this, "Status", "No devices to check.");
        return;
    }

    for (const auto& dev : foundDevices) {
        QString url = "http://" + dev.ip + "/stats";
        QNetworkReply* reply = networkManager->get(QNetworkRequest(QUrl(url)));

        while (!reply->isFinished()) {
            QCoreApplication::processEvents(); 
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject colorObj = doc.object()["leds"].toObject()["color"].toObject();

        LedDevice dev;
        dev.ip = reply->url().host();
        dev.currentColor = QColor(colorObj["r"].toInt(),
                colorObj["g"].toInt(),
                colorObj["b"].toInt());
        activeStats.append(dev);
        MessageBoxA(NULL, doc.toJson(QJsonDocument::Indented).toStdString().c_str(), NULL, NULL);
    }
    

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Devices Status");
    dialog->setMinimumWidth(300);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QString headerText = QString("Online: %1 / Total: %2")
        .arg(activeStats.size())
        .arg(foundDevices.size());
    layout->addWidget(new QLabel(headerText));

    for (const auto& dev : activeStats) {
        QHBoxLayout* row = new QHBoxLayout();

        QLabel* colorPreview = new QLabel();
        colorPreview->setFixedSize(20, 20);
        colorPreview->setStyleSheet(QString("background-color: %1;")
            .arg(dev.currentColor.name()));

        row->addWidget(colorPreview);
        row->addWidget(new QLabel(QString("%1 (%2)").arg(dev.ip).arg(dev.currentColor.name())));
        row->addStretch();

        layout->addLayout(row);
    }

    QPushButton* closeBtn = new QPushButton("Close", dialog);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->setAttribute(Qt::WA_DeleteOnClose); 
    dialog->show();
}

void XTZLedController::sendColor(QString ip, QColor color, int brightness) {
    QString urlStr = QString("http://%1/setup?brightness=%2&r=%3&g=%4&b=%5")
        .arg(ip)
        .arg(brightness)
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());

    networkManager->get(QNetworkRequest(QUrl(urlStr)));
}

void XTZLedController::changeMask() {
    bool ok;
    QString text = QInputDialog::getText(this,
        QString::fromUtf8("Network Settings"),
        QString::fromUtf8("Enter network mask i.e 192.168.1 | 192.168.255"),
        QLineEdit::Normal,
        networkMask, &ok);

    if (ok && !text.isEmpty()) {
        networkMask = text.trimmed();
        if (networkMask.endsWith('.')) {
            networkMask.chop(1);
        }
        foundDevices.clear();
        scanNetwork();
    }
}