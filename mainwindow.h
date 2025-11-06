#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QDebug>
#include <QSet>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDateTime>
#include <windows.h>
#include <winspool.h>


class PrintMonitor : public QThread {
    Q_OBJECT
protected:
    void run() override {
        const wchar_t *printerName = L"Foxit PDF Editor Printer"; // ✅ Change to your printer
        HANDLE hPrinter = nullptr;

        if (!OpenPrinterW((LPWSTR)printerName, &hPrinter, nullptr)) {
            qWarning() << "Failed to open printer!";
            return;
        }

        // Request detailed notifications
        WORD jobFields[] = { JOB_NOTIFY_FIELD_STATUS, JOB_NOTIFY_FIELD_DOCUMENT };
        PRINTER_NOTIFY_OPTIONS_TYPE optionsType = {};
        optionsType.Type = JOB_NOTIFY_TYPE;
        optionsType.Reserved0 = 0;
        optionsType.Reserved1 = 0;
        optionsType.Reserved2 = 0;
        optionsType.Count = 2;             // ✅ correct field name
        optionsType.pFields = jobFields;

        PRINTER_NOTIFY_OPTIONS options = {};
        options.Version = 2;
        options.Count = 1;
        options.pTypes = &optionsType;

        HANDLE hChange = FindFirstPrinterChangeNotification(
            hPrinter,
            PRINTER_CHANGE_ADD_JOB | PRINTER_CHANGE_SET_JOB | PRINTER_CHANGE_DELETE_JOB,
            0,
            &options
            );

        if (hChange == INVALID_HANDLE_VALUE) {
            qWarning() << "Invalid printer notification handle!";
            ClosePrinter(hPrinter);
            return;
        }

        qDebug() << "Monitoring printer:" << QString::fromWCharArray(printerName);

        while (!isInterruptionRequested()) {
            DWORD waitStatus = WaitForSingleObject(hChange, 2000); // 2s timeout to allow thread exit
            if (waitStatus == WAIT_TIMEOUT) continue;

            DWORD change = 0;
            PRINTER_NOTIFY_INFO *pInfo = nullptr;
            BOOL ok = FindNextPrinterChangeNotification(hChange, &change, nullptr, (LPVOID*)&pInfo);

            if (!ok) continue;
            qDebug() << "Printer event triggered! change=" << change;

            if (pInfo) {
                for (DWORD i = 0; i < pInfo->Count; ++i) {
                    if (pInfo->aData[i].Field == JOB_NOTIFY_FIELD_STATUS ||
                        pInfo->aData[i].Field == JOB_NOTIFY_FIELD_DOCUMENT) {

                        DWORD jobId = pInfo->aData[i].Id;
                        if (!activeJobs.contains(jobId)) {
                            activeJobs.insert(jobId);
                            qDebug() << "🖨️ New print job detected! Job ID:" << jobId;
                            savePrintJobInfo(hPrinter, jobId);
                        }
                    }
                }
                FreePrinterNotifyInfo(pInfo);
            } else {
                // No info — fallback using EnumJobs()
                logJobsByEnum(hPrinter);
            }

            FindNextPrinterChangeNotification(hChange, nullptr, nullptr, nullptr);
        }

        FindClosePrinterChangeNotification(hChange);
        ClosePrinter(hPrinter);
    }

    void savePrintJobInfo(HANDLE hPrinter, DWORD jobId) {
        DWORD bytesNeeded = 0;
        GetJobW(hPrinter, jobId, 2, nullptr, 0, &bytesNeeded);
        if (!bytesNeeded) return;

        JOB_INFO_2W *pJob = (JOB_INFO_2W*)malloc(bytesNeeded);
        if (!pJob) return;

        if (GetJobW(hPrinter, jobId, 2, (LPBYTE)pJob, bytesNeeded, &bytesNeeded)) {
            QString docName = QString::fromWCharArray(pJob->pDocument);
            QString printer = QString::fromWCharArray(pJob->pPrinterName);
            DWORD pages = pJob->TotalPages;

            QString logText = QString("[%1] Printed: %2 on %3 (%4 pages)\n")
                                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(docName).arg(printer).arg(pages);

            writeLog(logText);
            captureScreen();
        }

        free(pJob);
    }

    void logJobsByEnum(HANDLE hPrinter) {
        DWORD needed = 0, returned = 0;
        EnumJobsW(hPrinter, 0, 10, 2, nullptr, 0, &needed, &returned);
        if (needed == 0) return;

        BYTE *buffer = new BYTE[needed];
        if (EnumJobsW(hPrinter, 0, 10, 2, buffer, needed, &needed, &returned)) {
            JOB_INFO_2W *jobs = reinterpret_cast<JOB_INFO_2W*>(buffer);
            for (DWORD i = 0; i < returned; ++i) {
                DWORD jobId = jobs[i].JobId;
                if (activeJobs.contains(jobId)) continue;

                activeJobs.insert(jobId);
                QString docName = QString::fromWCharArray(jobs[i].pDocument);
                QString printer = QString::fromWCharArray(jobs[i].pPrinterName);
                DWORD pages = jobs[i].TotalPages;

                QString logText = QString("[Enum] %1 Printed: %2 on %3 (%4 pages)\n")
                                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                                      .arg(docName).arg(printer).arg(pages);
                writeLog(logText);
                captureScreen();
            }
        }
        delete[] buffer;
    }

    void writeLog(const QString &text) {
        QDir logDir(QDir::homePath() + "/PrintedLogs");
        if (!logDir.exists()) logDir.mkpath(".");

        QFile file(logDir.filePath("print_log.txt"));
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write(text.toUtf8());
            file.close();
        }
        qDebug() << text.trimmed();
    }

    void captureScreen() {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen) return;

        QPixmap pixmap = screen->grabWindow(0);
        QDir dir(QDir::homePath() + "/PrintedLogs");
        if (!dir.exists()) dir.mkpath(".");

        QString filename = dir.filePath(
            QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png"
            );
        pixmap.save(filename);
        qDebug() << "📸 Screen captured:" << filename;
    }

private:
    QSet<DWORD> activeJobs;
};

// --- Main window ---
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void addToStartup();

private:
    Ui::MainWindow *ui;
    PrintMonitor monitor;
};

#endif // MAINWINDOW_H
