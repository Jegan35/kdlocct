#ifndef RIGHTPANEL_H
#define RIGHTPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QList>
#include <QElapsedTimer>

// ✅ Correctly include your existing custom widget headers!
#include "RightHeader.h"
#include "MenuTray.h"
#include "ClientBackend.h"
#include "OcctWidget.h"

class RightPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RightPanel(ClientBackend *backend, QWidget *parent = nullptr);
    ~RightPanel() override = default;

    // Getter for MainWindow to access the 3D viewer
    OcctWidget* getDxfPreviewWidget() const { return m_dxfPreviewWidget; }

public slots:
    void setGetPointsEnabled(bool enabled);
    void onTelemetryChanged();
    void onProgramDataChanged();
    void onDirectoryDataChanged();
    void onLocalStateChanged();
    void onHighlightChanged();
    void setActiveTab(int index);
    void updateOriginLabel(double x, double y, double z);

signals:
    void swipeLockToggled(bool locked);
    void maximizedToggled(bool maximized);
    void requestFooterSwipe(); // For MainWindow to catch
    void requestMainLoadStep(const QString& filePath);
    void requestMainClearStep();
    void requestMainSetUserFrame(double x, double y, double z);
protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUI();
    void toggleMaximized();
    void saveUserFramesConfig();
    void loadUserFramesConfig();

    // UI Builders
    QWidget* buildDxfFileWidget();
    QWidget* buildFrameWidget(); // NEW FRAME WIDGET
    QWidget* buildSpeedPanel();
    QWidget* buildJointsPanel();

    void refreshFrameUI(); // Updates the frame list dynamically

    // Legacy placeholders
    void updateIOLeds();
    void updateInstructionTable();
    void updatePrTable();
    void updateTpTable();
    void updateOpPgDisplay();

private:
    ClientBackend *m_backend = nullptr;
    QElapsedTimer m_uiThrottle;
    QString currentMovementMode = "JOG";

    // Layout elements
    QVBoxLayout *m_mainLayout = nullptr;
    RightHeader *header = nullptr;
    QStackedWidget *controlStack = nullptr;
    QTabWidget *m_workspaceTabs = nullptr;
    MenuTray *tray = nullptr;

    // Jog D-Pad Buttons
    QPushButton *btnToggleXYZ = nullptr;
    QPushButton *btnToggleOrient = nullptr;
    QPushButton *btnXPlus = nullptr;
    QPushButton *btnXMinus = nullptr;
    QPushButton *btnYPlus = nullptr;
    QPushButton *btnYMinus = nullptr;
    QPushButton *btnZPlus = nullptr;
    QPushButton *btnZMinus = nullptr;
    QPushButton *m_btnMax = nullptr;

    // DXF Page Items
    OcctWidget *m_dxfPreviewWidget = nullptr;
    QLabel *m_lblOrigin = nullptr;
    QPushButton *m_btnGetPoints = nullptr;
    QTextEdit *m_txtCoordinates = nullptr;

    // ✅ User Frame Structure & Variables
    struct UserFrameData {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };
    QList<UserFrameData> m_userFrames;
    int m_activeFrameIndex = 0;
    bool m_frameDeleteMode = false;
    QVBoxLayout* m_frameListLayout = nullptr;
    QList<QCheckBox*> m_frameCheckboxes;
};

#endif // RIGHTPANEL_H