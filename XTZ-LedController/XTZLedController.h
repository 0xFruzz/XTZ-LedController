#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColorDialog>
#include <QList>

struct LedDevice {
    QString name;
    QString ip;
    QColor currentColor;
};

class XTZLedController : public QWidget {
    Q_OBJECT

public:
    XTZLedController(QWidget* parent = nullptr);
    ~XTZLedController();

private slots:
    void scanNetwork();        
    void setAllColors();         
    void checkStats();           
    void onReplyFinished(QNetworkReply* reply);
    void deviceClicked();       
    void changeMask();

private:
    void createTrayIcon();
    void sendColor(QString ip, QColor color, int brightness = 255);
    void updateMenu();


    QString networkMask = "192.168.31";
    QList<LedDevice> activeStats;
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    QNetworkAccessManager* networkManager;
    QList<LedDevice> foundDevices;
};