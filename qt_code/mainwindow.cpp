#include "mainwindow.h"
#include <QApplication>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QTextStream>
#include <QTableWidgetItem>
#include <numeric>
#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), darkMode(false)
{
    serial = new QSerialPort(this);
    setupUI();

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

    // 🔌 Configuration du port série (adaptée à ton Arduino)
    serial->setPortName("COM4"); // Change pour ton port (ex. /dev/ttyUSB0)
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Erreur", "Impossible d'ouvrir le port série.");
    }
}

MainWindow::~MainWindow() {
    if (serial->isOpen())
        serial->close();
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // LCD
    lcdTemperature = new QLCDNumber;
    lcdTemperature->setDigitCount(5);
    lcdTemperature->setSegmentStyle(QLCDNumber::Flat);
    mainLayout->addWidget(lcdTemperature);

    // État
    statusLabel = new QLabel("État: Normal");
    mainLayout->addWidget(statusLabel);

    // Statistiques
    minLabel = new QLabel("Min: --");
    maxLabel = new QLabel("Max: --");
    avgLabel = new QLabel("Moyenne: --");
    mainLayout->addWidget(minLabel);
    mainLayout->addWidget(maxLabel);
    mainLayout->addWidget(avgLabel);

    // 📋 Table historique
    logTable = new QTableWidget;
    logTable->setColumnCount(2);
    logTable->setHorizontalHeaderLabels({"Heure", "Température"});
    mainLayout->addWidget(logTable);

    // Boutons
    QPushButton *themeButton = new QPushButton("🌙 Mode sombre");
    connect(themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    QPushButton *exportButton = new QPushButton("💾 Exporter CSV");
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportCSV);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(themeButton);
    buttonLayout->addWidget(exportButton);
    mainLayout->addLayout(buttonLayout);

    setCentralWidget(central);
}

void MainWindow::readSerialData()
{
    static QByteArray buffer;
    buffer += serial->readAll();

    // Si un retour à la ligne est trouvé
    if (buffer.contains('\n')) {
        QList<QByteArray> lines = buffer.split('\n');
        for (int i = 0; i < lines.size() - 1; ++i) {
            QString line = QString::fromUtf8(lines[i]).trimmed();

            // Debug : vérifier la chaîne lue
            qDebug() << "Données reçues avant conversion : " << line;

            bool ok;
            float temperature = line.toFloat(&ok);

            // Si la conversion a réussi
            if (ok) {
                qDebug() << "Température lue : " << temperature;
                lcdTemperature->display(temperature); // Afficher la température sur l'écran LCD
                updateStatus(temperature); // Mettre à jour le statut
                logTemperature(temperature); // Ajouter dans l'historique
            } else {
                qDebug() << "Erreur de conversion de la température.";
            }
        }
        buffer = lines.last(); // Garder la dernière partie du buffer
    }
}



void MainWindow::updateStatus(float temperature)
{
    if (temperature > 48.44f) {
        statusLabel->setText("⚠️ État: Surchauffe !");
        showOverheatNotification(temperature);
    } else {
        statusLabel->setText("✅ État: Normal");
    }

    temperatureHistory.append(temperature);
    float min = *std::min_element(temperatureHistory.begin(), temperatureHistory.end());
    float max = *std::max_element(temperatureHistory.begin(), temperatureHistory.end());
    float avg = std::accumulate(temperatureHistory.begin(), temperatureHistory.end(), 0.0f) / temperatureHistory.size();

    minLabel->setText(QString("Min: %1°C").arg(min, 0, 'f', 1));
    maxLabel->setText(QString("Max: %1°C").arg(max, 0, 'f', 1));
    avgLabel->setText(QString("Moyenne: %1°C").arg(avg, 0, 'f', 1));
}

void MainWindow::logTemperature(float temperature)
{
    int row = logTable->rowCount();
    logTable->insertRow(row);
    logTable->setItem(row, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss")));
    logTable->setItem(row, 1, new QTableWidgetItem(QString::number(temperature, 'f', 1)));
}

void MainWindow::showOverheatNotification(float temperature)
{
    QMessageBox::warning(this, "⚠️ Alerte", QString("Température élevée : %1°C").arg(temperature, 0, 'f', 1));
}

void MainWindow::toggleTheme()
{
    darkMode = !darkMode;
    applyTheme(darkMode);  // Correction de l'appel de la méthode applyTheme avec l'argument nécessaire
}

void MainWindow::applyTheme(bool dark)
{
    qApp->setStyleSheet(dark ?
                            "QMainWindow { background-color: #121212; color: white; }"
                            "QLabel, QPushButton { color: white; }"
                            "QTableWidget { background-color: #1E1E1E; color: white; }"
                             : "");
}

void MainWindow::exportCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Exporter en CSV", "", "Fichiers CSV (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream out(&file);
        out << "Heure,Température\n";
        for (int i = 0; i < logTable->rowCount(); ++i) {
            out << logTable->item(i, 0)->text() << "," << logTable->item(i, 1)->text() << "\n";
        }
        file.close();
    }
}
