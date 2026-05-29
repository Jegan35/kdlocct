#include "MainWindow.h"
#include "LoginDialog.h"
#include <QApplication>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_backend = new ClientBackend(this);

    QStackedWidget *mainStack = new QStackedWidget(this);
    setCentralWidget(mainStack);

    // ==========================================
    // TEMP OFF: Login Page Bypassed
    // ==========================================
    // LoginDialog *loginPage = new LoginDialog(m_backend, mainStack);
    // mainStack->addWidget(loginPage);

    QWidget *dashboardPage = new QWidget(mainStack);
    dashboardPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *dashLayout = new QVBoxLayout(dashboardPage);
    dashLayout->setContentsMargins(0, 0, 0, 8);
    dashLayout->setSpacing(0);
    mainStack->addWidget(dashboardPage);

    // 1. Swap to dashboard page immediately
    mainStack->setCurrentWidget(dashboardPage);

    // ==========================================
    // DASHBOARD CONSTRUCTION (Moved out of lambda)
    // ==========================================

    // 2. Build the Splitter
    this->mainSplitter = new QSplitter(Qt::Horizontal, dashboardPage);
    this->mainSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->mainSplitter->setStyleSheet(
        "QSplitter::handle { background-color: #1E1E1E; width: 4px; }");

    // 3. Build Panels
    this->leftPanel  = new LeftPanel (this->m_backend, this->mainSplitter);
    this->rightPanel = new RightPanel(this->m_backend, this->mainSplitter);

    // ✅ THE ULTIMATE SPLITTER FIX:
    // Changing "Expanding" to "Ignored" horizontally forces both panels
    // to surrender to the QSplitter. The giant tables can no longer break the 50/50 split!
    this->leftPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    this->rightPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    this->leftPanel->setMinimumWidth(200);
    this->rightPanel->setMinimumWidth(200);

    this->mainSplitter->addWidget(this->leftPanel);
    this->mainSplitter->addWidget(this->rightPanel);

    // 4. Force 50/50 Layout Proportionately
    this->mainSplitter->setStretchFactor(0, 1);
    this->mainSplitter->setStretchFactor(1, 1);
    this->mainSplitter->setCollapsible(0, false);
    this->mainSplitter->setCollapsible(1, false);

    dashLayout->addWidget(this->mainSplitter, 1);

    // 5. Wire the Panels together
    // ========================================================
    // ✅ NEW: ROUTE FRAME COMMANDS TO THE LEFT PANEL (ROBOT GRAPH)
    // ========================================================
    connect(this->rightPanel, &RightPanel::requestMainLoadStep, this, [this](const QString& filePath){
        if(this->leftPanel) this->leftPanel->getMainOcctWidget()->loadStepFile(filePath.toStdString());
    });

    connect(this->rightPanel, &RightPanel::requestMainClearStep, this, [this](){
        if(this->leftPanel) this->leftPanel->getMainOcctWidget()->clearLoadedPart();
    });

    connect(this->rightPanel, &RightPanel::requestMainSetUserFrame, this, [this](double x, double y, double z){
        if(this->leftPanel) this->leftPanel->getMainOcctWidget()->setUserFrameOrigin(x, y, z);
    });
    connect(this->leftPanel->getMainOcctWidget(),  &OcctWidget::partSelectedForIsolation,
            this->rightPanel->getDxfPreviewWidget(), &OcctWidget::displayIsolatedPart);

    connect(this->rightPanel, &RightPanel::swipeLockToggled,
            this->leftPanel,  &LeftPanel::setSwipeEnabled);

    connect(this->rightPanel, &RightPanel::requestFooterSwipe,
            this->leftPanel,  &LeftPanel::toggleFooterSwipe);

    connect(this->leftPanel,  &LeftPanel::requestTabChange,
            this->rightPanel, &RightPanel::setActiveTab);

    // ========================================================
    // ✅ THE PERFECT FIX: TRUE FULLSCREEN (KIOSK MODE)
    // This bypasses the Ubuntu window manager completely.
    // No more cutoffs, and it looks like a professional industrial controller.
    // ========================================================
    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    this->showFullScreen();
}

MainWindow::~MainWindow() {}