#include "RightPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QFrame>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QApplication>
#include <QTimer>
#include <QFileDialog>
#include <QSettings>

// ============================================================
//  STYLE CONSTANTS
// ============================================================
static const QString S_COMBO =
    "QComboBox { background:#1a1e2a; color:#e0e0e0; border:1px solid #2a2d35; border-radius:3px; "
    "padding:3px 6px; font-size:11px; font-weight:bold; min-height:26px; }"
    "QComboBox::drop-down { border:none; }"
    "QComboBox QAbstractItemView { background:#1a1e2a; color:#e0e0e0; "
    "selection-background-color:#00bcd4; selection-color:#000; }";

static const QString S_INPUT =
    "QLineEdit { background:#1a1e2a; color:#e0e0e0; border:1px solid #2a2d35; border-radius:3px; "
    "padding:3px 8px; font-size:11px; min-height:26px; }"
    "QLineEdit:focus { border-color:#00bcd4; }";

static const QString S_MENU_STYLE =
    "QMenu { background:#1a1e2a; color:#e0e0e0; border:1px solid #2a2d35; }"
    "QMenu::item { padding:8px 20px; }"
    "QMenu::item:selected { background:#00bcd4; color:#000; }";

static QString sBtnColor(const QString &top, const QString &bot, const QString &edge) {
    return QString(
               "QPushButton { background:qlineargradient(y1:0,y2:1,stop:0 %1,stop:1 %2); "
               "color:white; font-weight:bold; border-radius:4px; border-bottom:3px solid %3; "
               "padding:0 6px; font-size:11px; min-height:28px; }"
               "QPushButton:pressed { margin-top:2px; border-bottom:1px solid %3; }"
               ).arg(top, bot, edge);
}
static const QString S_BTN_GREEN  = sBtnColor("#2E7D32","#1b5e20","#103a14");
static const QString S_BTN_RED    = sBtnColor("#c62828","#b71c1c","#7f0000");

// ============================================================
//  CONSTRUCTOR
// ============================================================
RightPanel::RightPanel(ClientBackend *backend, QWidget *parent)
    : QWidget(parent), m_backend(backend)
{
    m_uiThrottle.start();

    // Default User Frame setup
    m_userFrames.append(UserFrameData{0.0, -800.0, 600.0});
    m_activeFrameIndex = 0;
    m_frameDeleteMode = false;
    loadUserFramesConfig();
    setupUI();

    if (m_backend) {
        connect(m_backend, &ClientBackend::telemetryChanged,
                this, &RightPanel::onTelemetryChanged);
    }

    qApp->installEventFilter(this);
}

// ============================================================
//  setupUI
// ============================================================
void RightPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0,0,0,0);
    m_mainLayout->setSpacing(0);

    header = new RightHeader(m_backend, this);
    header->setStyleSheet("background-color:#1E1E24; border-bottom:2px solid #3E3E42;");

    controlStack = new QStackedWidget(this);
    controlStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // --- Cartesian D-Pad page ---
    QWidget *cartWidget = new QWidget(this);
    cartWidget->setStyleSheet("background-color:#151822;");
    QVBoxLayout *dpadLay = new QVBoxLayout(cartWidget);
    dpadLay->setContentsMargins(20,5,20,5);
    dpadLay->setSpacing(10);

    QFrame *pillFrame = new QFrame(this);
    pillFrame->setStyleSheet("QFrame{background:#1E1E24;border:1px solid #3E3E42;border-radius:18px;}");
    pillFrame->setFixedSize(180,36);
    QHBoxLayout *pillLay = new QHBoxLayout(pillFrame);
    pillLay->setContentsMargins(2,2,2,2); pillLay->setSpacing(0);
    btnToggleXYZ    = new QPushButton("CART", pillFrame);
    btnToggleOrient = new QPushButton("JNT",  pillFrame);
    const QString XYZ_ACT = "QPushButton{color:black;font-weight:bold;border:none;border-radius:14px;background:#00E5FF;}";
    const QString ORI_ACT = "QPushButton{color:white;font-weight:bold;border:none;border-radius:14px;background:#22C55E;}";
    const QString INACT   = "QPushButton{color:#888;font-weight:bold;border:none;border-radius:14px;background:transparent;}";
    btnToggleXYZ->setStyleSheet(XYZ_ACT);
    btnToggleOrient->setStyleSheet(INACT);
    pillLay->addWidget(btnToggleXYZ); pillLay->addWidget(btnToggleOrient);
    QHBoxLayout *pillRow = new QHBoxLayout(); pillRow->addWidget(pillFrame); pillRow->addStretch();
    dpadLay->addLayout(pillRow);

    const QString B_RED  = "QPushButton{background:#EF4444;color:white;font-weight:bold;font-size:20px;border-radius:8px;border-bottom:5px solid #B91C1C;min-width:60px;min-height:60px;}QPushButton:pressed{margin-top:5px;border-bottom:0;}";
    const QString B_GRN  = "QPushButton{background:#4ADE80;color:white;font-weight:bold;font-size:20px;border-radius:8px;border-bottom:5px solid #16A34A;min-width:60px;min-height:60px;}QPushButton:pressed{margin-top:5px;border-bottom:0;}";
    const QString B_BLU  = "QPushButton{background:#0EA5E9;color:white;font-weight:bold;font-size:20px;border-radius:8px;border-bottom:5px solid #0369A1;min-width:60px;min-height:60px;}QPushButton:pressed{margin-top:5px;border-bottom:0;}";
    const QString B_HOME = "QPushButton{background:#0284C7;color:white;font-weight:bold;font-size:13px;border-radius:30px;border-bottom:5px solid #075985;min-width:60px;min-height:60px;max-width:60px;max-height:60px;}QPushButton:pressed{margin-top:5px;border-bottom:0;}";

    btnYPlus  = new QPushButton("Y+",this); btnYPlus->setStyleSheet(B_GRN);
    btnYMinus = new QPushButton("Y-",this); btnYMinus->setStyleSheet(B_GRN);
    btnXPlus  = new QPushButton("X+",this); btnXPlus->setStyleSheet(B_RED);
    btnXMinus = new QPushButton("X-",this); btnXMinus->setStyleSheet(B_RED);
    btnZPlus  = new QPushButton("Z+",this); btnZPlus->setStyleSheet(B_BLU);
    btnZMinus = new QPushButton("Z-",this); btnZMinus->setStyleSheet(B_BLU);
    QPushButton *btnHome = new QPushButton("HOME",this); btnHome->setStyleSheet(B_HOME);

    auto connectJog = [this](QPushButton *b){
        connect(b, &QPushButton::pressed,  this, [this, b]{ if (m_backend) m_backend->handleButtonPress(b->text()); });
        connect(b, &QPushButton::released, this, [this, b]{ if (m_backend) m_backend->handleButtonRelease(b->text()); });
    };
    connectJog(btnXPlus); connectJog(btnXMinus);
    connectJog(btnYPlus); connectJog(btnYMinus);
    connectJog(btnZPlus); connectJog(btnZMinus);

    QGridLayout *jogGrid = new QGridLayout();
    jogGrid->setSpacing(10);
    jogGrid->setAlignment(Qt::AlignCenter);
    jogGrid->addWidget(btnYPlus,  0,1); jogGrid->addWidget(btnXMinus,1,0);
    jogGrid->addWidget(btnHome,   1,1); jogGrid->addWidget(btnXPlus, 1,2);
    jogGrid->addWidget(btnYMinus, 2,1); jogGrid->addWidget(btnZPlus, 0,3);
    jogGrid->addWidget(btnZMinus, 2,3);
    dpadLay->addLayout(jogGrid);
    dpadLay->addStretch();

    controlStack->addWidget(cartWidget);
    controlStack->addWidget(buildSpeedPanel());
    controlStack->addWidget(buildJointsPanel());

    // --- Workspace Tabs ---
    m_workspaceTabs = new QTabWidget(this);
    m_workspaceTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_workspaceTabs->setStyleSheet(
        "QTabWidget::pane{border-top:3px solid #00E5FF;background:#151822;}"
        "QTabBar::tab{background:#1E1E24;color:#9CA3AF;padding:7px 13px;font-weight:bold;"
        "font-size:11px;text-transform:uppercase;border:none;}"
        "QTabBar::tab:selected{color:#00E5FF;border-bottom:3px solid #00E5FF;background:#151822;}"
        "QTabBar::tab:hover{color:#fff;}");

    m_btnMax = new QPushButton("⛶ MAX", this);
    m_btnMax->setStyleSheet("color:#00E5FF;background:transparent;font-weight:bold;"
                            "font-size:13px;border:none;padding:0 14px;");
    m_btnMax->setCursor(Qt::PointingHandCursor);
    m_btnMax->setFocusPolicy(Qt::NoFocus);
    m_workspaceTabs->setCornerWidget(m_btnMax, Qt::TopRightCorner);
    connect(m_btnMax, &QPushButton::clicked, this, &RightPanel::toggleMaximized);

    m_workspaceTabs->addTab(buildDxfFileWidget(), "DXF FILE");
    m_workspaceTabs->addTab(buildFrameWidget(), "FRAME"); // ✅ NEW FRAME TAB

    m_mainLayout->addWidget(header,         0);
    m_mainLayout->addWidget(controlStack,   0);
    m_mainLayout->addWidget(m_workspaceTabs,1);

    tray = new MenuTray(m_backend, this);
    tray->hide();

    connect(header, &RightHeader::menuClicked, this, [this]{
        tray->toggleTray(this->width(), this->height());
    });
    connect(header, &RightHeader::swipeLockChanged, this, [this](bool u){
        emit swipeLockToggled(u);
    });

    connect(tray, &MenuTray::modeSelected, this, [this, XYZ_ACT, ORI_ACT, INACT](const QString &mode){
        if      (mode == "SPEED")     { header->updateStatusText("SETTINGS | GLOBAL SPEED",  false); controlStack->setCurrentIndex(1); }
        else if (mode == "JOG_CART")  { currentMovementMode = "JOG";  header->updateStatusText("MANUAL | JOG : CARTESIAN", false); controlStack->setCurrentIndex(0); }
        else if (mode == "JOG_JNT")   { currentMovementMode = "JOG";  header->updateStatusText("MANUAL | JOG : JOINTS",    false); controlStack->setCurrentIndex(2); }
        else if (mode == "MOVE_CART") { currentMovementMode = "MOVE"; header->updateStatusText("MANUAL | MOVE : CARTESIAN",false); controlStack->setCurrentIndex(0); }
        else if (mode == "MOVE_JNT")  { currentMovementMode = "MOVE"; header->updateStatusText("MANUAL | MOVE : JOINTS",   false); controlStack->setCurrentIndex(2); }
        else if (mode == "AUTO")      { header->updateStatusText("AUTO | RUNNING", true); }
        else                          { header->updateStatusText(mode + " | STANDBY", false); }
        tray->toggleTray(this->width(), this->height());
    });

    connect(btnToggleXYZ, &QPushButton::clicked, this, [=]{
        btnToggleXYZ->setStyleSheet(XYZ_ACT); btnToggleOrient->setStyleSheet(INACT);
        header->updateStatusText("MANUAL | " + currentMovementMode + " : CARTESIAN", false);
        btnXPlus->setText("X+"); btnXMinus->setText("X-");
        btnYPlus->setText("Y+"); btnYMinus->setText("Y-");
        btnZPlus->setText("Z+"); btnZMinus->setText("Z-");
    });
    connect(btnToggleOrient, &QPushButton::clicked, this, [=]{
        btnToggleOrient->setStyleSheet(ORI_ACT); btnToggleXYZ->setStyleSheet(INACT);
        header->updateStatusText("MANUAL | " + currentMovementMode + " : JOINT (RxRyRz)", false);
        btnXPlus->setText("Rx+"); btnXMinus->setText("Rx-");
        btnYPlus->setText("Ry+"); btnYMinus->setText("Ry-");
        btnZPlus->setText("Rz+"); btnZMinus->setText("Rz-");
    });
}

// ============================================================
//  toggleMaximized
// ============================================================
void RightPanel::toggleMaximized()
{
    this->setUpdatesEnabled(false);
    if (tray && !tray->isHidden()) tray->hide();

    static bool isMaximized = false;
    isMaximized = !isMaximized;

    if (isMaximized) {
        // --- MAX MODE ---
        m_btnMax->setText("🗗 MIN");
        if (header) header->hide();
        if (controlStack) controlStack->hide();

        // DXF/FRAME Tabs-க்கு 100% இடம் கொடுக்கிறோம்
        m_mainLayout->setStretch(0, 0); // Header
        m_mainLayout->setStretch(1, 0); // Jog Controls
        m_mainLayout->setStretch(2, 1); // Tabs (Full Space)
    } else {
        // --- MIN MODE ---
        m_btnMax->setText("⛶ MAX");
        if (header) header->show();
        if (controlStack) controlStack->show();

        // ✅ THE FIX: Jog Controls-க்கு "Stretch = 1" கொடுக்கிறோம்.
        // இதனால் அது Tabs-ஐக் கீழே தள்ளி, OcctView-ன் உயரத்தைக் குறைத்துவிடும்!
        m_mainLayout->setStretch(0, 0); // Header
        m_mainLayout->setStretch(1, 1); // Jog Controls (Takes 50% space)
        m_mainLayout->setStretch(2, 1); // Tabs (Takes 50% space)
    }

    this->setUpdatesEnabled(true);
}
// ============================================================
//  buildDxfFileWidget
// ============================================================
QWidget* RightPanel::buildDxfFileWidget()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background:#0d1117;");
    QHBoxLayout *dxfLayout = new QHBoxLayout(w);
    dxfLayout->setContentsMargins(15,15,15,10); dxfLayout->setSpacing(20);

    QWidget *dxfViewArea = new QWidget();
    dxfViewArea->setObjectName("DxfViewArea");
    dxfViewArea->setStyleSheet("background-color:#0a0d14; border:1px solid #1e2330; border-radius:5px;");
    dxfViewArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *viewLayout = new QVBoxLayout(dxfViewArea);
    viewLayout->setContentsMargins(4,4,4,4); viewLayout->setSpacing(8);

    m_dxfPreviewWidget = new OcctWidget(this);
    m_dxfPreviewWidget->setViewRole(OcctWidget::SideRole);
    m_dxfPreviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_dxfPreviewWidget->setMinimumSize(50,50);

    QWidget *originContainer = new QWidget();
    originContainer->setStyleSheet("background:transparent; border:none;");
    originContainer->setFixedHeight(40);
    QHBoxLayout *originLay = new QHBoxLayout(originContainer);
    originLay->setContentsMargins(0,0,0,0); originLay->setSpacing(10);

    m_lblOrigin = new QLabel("Part Offset -> X: 0.000 | Y: -800.000 | Z: 600.000");
    m_lblOrigin->setStyleSheet("color:#F59E0B; font-weight:bold; font-size:11px; background:#141820; border:1px solid #3A4460; border-radius:4px; padding:6px;");
    m_lblOrigin->setAlignment(Qt::AlignCenter);

    QLabel *lblFileOrigin = new QLabel("3D File Origin -> X: 0.000 | Y: 0.000 | Z: 0.000");
    lblFileOrigin->setStyleSheet("color:#00E5FF; font-weight:bold; font-size:11px; background:#141820; border:1px solid #00838F; border-radius:4px; padding:6px;");
    lblFileOrigin->setAlignment(Qt::AlignCenter);

    originLay->addWidget(m_lblOrigin, 1); originLay->addWidget(lblFileOrigin, 1);
    viewLayout->addWidget(m_dxfPreviewWidget, 1);
    viewLayout->addWidget(originContainer, 0);

    // ---- Right control panel ----
    QWidget *dxfControlArea = new QWidget();
    dxfControlArea->setStyleSheet("background:transparent; border:none;");
    QVBoxLayout *ctrlLayout = new QVBoxLayout(dxfControlArea);
    ctrlLayout->setContentsMargins(0,0,0,0); ctrlLayout->setSpacing(12);

    QLabel *lblMode = new QLabel("SELECTION MODE:");
    lblMode->setStyleSheet("color:#00bcd4; font-weight:bold; font-size:11px; letter-spacing:1px;");

    QComboBox *cmbSelection = new QComboBox();
    cmbSelection->addItems({"Face (Surface)", "Edge (Line)", "Wire (Contour)"});
    cmbSelection->setStyleSheet(
        "QComboBox { background-color:#050608; color:#FFFFFF; border:1px solid #2a2d35; padding:8px 12px; border-radius:4px; font-weight:bold; font-size:13px; }"
        "QComboBox:hover { border:1px solid #00bcd4; }"
        "QComboBox::drop-down { border-left:1px solid #2a2d35; width:26px; }"
        "QComboBox QAbstractItemView { background-color:#0a0d14; color:#FFFFFF; border:1px solid #00bcd4; border-radius:4px; selection-background-color:#00bcd4; selection-color:#000000; outline:none; }"
        "QComboBox QAbstractItemView::item { min-height:32px; padding-left:8px; }");
    connect(cmbSelection, &QComboBox::currentIndexChanged, this, [this](int index){
        if (m_dxfPreviewWidget) m_dxfPreviewWidget->setSelectionMode(index + 1);
    });

    QLabel *lblDist = new QLabel("Distance (mm):");
    lblDist->setStyleSheet("color:#00bcd4; font-weight:bold; font-size:12px;");

    QLineEdit *txtDistance = new QLineEdit();
    txtDistance->setText("2.0");
    txtDistance->setStyleSheet(
        "QLineEdit { background:#1a1e2a; color:#ffffff; border:1px solid #2a2d35; padding:10px; border-radius:4px; font-size:14px; font-family:monospace; }"
        "QLineEdit:focus { border-color:#00bcd4; }");

    m_btnGetPoints = new QPushButton("📍 GET POINTS");
    m_btnGetPoints->setEnabled(false);
    m_btnGetPoints->setStyleSheet(
        "QPushButton { background-color:#2a3040; color:#64748b; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; }");

    // ========================================================
    // ✅ SMART RUN / STOP BUTTON (FREEZE BUG FIXED)
    // ========================================================
    QPushButton *btnRunDxf = new QPushButton("▶ RUN");
    btnRunDxf->setStyleSheet("QPushButton { background-color:#10B981; color:#000000; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; } QPushButton:hover { background-color:#059669; }");

    btnRunDxf->setProperty("isRunning", false);

    connect(btnRunDxf, &QPushButton::clicked, this, [this, btnRunDxf]() {
        bool isRunning = btnRunDxf->property("isRunning").toBool();

        if (!isRunning) {
            // 🟢 START THE PROGRAM
            QString csvData = m_txtCoordinates->toPlainText();
            if (csvData.isEmpty() || csvData.contains("Extracted XYZ")) {
                qDebug() << "⚠️ No points to run!";
                return;
            }

            // 1. பட்டனை STOP மோடுக்கு மாற்று
            btnRunDxf->setText("⏹ STOP");
            btnRunDxf->setStyleSheet("QPushButton { background-color:#EF4444; color:#FFFFFF; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; } QPushButton:hover { background-color:#DC2626; }");
            btnRunDxf->setProperty("isRunning", true);

            // 2. பட்டன் மாறியதை உறுதி செய்ய UI-ஐ அப்டேட் செய்
            QApplication::processEvents();

            // 3. Backend-ல் ரன் செய் (இது Popup கொடுத்தால், கீழேயுள்ள சிக்னல் பட்டனை மீண்டும் RUN-க்கு மாற்றிவிடும்)
            if (m_backend) m_backend->runDxfProgram(csvData);

        } else {
            // 🔴 STOP THE PROGRAM
            if (m_backend) m_backend->stopDxfProgram();
        }
    });

    // ✅ RESET BUTTON WHEN PROGRAM FINISHES OR ERROR OCCURS
    if (m_backend) {
        connect(m_backend, &ClientBackend::programFinished, this, [btnRunDxf]() {
            // Popup வந்து மூடியவுடனோ, அல்லது ரோபோ ஓடி முடித்தவுடனோ இது வேலை செய்யும்!
            btnRunDxf->setText("▶ RUN");
            btnRunDxf->setStyleSheet("QPushButton { background-color:#10B981; color:#000000; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; } QPushButton:hover { background-color:#059669; }");
            btnRunDxf->setProperty("isRunning", false);
        });
    }

    // ✅ NEW SPEED SLIDER
    QWidget *speedContainer = new QWidget();
    QHBoxLayout *speedLay = new QHBoxLayout(speedContainer);
    speedLay->setContentsMargins(0,5,0,5);

    QLabel *lblSpeedIcon = new QLabel("🚀 SPEED:");
    lblSpeedIcon->setStyleSheet("color:#00bcd4; font-weight:bold; font-size:12px;");

    QSlider *sliderSpeed = new QSlider(Qt::Horizontal);
    sliderSpeed->setRange(1, 100);
    sliderSpeed->setValue(100);
    sliderSpeed->setStyleSheet(
        "QSlider::groove:horizontal { background: #2a2d35; height: 6px; border-radius: 3px; } "
        "QSlider::handle:horizontal { background: #10B981; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; }");

    QLabel *lblSpeedVal = new QLabel("100%");
    lblSpeedVal->setStyleSheet("color:#10B981; font-weight:bold; font-size:13px; width: 45px;");
    lblSpeedVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(sliderSpeed, &QSlider::valueChanged, this, [this, lblSpeedVal](int v){
        lblSpeedVal->setText(QString::number(v) + "%");
        if (m_backend) m_backend->setAutoRunSpeed(v);
    });

    speedLay->addWidget(lblSpeedIcon);
    speedLay->addWidget(sliderSpeed);
    speedLay->addWidget(lblSpeedVal);

    m_txtCoordinates = new QTextEdit();
    m_txtCoordinates->setReadOnly(true);
    m_txtCoordinates->setPlaceholderText("Extracted XYZ coordinates will appear here...");
    m_txtCoordinates->setStyleSheet(
        "QTextEdit { background:#0a0d14; color:#00FF9D; border:1px solid #1e2330; border-radius:4px; padding:8px; font-family:'Consolas',monospace; font-size:12px; }");
    m_txtCoordinates->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_txtCoordinates->setMinimumHeight(40);

    ctrlLayout->addWidget(lblMode);
    ctrlLayout->addWidget(cmbSelection);
    ctrlLayout->addWidget(lblDist);
    ctrlLayout->addWidget(txtDistance);
    ctrlLayout->addWidget(m_btnGetPoints);
    ctrlLayout->addWidget(btnRunDxf);
    ctrlLayout->addWidget(speedContainer);
    ctrlLayout->addWidget(m_txtCoordinates, 1);

    dxfLayout->addWidget(dxfViewArea,    1);
    dxfLayout->addWidget(dxfControlArea, 1);

    connect(m_btnGetPoints, &QPushButton::clicked, this, [this, txtDistance, lblFileOrigin](){
        double dist = txtDistance->text().toDouble();
        if (dist <= 0.001) dist = 2.0;
        m_dxfPreviewWidget->processCurrentSelection(dist);
        lblFileOrigin->setText("3D File Origin -> " + m_dxfPreviewWidget->getOriginText());
    });

    connect(m_dxfPreviewWidget, &OcctWidget::coordinatesExtracted, this, [this](const QString &data){
        m_txtCoordinates->setPlainText(data);
    });

    connect(m_dxfPreviewWidget, &OcctWidget::selectionChanged, this, [this](bool hasSelection){
        this->setGetPointsEnabled(hasSelection);
    });

    return w;
}

// ============================================================
//  2. NEW FRAME MANAGEMENT WIDGET (WITH LOAD & CLEAR STEP)
// ============================================================
QWidget* RightPanel::buildFrameWidget()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background:#0d1117;");
    QVBoxLayout *mainLay = new QVBoxLayout(w);
    mainLay->setContentsMargins(15,15,15,15);
    mainLay->setSpacing(10);

    // --- TOP TOOLBAR ---
    QHBoxLayout *toolsLay = new QHBoxLayout();

    QPushButton *btnAdd = new QPushButton("+ ADD FRAME");
    btnAdd->setStyleSheet("QPushButton{background:#10B981; color:black; font-weight:bold; padding:8px; border-radius:4px;}");

    // ✅ NEW: LOAD STEP BUTTON
    QPushButton *btnLoadStep = new QPushButton("📂 LOAD STEP");
    btnLoadStep->setStyleSheet("QPushButton{background:#1565C0; color:white; font-weight:bold; padding:8px; border-radius:4px;}");

    // ✅ NEW: CLEAR STEP BUTTON
    QPushButton *btnClearStep = new QPushButton("🗑 CLEAR STEP");
    btnClearStep->setStyleSheet("QPushButton{background:#64748B; color:white; font-weight:bold; padding:8px; border-radius:4px;}");

    QPushButton *btnDel = new QPushButton("🗑 DELETE MODE");
    btnDel->setStyleSheet("QPushButton{background:#EF4444; color:white; font-weight:bold; padding:8px; border-radius:4px;}");

    // நீங்கள் கேட்ட வரிசையில் பட்டன்களை Layout-ல் சேர்க்கிறோம்
    toolsLay->addWidget(btnAdd);
    toolsLay->addWidget(btnLoadStep);
    toolsLay->addWidget(btnClearStep);
    toolsLay->addStretch();
    toolsLay->addWidget(btnDel);
    mainLay->addLayout(toolsLay);

    // --- SCROLL AREA ---
    QScrollArea *scroll = new QScrollArea();
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");
    QWidget *content = new QWidget();
    content->setStyleSheet("background:transparent;");

    m_frameListLayout = new QVBoxLayout(content);
    m_frameListLayout->setAlignment(Qt::AlignTop);
    m_frameListLayout->setSpacing(8);

    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    mainLay->addWidget(scroll);

    // --- BUTTON ACTIONS ---

    // 1. ADD FRAME
    connect(btnAdd, &QPushButton::clicked, this, [this](){
        m_userFrames.append(UserFrameData{0.0, 0.0, 0.0});
        saveUserFramesConfig();
        refreshFrameUI();
    });

    // 2. LOAD STEP
    // 2. LOAD STEP
    connect(btnLoadStep, &QPushButton::clicked, this, [this](){
        QString filePath = QFileDialog::getOpenFileName(this, "Select STEP File", "", "STEP Files (*.step *.stp)");
        if (!filePath.isEmpty()) {

            if (m_dxfPreviewWidget) m_dxfPreviewWidget->loadStepFile(filePath.toStdString());

            // ✅ THE MAGIC: இது MainWindow வழியாக Left Panel-ல் ஃபைலை லோட் செய்யும்!
            emit requestMainLoadStep(filePath);

            if (m_activeFrameIndex >= 0 && m_activeFrameIndex < m_userFrames.size()) {
                double fx = m_userFrames[m_activeFrameIndex].x;
                double fy = m_userFrames[m_activeFrameIndex].y;
                double fz = m_userFrames[m_activeFrameIndex].z;

                if (m_dxfPreviewWidget) m_dxfPreviewWidget->setUserFrameOrigin(fx, fy, fz);
                emit requestMainSetUserFrame(fx, fy, fz); // ✅ Left Panel-ல் மார்க்கரை செட் செய்யும்!
                updateOriginLabel(fx, fy, fz);
            }
        }
    });

    // 3. CLEAR STEP
    connect(btnClearStep, &QPushButton::clicked, this, [this](){
        if (m_dxfPreviewWidget) m_dxfPreviewWidget->clearLoadedPart();
        emit requestMainClearStep(); // ✅ Left Panel-ல் மாடலை அழிக்கும்!
    });

    // 4. DELETE MODE
    connect(btnDel, &QPushButton::clicked, this, [this, btnDel](){
        if (!m_frameDeleteMode) {
            m_frameDeleteMode = true;
            btnDel->setText("✅ CONFIRM DELETE");
            btnDel->setStyleSheet("QPushButton{background:#B91C1C; color:white; font-weight:bold; padding:8px; border-radius:4px; border:2px solid #FCA5A5;}");
        } else {
            for (int i = m_frameCheckboxes.size() - 1; i >= 0; i--) {
                if (m_frameCheckboxes[i]->isChecked()) {
                    int idx = m_frameCheckboxes[i]->property("frameIndex").toInt();
                    m_userFrames.removeAt(idx);
                    if (m_activeFrameIndex == idx) m_activeFrameIndex = -1;
                    else if (m_activeFrameIndex > idx) m_activeFrameIndex--;
                }
            }
            m_frameDeleteMode = false;
            btnDel->setText("🗑 DELETE MODE");
            btnDel->setStyleSheet("QPushButton{background:#EF4444; color:white; font-weight:bold; padding:8px; border-radius:4px;}");
        }
        saveUserFramesConfig();
        refreshFrameUI();
    });

    refreshFrameUI();
    return w;
}

// ============================================================
//  3. FRAME LIST REFRESHER (WITH EDIT / SAVE TOGGLE)
// ============================================================
void RightPanel::refreshFrameUI()
{
    if (!m_frameListLayout) return;

    QLayoutItem *child;
    while ((child = m_frameListLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    m_frameCheckboxes.clear();

    // ✅ ஸ்டைல்களை முன்கூட்டியே வரையறுக்கிறோம்
    QString readOnlyStyle = "QLineEdit { background:transparent; color:#00E5FF; border:none; font-weight:bold; font-size:13px; font-family:monospace; }";
    QString editStyle = "QLineEdit { background:#0a0d14; color:#FFFFFF; border:1px solid #F59E0B; border-radius:2px; padding:2px; font-size:13px; font-family:monospace; }";
    QString lblStyle = "QLabel { color:#9CA3AF; font-weight:bold; font-size:12px; border:none; background:transparent; }"; // கறுப்பு நிறத்தைத் தடுக்க

    for (int i = 0; i < m_userFrames.size(); i++) {
        QWidget *row = new QWidget();
        row->setStyleSheet("background:#1a1e2a; border:1px solid #2a2d35; border-radius:4px;");
        QHBoxLayout *rLay = new QHBoxLayout(row);
        rLay->setContentsMargins(10,8,10,8);

        if (m_frameDeleteMode) {
            QCheckBox *chk = new QCheckBox();
            chk->setProperty("frameIndex", i);
            chk->setStyleSheet("QCheckBox::indicator { width:18px; height:18px; }");
            m_frameCheckboxes.append(chk);
            rLay->addWidget(chk);
        }

        QLabel *lbl = new QLabel();
        if (m_activeFrameIndex == i) {
            lbl->setText(QString("★ UF %1 (SET)").arg(i + 1));
            lbl->setStyleSheet("color:#10B981; font-weight:bold; font-size:13px; border:none;");
        } else {
            lbl->setText(QString("USERFRAME %1").arg(i + 1));
            lbl->setStyleSheet("color:#00bcd4; font-weight:bold; font-size:12px; border:none;");
        }
        rLay->addWidget(lbl);
        rLay->addStretch();

        // ✅ X Label & Input
        QLabel *lblX = new QLabel("X:"); lblX->setStyleSheet(lblStyle); rLay->addWidget(lblX);
        QLineEdit *xEdit = new QLineEdit(QString::number(m_userFrames[i].x));
        xEdit->setFixedWidth(55); xEdit->setReadOnly(true); xEdit->setStyleSheet(readOnlyStyle); rLay->addWidget(xEdit);

        // ✅ Y Label & Input
        QLabel *lblY = new QLabel("Y:"); lblY->setStyleSheet(lblStyle); rLay->addWidget(lblY);
        QLineEdit *yEdit = new QLineEdit(QString::number(m_userFrames[i].y));
        yEdit->setFixedWidth(55); yEdit->setReadOnly(true); yEdit->setStyleSheet(readOnlyStyle); rLay->addWidget(yEdit);

        // ✅ Z Label & Input
        QLabel *lblZ = new QLabel("Z:"); lblZ->setStyleSheet(lblStyle); rLay->addWidget(lblZ);
        QLineEdit *zEdit = new QLineEdit(QString::number(m_userFrames[i].z));
        zEdit->setFixedWidth(55); zEdit->setReadOnly(true); zEdit->setStyleSheet(readOnlyStyle); rLay->addWidget(zEdit);

        // ========================================================
        // ✅ 2-IN-1 BUTTON (EDIT <--> SAVE)
        // ========================================================
        QPushButton *btnEditSave = new QPushButton("✏️EDIT");
        btnEditSave->setProperty("isEditing", false); // ஆரம்பத்தில் எடிட் மோடில் இல்லை
        btnEditSave->setStyleSheet("QPushButton{background:#37474f; color:white; font-weight:bold; padding:4px 10px; border-radius:3px;}");

        connect(btnEditSave, &QPushButton::clicked, this, [this, i, xEdit, yEdit, zEdit, btnEditSave, readOnlyStyle, editStyle](){
            bool isEditing = btnEditSave->property("isEditing").toBool();

            if (!isEditing) {
                // 1️⃣ எடிட் பட்டனை அழுத்தியதும் -> SAVE மோடிற்கு மாறுதல்
                xEdit->setReadOnly(false); xEdit->setStyleSheet(editStyle);
                yEdit->setReadOnly(false); yEdit->setStyleSheet(editStyle);
                zEdit->setReadOnly(false); zEdit->setStyleSheet(editStyle);

                btnEditSave->setText("📂SAVE");
                btnEditSave->setStyleSheet("QPushButton{background:#F59E0B; color:black; font-weight:bold; padding:4px 10px; border-radius:3px;}");
                btnEditSave->setProperty("isEditing", true);
                xEdit->setFocus(); // டைப் செய்ய ரெடி ஆக்குகிறது
            } else {
                // 2️⃣ SAVE பட்டனை அழுத்தியதும் -> மீண்டும் EDIT மோடிற்கு மாறுதல்
                m_userFrames[i].x = xEdit->text().toDouble();
                m_userFrames[i].y = yEdit->text().toDouble();
                m_userFrames[i].z = zEdit->text().toDouble();
                saveUserFramesConfig(); // ஹார்ட் டிஸ்க்கில் சேமிக்கிறது

                xEdit->setReadOnly(true); xEdit->setStyleSheet(readOnlyStyle);
                yEdit->setReadOnly(true); yEdit->setStyleSheet(readOnlyStyle);
                zEdit->setReadOnly(true); zEdit->setStyleSheet(readOnlyStyle);

                btnEditSave->setText("✏️EDIT");
                btnEditSave->setStyleSheet("QPushButton{background:#37474f; color:white; font-weight:bold; padding:4px 10px; border-radius:3px;}");
                btnEditSave->setProperty("isEditing", false);

                // Active Frame-ஐ எடிட் செய்திருந்தால், உடனடியாக 3D மார்க்கரையும் நகர்த்துகிறோம்
                if (m_activeFrameIndex == i && m_dxfPreviewWidget) {
                    m_dxfPreviewWidget->setUserFrameOrigin(m_userFrames[i].x, m_userFrames[i].y, m_userFrames[i].z);
                    emit requestMainSetUserFrame(m_userFrames[i].x, m_userFrames[i].y, m_userFrames[i].z);
                    updateOriginLabel(m_userFrames[i].x, m_userFrames[i].y, m_userFrames[i].z);
                }
            }
        });
        rLay->addWidget(btnEditSave);

        // ========================================================
        // ✅ SET BUTTON
        // ========================================================
        QPushButton *btnSet = new QPushButton("SET");
        btnSet->setStyleSheet("QPushButton{background:#00E5FF; color:black; font-weight:bold; padding:4px 10px; border-radius:3px;}");
        connect(btnSet, &QPushButton::clicked, this, [this, i](){
            m_activeFrameIndex = i;

            double fx = m_userFrames[i].x;
            double fy = m_userFrames[i].y;
            double fz = m_userFrames[i].z;

            if(m_dxfPreviewWidget) m_dxfPreviewWidget->setUserFrameOrigin(fx, fy, fz);
            emit requestMainSetUserFrame(fx, fy, fz);

            updateOriginLabel(fx, fy, fz);
            saveUserFramesConfig();
            refreshFrameUI();
        });
        rLay->addWidget(btnSet);

        m_frameListLayout->addWidget(row);
    }
}

// ============================================================
//  buildSpeedPanel
// ============================================================
QWidget* RightPanel::buildSpeedPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setStyleSheet("background:#151822;");
    QHBoxLayout *mainLayout = new QHBoxLayout(panel);
    mainLayout->setContentsMargins(20,10,20,10);

    const QString comboStyle = "QComboBox{background:white;color:black;font-weight:bold;padding:5px;border-radius:2px;min-width:100px;}";
    const QString spinStyle  = "QDoubleSpinBox{background:white;color:black;font-weight:bold;padding:5px;border-radius:2px;min-width:100px;}";
    const QString lblStyle   = "QLabel{color:white;font-weight:bold;font-size:13px;}";

    QWidget *formWidget = new QWidget();
    QFormLayout *form = new QFormLayout(formWidget);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(15);

    QComboBox *cmbMmInc = new QComboBox();
    cmbMmInc->addItems({"mm","50","25","15","10","5","2","1","0.1","0.01","0.001"});
    cmbMmInc->setStyleSheet(comboStyle);

    QDoubleSpinBox *spnMms = new QDoubleSpinBox();
    spnMms->setRange(0.1,500.0); spnMms->setValue(50.0);
    spnMms->setStyleSheet(spinStyle);

    QComboBox *cmbDegInc = new QComboBox();
    cmbDegInc->addItems({"deg","20","15","10","5","2","1","0.1","0.01","0.0001"});
    cmbDegInc->setStyleSheet(comboStyle);

    QDoubleSpinBox *spnDegs = new QDoubleSpinBox();
    spnDegs->setRange(0.1,500.0); spnDegs->setValue(50.0);
    spnDegs->setStyleSheet(spinStyle);

    QComboBox *cmbFrame = new QComboBox();
    cmbFrame->addItems({"frames","Base","Tool","User"});
    cmbFrame->setStyleSheet(comboStyle);

    connect(cmbMmInc,  &QComboBox::currentTextChanged, this, [this](const QString &t){ if (m_backend) m_backend->setMmIncrement(t); });
    connect(cmbDegInc, &QComboBox::currentTextChanged, this, [this](const QString &t){ if (m_backend) m_backend->setDegIncrement(t); });
    connect(spnMms,    &QDoubleSpinBox::valueChanged,  this, [this](double v){ if (m_backend) m_backend->setCartesianSpeed(v); });
    connect(spnDegs,   &QDoubleSpinBox::valueChanged,  this, [this](double v){ if (m_backend) m_backend->setJointSpeed(v); });

    auto mkL = [&](const QString &t){ QLabel *l = new QLabel(t); l->setStyleSheet(lblStyle); return l; };
    form->addRow(mkL("MM Inc:"),  cmbMmInc);
    form->addRow(mkL("MM/S:"),    spnMms);
    form->addRow(mkL("DEG Inc:"), cmbDegInc);
    form->addRow(mkL("DEG/S:"),   spnDegs);
    form->addRow(mkL("FRAME:"),   cmbFrame);

    QWidget *speedWidget = new QWidget();
    QVBoxLayout *sl = new QVBoxLayout(speedWidget);
    sl->setAlignment(Qt::AlignCenter); sl->setSpacing(20);

    QLabel *lblTitle = new QLabel("GLOBAL SPEED");
    lblTitle->setStyleSheet("color:white;font-weight:bold;font-size:16px;letter-spacing:2px;");
    lblTitle->setAlignment(Qt::AlignCenter);

    QLabel *lblReadout = new QLabel("50%");
    lblReadout->setStyleSheet("color:#0EA5E9;font-weight:bold;font-size:32px;border:2px solid #0EA5E9;border-radius:8px;padding:10px 30px;background:#1E1E24;");
    lblReadout->setAlignment(Qt::AlignCenter);

    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(1,100); slider->setValue(50);
    slider->setStyleSheet(
        "QSlider::groove:horizontal{border-radius:4px;height:8px;background:#2D2D30;}"
        "QSlider::sub-page:horizontal{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #22C55E,stop:1 #0EA5E9);border-radius:4px;}"
        "QSlider::handle:horizontal{background:white;width:18px;height:18px;margin:-5px 0;border-radius:9px;}");
    connect(slider, &QSlider::valueChanged, this, [this, lblReadout](int v){
        lblReadout->setText(QString::number(v) + "%");
        if (m_backend) m_backend->setGlobalSpeed(v);
    });

    sl->addWidget(lblTitle);
    sl->addWidget(lblReadout, 0, Qt::AlignCenter);
    sl->addWidget(slider);

    mainLayout->addWidget(formWidget, 1);
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet("background:#3E3E42;max-width:1px;border:none;");
    mainLayout->addWidget(line);
    mainLayout->addWidget(speedWidget, 1);
    return panel;
}

// ============================================================
//  buildJointsPanel
// ============================================================
QWidget* RightPanel::buildJointsPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setStyleSheet("background:#151822;");
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);
    mainLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QGridLayout *grid = new QGridLayout(); grid->setSpacing(8);
    QLabel *lN = new QLabel("NEG"); lN->setStyleSheet("color:#EF4444;font-weight:bold;"); lN->setAlignment(Qt::AlignCenter);
    QLabel *lB = new QLabel("BASE / WRIST"); lB->setStyleSheet("color:white;font-weight:bold;"); lB->setAlignment(Qt::AlignCenter);
    QLabel *lP = new QLabel("POS"); lP->setStyleSheet("color:#22C55E;font-weight:bold;"); lP->setAlignment(Qt::AlignCenter);
    grid->addWidget(lN,0,0); grid->addWidget(lB,0,1); grid->addWidget(lP,0,2);

    const QString bRed = "QPushButton{background:#EF4444;color:white;font-weight:bold;font-size:18px;border-radius:4px;border-bottom:4px solid #B91C1C;min-width:95px;min-height:42px;}QPushButton:pressed{margin-top:4px;border-bottom:0;}";
    const QString bGrn = "QPushButton{background:#4ADE80;color:white;font-weight:bold;font-size:18px;border-radius:4px;border-bottom:4px solid #16A34A;min-width:95px;min-height:42px;}QPushButton:pressed{margin-top:4px;border-bottom:0;}";
    const QString lblJ = "QLabel{color:white;font-weight:bold;font-size:15px;}";

    for (int i = 1; i <= 6; ++i) {
        QPushButton *bN = new QPushButton(QString("J%1-").arg(i)); bN->setStyleSheet(bRed);
        QLabel     *lJ = new QLabel(QString("J%1").arg(i));         lJ->setStyleSheet(lblJ); lJ->setAlignment(Qt::AlignCenter);
        QPushButton *bP = new QPushButton(QString("J%1+").arg(i)); bP->setStyleSheet(bGrn);

        connect(bN, &QPushButton::pressed,  this, [this, bN]{ if (m_backend) m_backend->handleButtonPress(bN->text()); });
        connect(bN, &QPushButton::released, this, [this, bN]{ if (m_backend) m_backend->handleButtonRelease(bN->text()); });
        connect(bP, &QPushButton::pressed,  this, [this, bP]{ if (m_backend) m_backend->handleButtonPress(bP->text()); });
        connect(bP, &QPushButton::released, this, [this, bP]{ if (m_backend) m_backend->handleButtonRelease(bP->text()); });

        grid->addWidget(bN, i, 0); grid->addWidget(lJ, i, 1); grid->addWidget(bP, i, 2);
    }
    mainLayout->addLayout(grid);
    mainLayout->addStretch();
    return panel;
}

// ============================================================
//  SLOTS
// ============================================================
void RightPanel::setGetPointsEnabled(bool enabled)
{
    if (!m_btnGetPoints) return;
    m_btnGetPoints->setEnabled(enabled);
    m_btnGetPoints->setStyleSheet(enabled
                                      ? "QPushButton { background-color:#F59E0B; color:#000000; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; } QPushButton:hover { background-color:#D97706; }"
                                      : "QPushButton { background-color:#2a3040; color:#64748b; font-weight:bold; padding:12px; border-radius:4px; border:none; font-size:13px; }");
}

void RightPanel::onTelemetryChanged()
{
    if (m_uiThrottle.elapsed() < 33) return;
    m_uiThrottle.restart();
    updateIOLeds();
}

void RightPanel::onProgramDataChanged()   {}
void RightPanel::onDirectoryDataChanged() {}
void RightPanel::onLocalStateChanged()    {}
void RightPanel::onHighlightChanged()     {}

void RightPanel::updateIOLeds()            {}
void RightPanel::updateInstructionTable()  {}
void RightPanel::updatePrTable()           {}
void RightPanel::updateTpTable()           {}
void RightPanel::updateOpPgDisplay()       {}

void RightPanel::setActiveTab(int index)
{
    if (m_workspaceTabs) m_workspaceTabs->setCurrentIndex(index);
}

void RightPanel::updateOriginLabel(double x, double y, double z)
{
    if (m_lblOrigin) {
        m_lblOrigin->setText(
            QString("Part Offset -> X: %1 | Y: %2 | Z: %3")
                .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3));
    }
}

// ============================================================
//  resizeEvent
// ============================================================
void RightPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (tray && !tray->isHidden()) {
        const int tw = 280;
        tray->setGeometry(this->width() - tw, 0, tw, this->height());
    }
}

// ============================================================
//  SAVE & LOAD USER FRAMES (QSettings)
// ============================================================
void RightPanel::saveUserFramesConfig()
{
    QSettings settings("Texsonics", "RobotStudio");

    settings.beginWriteArray("UserFrames");
    for (int i = 0; i < m_userFrames.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("x", m_userFrames[i].x);
        settings.setValue("y", m_userFrames[i].y);
        settings.setValue("z", m_userFrames[i].z);
    }
    settings.endArray();

    settings.setValue("ActiveFrameIndex", m_activeFrameIndex);
}

void RightPanel::loadUserFramesConfig()
{
    QSettings settings("Texsonics", "RobotStudio");

    m_userFrames.clear();

    int frameCount = settings.beginReadArray("UserFrames");
    if (frameCount > 0) {
        for (int i = 0; i < frameCount; ++i) {
            settings.setArrayIndex(i);
            double x = settings.value("x", 0.0).toDouble();
            double y = settings.value("y", 0.0).toDouble();
            double z = settings.value("z", 0.0).toDouble();
            m_userFrames.append(UserFrameData{x, y, z});
        }
    } else {

        m_userFrames.append(UserFrameData{0.0, -800.0, 600.0});
    }
    settings.endArray();

    m_activeFrameIndex = settings.value("ActiveFrameIndex", 0).toInt();
}


// ============================================================
//  eventFilter
// ============================================================
bool RightPanel::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    return QWidget::eventFilter(watched, event);
}