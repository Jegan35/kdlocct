#include "ClientBackend.h"
#include <QVariant>
#include <QDebug>
#include <cmath>
#include <algorithm>
//origin userframe 0.0, -800.0, 600.0
// Include your kinematics
#include "kinematic.h"

// Link to the global KDL variables you defined in kinematic.cpp
extern KDL::Chain KDLChain;
extern KDL::JntArray KDLJointMin;
extern KDL::JntArray KDLJointMax;
extern KDL::JntArray KDLJointCur;
extern KDL::Frame cart;

// Important: Pass the parent pointer to the constructor
ClientBackend::ClientBackend(QObject *parent) : QObject(parent)
{
    m_j1 = m_j2 = m_j3 = m_j4 = m_j5 = m_j6 = 0.0;

    // Initialize the KDL Chain!
    m_kinematics.Init();


    m_userFrame = KDL::Frame(KDL::Rotation::Identity(), KDL::Vector(0.0, -800.0, 600.0));

    m_playbackTimer = new QTimer(this);
    connect(m_playbackTimer, &QTimer::timeout, this, &ClientBackend::playbackTick);
    m_jogTimer = new QTimer(this);
    connect(m_jogTimer, &QTimer::timeout, this, &ClientBackend::jogTick);
}

void ClientBackend::calculateAndRunHome()
{
    m_localJointTrajectory.clear();

    // Use the current joint positions stored in the class
    double start_j[6] = { m_j1, m_j2, m_j3, m_j4, m_j5, m_j6 };
    double target_j[6] = { 0.0, 0.0, 0.0, 0.0, 90.0, 0.0 };

    double D = 0.0;
    for (int i = 0; i < 6; i++) {
        D = std::max(D, std::abs(target_j[i] - start_j[i]));
    }

    if (D < 0.001) {
        qDebug() << "Already at Home position!";
        return;
    }

    scurve trajectoryPlanner;
    std::vector<scurve::point> pathvec;
    pathvec.push_back({0,0,0});
    pathvec.push_back({D,0,0});

    // 50.0 = Max Velocity, 100.0 = Max Accel
    auto rawSCurve = trajectoryPlanner.create_point_for_every_ms_path(50.0, 100.0, 5.0, 5.0, pathvec);

    for (size_t i = 0; i < rawSCurve.size(); i++) {
        double progress = (D > 0) ? (rawSCurve[i].x / D) : 0.0;
        JointPoint jp;
        jp.j1 = start_j[0] + (target_j[0] - start_j[0]) * progress;
        jp.j2 = start_j[1] + (target_j[1] - start_j[1]) * progress;
        jp.j3 = start_j[2] + (target_j[2] - start_j[2]) * progress;
        jp.j4 = start_j[3] + (target_j[3] - start_j[3]) * progress;
        jp.j5 = start_j[4] + (target_j[4] - start_j[4]) * progress;
        jp.j6 = start_j[5] + (target_j[5] - start_j[5]) * progress;
        m_localJointTrajectory.append(jp);
    }

    m_isCartesianPlayback = false; // இது Joint Mode என்று சொல்கிறோம்
    m_playbackIndex = 0;
    m_playbackTimer->start(16);// 20ms tick
}

void ClientBackend::playbackTick()
{
    // ==========================================
    // 1. CARTESIAN PLAYBACK (DXF/STEP AUTO RUN)
    // ==========================================
    if (m_isCartesianPlayback) {
        if (m_playbackIndex >= m_cartesianTrajectory.size()) {
            m_playbackTimer->stop();
            emit programFinished();
            return;
        }

        scurve::point pt = m_cartesianTrajectory[m_playbackIndex];

        // Tool கோணத்தை மாற்றாமல் XYZ-ஐ மட்டும் அப்டேட் செய்கிறோம்
        KDL::Frame target_base_cart(cart.M, KDL::Vector(pt.x, pt.y, pt.z));
        KDL::JntArray target_joints(6);

        // ========================================================
        // ✅ FIX 3: FAST TRACKING IK (NO LAG!)
        // 18,000 புள்ளிகளுக்கும் லேக் வராமல் இருக்க Fast Solver பயன்படுத்துகிறோம்
        // ========================================================
        KDL::ChainFkSolverPos_recursive fksolver(KDLChain);
        KDL::ChainIkSolverVel_pinv iksolverv(KDLChain);
        KDL::ChainIkSolverPos_NR_JL iksolver(KDLChain, KDLJointMin, KDLJointMax, fksolver, iksolverv, 50, 1e-4);

        if (iksolver.CartToJnt(KDLJointCur, target_base_cart, target_joints) >= 0) {
            m_j1 = target_joints(0) * (180.0 / M_PI);
            m_j2 = target_joints(1) * (180.0 / M_PI);
            m_j3 = target_joints(2) * (180.0 / M_PI);
            m_j4 = target_joints(3) * (180.0 / M_PI);
            m_j5 = target_joints(4) * (180.0 / M_PI);
            m_j6 = target_joints(5) * (180.0 / M_PI);

            KDLJointCur = target_joints;
        } else {
            qDebug() << "❌ IK FAILED! Stopping Robot.";
            m_playbackTimer->stop();
            emit programFinished();
            return;
        }

        m_playbackIndex += 16;
    }
    // ==========================================
    // 2. JOINT PLAYBACK (HOME BUTTON)
    // ==========================================
    else {
        if (m_playbackIndex >= m_localJointTrajectory.size()) {
            m_playbackTimer->stop();
            emit programFinished();
            return;
        }

        JointPoint jp = m_localJointTrajectory[m_playbackIndex];
        m_j1 = jp.j1; m_j2 = jp.j2; m_j3 = jp.j3;
        m_j4 = jp.j4; m_j5 = jp.j5; m_j6 = jp.j6;

        KDLJointCur(0) = m_j1 * (M_PI / 180.0);
        KDLJointCur(1) = m_j2 * (M_PI / 180.0);
        KDLJointCur(2) = m_j3 * (M_PI / 180.0);
        KDLJointCur(3) = m_j4 * (M_PI / 180.0);
        KDLJointCur(4) = m_j5 * (M_PI / 180.0);
        KDLJointCur(5) = m_j6 * (M_PI / 180.0);

        m_playbackIndex += 16;
    }

    // ==========================================
    // 3. COMMON UI UPDATE
    // ==========================================
    m_kinematics.Fk();
    updateUIWithUserFrame();
}


void ClientBackend::setGlobalSpeed(int percent) {
    m_globalSpeed = percent;
    qDebug() << "⚙️ BACKEND: Global Speed Set to ->" << m_globalSpeed << "%";
}
void ClientBackend::setCartesianSpeed(double mms) {
    m_cartSpeed = mms;
    qDebug() << "⚙️ BACKEND: Cartesian Speed Set to ->" << m_cartSpeed << "MM/S";
}
void ClientBackend::setJointSpeed(double degs) {
    m_jointSpeed = degs;
    qDebug() << "⚙️ BACKEND: Joint Speed Set to ->" << m_jointSpeed << "DEG/S";
}

void ClientBackend::setMmIncrement(const QString &val) {
    if (val == "mm") m_mmIncrement = 0.0; // Continuous Mode
    else m_mmIncrement = val.toDouble();
    qDebug() << "📏 BACKEND: MM Increment Set to ->" << m_mmIncrement;
}
void ClientBackend::setDegIncrement(const QString &val) {
    if (val == "deg") m_degIncrement = 0.0; // Continuous Mode
    else m_degIncrement = val.toDouble();
    qDebug() << "📐 BACKEND: Degree Increment Set to ->" << m_degIncrement;
}


void ClientBackend::handleButtonPress(const QString &btnText)
{
    if (!m_jogTimer) return;
    m_activeJogButton = btnText;

    bool isJointJog = btnText.startsWith("J");

    if ((isJointJog && m_degIncrement > 0.0) || (!isJointJog && m_mmIncrement > 0.0)) {
        executeStepJog();
    }
    else {
        // 16ms = 60 FPS (Butter Smooth)
        m_jogTimer->start(16);
    }
}
void ClientBackend::handleButtonRelease(const QString &btnText)
{
    if (!m_jogTimer) return;
    if (m_activeJogButton == btnText) {
        qDebug() << "⏹️ JOG RELEASE:" << btnText;
        m_jogTimer->stop();
        m_activeJogButton = "";
    }
}

// ========================================================
// ✅ 1. THE STEP JOG ENGINE (Click to Move)
// ========================================================
void ClientBackend::executeStepJog()
{
    qDebug() << "🚀 Executing STEP Jog for:" << m_activeJogButton;

    // ---- JOINT STEP JOGGING ----
    if (m_activeJogButton == "J1+") m_j1 += m_degIncrement;
    else if (m_activeJogButton == "J1-") m_j1 -= m_degIncrement;
    else if (m_activeJogButton == "J2+") m_j2 += m_degIncrement;
    else if (m_activeJogButton == "J2-") m_j2 -= m_degIncrement;
    else if (m_activeJogButton == "J3+") m_j3 += m_degIncrement;
    else if (m_activeJogButton == "J3-") m_j3 -= m_degIncrement;
    else if (m_activeJogButton == "J4+") m_j4 += m_degIncrement;
    else if (m_activeJogButton == "J4-") m_j4 -= m_degIncrement;
    else if (m_activeJogButton == "J5+") m_j5 += m_degIncrement;
    else if (m_activeJogButton == "J5-") m_j5 -= m_degIncrement;
    else if (m_activeJogButton == "J6+") m_j6 += m_degIncrement;
    else if (m_activeJogButton == "J6-") m_j6 -= m_degIncrement;

    // ---- CARTESIAN STEP JOGGING (WITH USER FRAME IK) ----
    else {
        KDL::Frame current_user_cart = m_userFrame.Inverse() * cart;

        if (m_activeJogButton == "X+") current_user_cart.p.x(current_user_cart.p.x() + m_mmIncrement);
        else if (m_activeJogButton == "X-") current_user_cart.p.x(current_user_cart.p.x() - m_mmIncrement);
        else if (m_activeJogButton == "Y+") current_user_cart.p.y(current_user_cart.p.y() + m_mmIncrement);
        else if (m_activeJogButton == "Y-") current_user_cart.p.y(current_user_cart.p.y() - m_mmIncrement);
        else if (m_activeJogButton == "Z+") current_user_cart.p.z(current_user_cart.p.z() + m_mmIncrement);
        else if (m_activeJogButton == "Z-") current_user_cart.p.z(current_user_cart.p.z() - m_mmIncrement);

        // Convert back to Base Frame for IK Calculation
        KDL::Frame target_base_cart = m_userFrame * current_user_cart;

        KDL::JntArray target_joints(6);
        if (m_kinematics.Ik_Optimal_Solution(target_base_cart, KDLJointCur, target_joints)) {
            m_j1 = target_joints(0) * (180.0 / M_PI);
            m_j2 = target_joints(1) * (180.0 / M_PI);
            m_j3 = target_joints(2) * (180.0 / M_PI);
            m_j4 = target_joints(3) * (180.0 / M_PI);
            m_j5 = target_joints(4) * (180.0 / M_PI);
            m_j6 = target_joints(5) * (180.0 / M_PI);
        } else {
            qDebug() << "⚠️ IK FAILED: Cannot step to this position!";
            return;
        }
    }

    // Update KDL
    KDLJointCur(0) = m_j1 * (M_PI / 180.0);
    KDLJointCur(1) = m_j2 * (M_PI / 180.0);
    KDLJointCur(2) = m_j3 * (M_PI / 180.0);
    KDLJointCur(3) = m_j4 * (M_PI / 180.0);
    KDLJointCur(4) = m_j5 * (M_PI / 180.0);
    KDLJointCur(5) = m_j6 * (M_PI / 180.0);

    m_kinematics.Fk();

    // ✅ UI UPDATE
    updateUIWithUserFrame();
}

// ========================================================
// ✅ 2. CONTINUOUS JOG ENGINE (Press and Hold)
// ========================================================
void ClientBackend::jogTick()
{
    // 60 FPS Smoothness
    double dt = 0.016;

    double actualJointSpeed = m_jointSpeed * (m_globalSpeed / 100.0);
    double actualCartSpeed  = m_cartSpeed  * (m_globalSpeed / 100.0);

    double jStep = actualJointSpeed * dt;
    double cStep = actualCartSpeed * dt;

    if (m_activeJogButton == "J1+") m_j1 += jStep;
    else if (m_activeJogButton == "J1-") m_j1 -= jStep;
    else if (m_activeJogButton == "J2+") m_j2 += jStep;
    else if (m_activeJogButton == "J2-") m_j2 -= jStep;
    else if (m_activeJogButton == "J3+") m_j3 += jStep;
    else if (m_activeJogButton == "J3-") m_j3 -= jStep;
    else if (m_activeJogButton == "J4+") m_j4 += jStep;
    else if (m_activeJogButton == "J4-") m_j4 -= jStep;
    else if (m_activeJogButton == "J5+") m_j5 += jStep;
    else if (m_activeJogButton == "J5-") m_j5 -= jStep;
    else if (m_activeJogButton == "J6+") m_j6 += jStep;
    else if (m_activeJogButton == "J6-") m_j6 -= jStep;

    // ---- CARTESIAN CONTINUOUS JOGGING (WITH USER FRAME IK) ----
    else {
        KDL::Frame current_user_cart = m_userFrame.Inverse() * cart;

        if (m_activeJogButton == "X+") current_user_cart.p.x(current_user_cart.p.x() + cStep);
        else if (m_activeJogButton == "X-") current_user_cart.p.x(current_user_cart.p.x() - cStep);
        else if (m_activeJogButton == "Y+") current_user_cart.p.y(current_user_cart.p.y() + cStep);
        else if (m_activeJogButton == "Y-") current_user_cart.p.y(current_user_cart.p.y() - cStep);
        else if (m_activeJogButton == "Z+") current_user_cart.p.z(current_user_cart.p.z() + cStep);
        else if (m_activeJogButton == "Z-") current_user_cart.p.z(current_user_cart.p.z() - cStep);

        // Convert back to Base Frame for IK Calculation
        KDL::Frame target_base_cart = m_userFrame * current_user_cart;

        KDL::JntArray target_joints(6);
        if (m_kinematics.Ik_Optimal_Solution(target_base_cart, KDLJointCur, target_joints)) {
            m_j1 = target_joints(0) * (180.0 / M_PI);
            m_j2 = target_joints(1) * (180.0 / M_PI);
            m_j3 = target_joints(2) * (180.0 / M_PI);
            m_j4 = target_joints(3) * (180.0 / M_PI);
            m_j5 = target_joints(4) * (180.0 / M_PI);
            m_j6 = target_joints(5) * (180.0 / M_PI);
        } else return;
    }

    // Update KDL
    KDLJointCur(0) = m_j1 * (M_PI / 180.0);
    KDLJointCur(1) = m_j2 * (M_PI / 180.0);
    KDLJointCur(2) = m_j3 * (M_PI / 180.0);
    KDLJointCur(3) = m_j4 * (M_PI / 180.0);
    KDLJointCur(4) = m_j5 * (M_PI / 180.0);
    KDLJointCur(5) = m_j6 * (M_PI / 180.0);

    m_kinematics.Fk();

    // ✅ UI UPDATE
    updateUIWithUserFrame();
}

// ========================================================
// ✅ MASTER UI UPDATE FUNCTION
// ========================================================
void ClientBackend::updateUIWithUserFrame()
{

    KDL::Frame user_cart = m_userFrame.Inverse() * cart;

    // 2. Joints UI Update
    setProperty("j1", QVariant(m_j1));
    setProperty("j2", QVariant(m_j2));
    setProperty("j3", QVariant(m_j3));
    setProperty("j4", QVariant(m_j4));
    setProperty("j5", QVariant(m_j5));
    setProperty("j6", QVariant(m_j6));

    // 3. User Frame XYZ Update
    setProperty("x", QVariant(user_cart.p.x()));
    setProperty("y", QVariant(user_cart.p.y()));
    setProperty("z", QVariant(user_cart.p.z()));

    // 4. User Frame ABC Update
    double a, b, c;
    user_cart.M.GetEulerZYX(a, b, c);
    setProperty("a", QVariant(a * (180.0 / M_PI)));
    setProperty("b", QVariant(b * (180.0 / M_PI)));
    setProperty("c", QVariant(c * (180.0 / M_PI)));

    emit updateRobot3DView(m_j1, m_j2, m_j3, m_j4, m_j5, m_j6);
    emit telemetryChanged();
}
// ========================================================
// ✅ STOP THE RUNNING PROGRAM
// ========================================================
void ClientBackend::stopDxfProgram()
{
    if (m_playbackTimer && m_playbackTimer->isActive()) {
        m_playbackTimer->stop();
        qDebug() << "⏹️ Auto-Run Program Stopped by User!";

        // Tell the UI to reset the button back to green!
        emit programFinished();
    }
}

// ========================================================
// ✅ OPTIMIZED AUTO-RUN PROGRAM (FAST & PERFECT POSITION)
// ========================================================
void ClientBackend::runDxfProgram(const QString &csvData)
{
    QStringList lines = csvData.split('\n', Qt::SkipEmptyParts);
    std::vector<scurve::point> pathvec;

    for (int i = 1; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        if (line.startsWith("---") || line.isEmpty()) continue;

        QStringList parts = line.split(',');
        if (parts.size() >= 3) {

            // ========================================================
            // ✅ FIX 1: THE 90-DEGREE OFFSET CORRECTION
            // OCCT-ல் இருந்து வரும் புள்ளிகளை KDL மூளைக்கு புரியும் படி மாற்றுகிறோம்!
            // ========================================================
            double occt_x = parts[0].toDouble();
            double occt_y = parts[1].toDouble();
            double occt_z = parts[2].toDouble();

            double kdl_x = -occt_y; // Y ஆக மாறுவது X ஆகிவிடும்
            double kdl_y =  occt_x; // X ஆக மாறுவது Y ஆகிவிடும்
            double kdl_z =  occt_z; // Z மாறாது

            pathvec.push_back({kdl_x, kdl_y, kdl_z});
        }
    }

    if (pathvec.size() < 2) return;

    // ========================================================
    // ✅ FIX 2: FAST & CRISP SPEED
    // ========================================================
    scurve trajectoryPlanner;
    double maxVel = 200.0; // 200 mm/s (ரோபோ வேகமாக வரையும்!)
    double maxAcc = 500.0;

    m_cartesianTrajectory = trajectoryPlanner.create_point_for_every_ms_path(maxVel, maxAcc, 0.0, 0.0, pathvec);

    m_isCartesianPlayback = true;
    m_playbackIndex = 0;
    m_playbackTimer->start(16); // 16ms = 60 FPS

    qDebug() << "🚀 Program Started! Points loaded:" << m_cartesianTrajectory.size();
}