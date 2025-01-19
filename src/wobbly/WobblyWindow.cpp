/*

Copyright (c) 2015, John Smith
Copyright (c) 2023, Setsugen no ao

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExpValidator>
#include <QScrollArea>
#include <QShortcut>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
#include <QClipboard>
#include <QDir>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "CombedFramesCollector.h"
#include "ProgressDialog.h"
#include "RandomStuff.h"
#include "ScrollArea.h"
#include "WobblyException.h"
#include "WobblyWindow.h"
#include "WobblyShared.h"

#include "string"


// To avoid duplicating the string literals passed to QSettings
#define KEY_STATE                           QStringLiteral("user_interface/state")
#define KEY_GEOMETRY                        QStringLiteral("user_interface/geometry")
#define KEY_FONT_SIZE                       QStringLiteral("user_interface/font_size")
#define KEY_OVERLAY_SIZE                    QStringLiteral("user_interface/overlay_size")
#define KEY_APPLICATION_STYLE               QStringLiteral("user_interface/application_style")
#define KEY_ASK_FOR_BOOKMARK_DESCRIPTION    QStringLiteral("user_interface/ask_for_bookmark_description")
#define KEY_COLORMATRIX                     QStringLiteral("user_interface/colormatrix")
#define KEY_MAXIMUM_CACHE_SIZE              QStringLiteral("user_interface/maximum_cache_size")
#define KEY_PRINT_DETAILS_ON_VIDEO          QStringLiteral("user_interface/print_details_on_video")
#define KEY_UNDO_STEPS                      QStringLiteral("user_interface/undo_steps")
#define KEY_NUMBER_OF_THUMBNAILS            QStringLiteral("user_interface/number_of_thumbnails")
#define KEY_THUMBNAIL_SIZE                  QStringLiteral("user_interface/thumbnail_size")
#define KEY_LAST_DIR                        QStringLiteral("user_interface/last_dir")
#define KEY_RECENT                          QStringLiteral("user_interface/recent%1")
#define KEY_KEYS                            QStringLiteral("user_interface/keys/")

#define KEY_COMPACT_PROJECT_FILES           QStringLiteral("projects/compact_project_files")
#define KEY_USE_RELATIVE_PATHS              QStringLiteral("projects/use_relative_paths")
#define KEY_DECIMATION_FUNCTION             QStringLiteral("projects/decimation_function")


struct CallbackData {
    WobblyWindow *window;
    VSNode *node;
    bool preview_node;
    const VSAPI *vsapi;

    CallbackData(WobblyWindow *_window, VSNode *_node, bool _preview_node, const VSAPI *_vsapi)
        : window(_window)
        , node(_node)
        , preview_node(_preview_node)
        , vsapi(_vsapi)
    {

    }
};


WobblyWindow::WobblyWindow()
    : QMainWindow()
    , splash_image(720, 480, QImage::Format_RGB32)
    , window_title(QStringLiteral("Wobbly IVTC Assistant v%1").arg(PACKAGE_VERSION))
    , match_pattern("cccnn")
    , decimation_pattern("kkkkd")
#ifdef _WIN32
    , settings(QApplication::applicationDirPath() + "/wobbly.ini", QSettings::IniFormat)
#endif
{
    createUI();

    readSettings();

    try {
        initialiseVapourSynth();
    } catch (WobblyException &e) {
        show();
        errorPopup(e.what());
        exit(1);
    }

    createPluginWindow();
}


void WobblyWindow::addRecentFile(const QString &path) {
    int index = -1;
    auto actions = recent_menu->actions();
    for (int i = 0; i < actions.size(); i++) {
        if (actions[i]->text().endsWith(path)) {
            index = i;
            break;
        }
    }

    if (index == 0) {
        return;
    } else if (index > 0) {
        recent_menu->removeAction(actions[index]);
        recent_menu->insertAction(actions[0], actions[index]);
    } else {
        QAction *recent = new QAction(QStringLiteral("&0. %1").arg(path), this);
        connect(recent, &QAction::triggered, recent_menu_signal_mapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
        recent_menu_signal_mapper->setMapping(recent, path);

        recent_menu->insertAction(actions.size() ? actions[0] : 0, recent);

        if (actions.size() == 10)
            recent_menu->removeAction(actions[9]);
    }

    actions = recent_menu->actions();
    for (int i = 0; i < actions.size(); i++) {
        QString text = actions[i]->text();
        text[1] = QChar('0' + i);
        actions[i]->setText(text);

        settings.setValue(KEY_RECENT.arg(i), text.mid(4));
    }
}


void WobblyWindow::readSettings() {
    if (settings.contains(KEY_STATE))
        restoreState(settings.value(KEY_STATE).toByteArray());

    if (settings.contains(KEY_GEOMETRY))
        restoreGeometry(settings.value(KEY_GEOMETRY).toByteArray());

    settings_font_spin->setValue(settings.value(KEY_FONT_SIZE, QApplication::font().pointSize()).toInt());

    overlay_size_spin->setValue(settings.value(KEY_OVERLAY_SIZE, 4).toInt());

    application_style_combo->setCurrentText(settings.value(KEY_APPLICATION_STYLE, "Dark").toString());

    settings_compact_projects_check->setChecked(settings.value(KEY_COMPACT_PROJECT_FILES, false).toBool());

    settings_use_relative_paths_check->setChecked(settings.value(KEY_USE_RELATIVE_PATHS, false).toBool());

    settings_bookmark_description_check->setChecked(settings.value(KEY_ASK_FOR_BOOKMARK_DESCRIPTION, true).toBool());

    settings_decimation_function_combo->setCurrentText(settings.value(KEY_DECIMATION_FUNCTION, "Auto").toString());

    settings_colormatrix_combo->setCurrentText(settings.value(KEY_COLORMATRIX, "BT 601").toString());

    settings_cache_spin->setValue(settings.value(KEY_MAXIMUM_CACHE_SIZE, 4096).toInt());

    settings_print_details_check->setChecked(settings.value(KEY_PRINT_DETAILS_ON_VIDEO, true).toBool());

    settings_undo_steps_spin->setValue(settings.value(KEY_UNDO_STEPS, 50).toInt());

    settings_num_thumbnails_spin->setValue(settings.value(KEY_NUMBER_OF_THUMBNAILS, 5).toInt());

    settings_thumbnail_size_dspin->setValue(settings.value(KEY_THUMBNAIL_SIZE, 15).toDouble());

    settings_shortcuts_table->setRowCount(shortcuts.size());
    for (size_t i = 0; i < shortcuts.size(); i++) {
        QString settings_key = KEY_KEYS + shortcuts[i].description;

        if (settings.contains(settings_key))
            shortcuts[i].keys = settings.value(settings_key).toString();

        QTableWidgetItem *item = new QTableWidgetItem(shortcuts[i].keys);
        settings_shortcuts_table->setItem(i, 0, item);

        item = new QTableWidgetItem(shortcuts[i].default_keys);
        settings_shortcuts_table->setItem(i, 1, item);

        item = new QTableWidgetItem(shortcuts[i].description);
        settings_shortcuts_table->setItem(i, 2, item);
    }
    settings_shortcuts_table->resizeColumnsToContents();

    QStringList recent;
    for (int i = 9; i >= 0; i--) {
        QString settings_key = KEY_RECENT.arg(i);
        if (settings.contains(settings_key))
            recent.push_back(settings.value(settings_key).toString());
    }
    for (int i = 0; i < recent.size(); i++)
        addRecentFile(recent[i]);
}


void WobblyWindow::writeSettings() {
    settings.setValue(KEY_STATE, saveState());

    settings.setValue(KEY_GEOMETRY, saveGeometry());
}


void WobblyWindow::errorPopup(const char *msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}


void WobblyWindow::closeEvent(QCloseEvent *event) {
    if (askToSaveIfModified() == QMessageBox::Cancel) {
        event->ignore();
        return;
    }

    writeSettings();

    cleanUpVapourSynth();

    if (project) {
        delete project;
        project = nullptr;
    }

    event->accept();
}


void WobblyWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}


void WobblyWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();

    int first_local = -1;

    for (int i = 0; i < urls.size(); i++)
        if (urls[i].isLocalFile()) {
            first_local = i;
            break;
        }

    if (first_local == -1)
        return;

    QString path = urls[first_local].toLocalFile();

    openFile(path);

    event->acceptProposedAction();
}


void WobblyWindow::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    // Shift+[ returns key() == Key_LeftBrace instead of Key_LeftBracket
    if ((key < Qt::Key_A || key > Qt::Key_Z) && key != Qt::Key_Up && key != Qt::Key_Down)
        mod &= ~Qt::ShiftModifier;

    QKeySequence sequence(mod | key);
    QString sequence_string = sequence.toString(QKeySequence::PortableText);
    //fprintf(stderr, "Sequence: '%s'\n", sequence_string.toUtf8().constData());

    for (size_t i = 0; i < shortcuts.size(); i++) {
        if (sequence_string == shortcuts[i].keys) {
            (this->*shortcuts[i].func)(); // This looks quite evil indeed.
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}


void WobblyWindow::wheelEvent(QWheelEvent *event) {
    if (event->buttons() == Qt::MouseButton::RightButton) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().ry() < 0)
            zoomOut();
    }

    QMainWindow::wheelEvent(event);
}


void WobblyWindow::createMenu() {
    QMenuBar *bar = menuBar();

    QMenu *p = bar->addMenu("&Project");

    undo_action = new QAction("Undo", this);
    undo_action->setEnabled(false);
    redo_action = new QAction("Redo", this);
    redo_action->setEnabled(false);

    connect(undo_action, &QAction::triggered, this, &WobblyWindow::undo);
    connect(redo_action, &QAction::triggered, this, &WobblyWindow::redo);

    p->addAction(undo_action);
    p->addAction(redo_action);
    p->addSeparator();

    struct Menu {
        const char *name;
        void (WobblyWindow::* func)();
    };

    std::vector<Menu> project_menu = {
        { "&Open project",              &WobblyWindow::openProject },
        { "Open video",                 &WobblyWindow::openVideo },
        { "&Save project",              &WobblyWindow::saveProject },
        { "Save project &as",           &WobblyWindow::saveProjectAs },
        { "Save script",                &WobblyWindow::saveScript },
        { "Save script as",             &WobblyWindow::saveScriptAs },
        { "Save timecodes",             &WobblyWindow::saveTimecodes },
        { "Save timecodes as",          &WobblyWindow::saveTimecodesAs },
        { "Save sections",              &WobblyWindow::saveSections },
        { "Save sections as",           &WobblyWindow::saveSectionsAs },
        { "Save screenshot",            &WobblyWindow::saveScreenshot },
        { "Import from project",        &WobblyWindow::importFromProject },
        { nullptr,                      nullptr },
        { "&Recently opened",           nullptr },
        { nullptr,                      nullptr },
        { "&Quit",                      &WobblyWindow::quit }
    };

    for (size_t i = 0; i < project_menu.size(); i++) {
        if (project_menu[i].name && project_menu[i].func) {
            QAction *action = new QAction(project_menu[i].name, this);
            connect(action, &QAction::triggered, this, project_menu[i].func);
            p->addAction(action);
        } else if (project_menu[i].name && !project_menu[i].func) {
            // Not very nicely done.
            recent_menu = p->addMenu(project_menu[i].name);
        } else {
            p->addSeparator();
        }
    }

    recent_menu_signal_mapper = new QSignalMapper(this);

    connect(recent_menu_signal_mapper, static_cast<void (QSignalMapper::*)(const QString &)>(&QSignalMapper::mappedString), [this] (const QString &path) {
        if (askToSaveIfModified() == QMessageBox::Cancel)
            return;

        openFile(path);
    });


    tools_menu = bar->addMenu("&Tools");


    QMenu *h = bar->addMenu("&Help");

    QAction *helpAbout = new QAction("About", this);
    QAction *helpAboutQt = new QAction("About Qt", this);

    connect(helpAbout, &QAction::triggered, [this] () {
        QMessageBox::about(this, QStringLiteral("About Wobbly"), QStringLiteral(
            "<a href='https://github.com/Jaded-Encoding-Thaumaturgy/Wobbly'>https://github.com/Jaded-Encoding-Thaumaturgy/Wobbly</a><br />"
            "<br />"
            "Copyright (c) 2015, John Smith<br />"
            "Copyright (c) 2023, Setsugen no ao<br />"
            "<br />"
            "Permission to use, copy, modify, and/or distribute this software for "
            "any purpose with or without fee is hereby granted, provided that the "
            "above copyright notice and this permission notice appear in all copies.<br />"
            "<br />"
            "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL "
            "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED "
            "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR "
            "BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES "
            "OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, "
            "WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, "
            "ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS "
            "SOFTWARE."
        ));
    });

    connect(helpAboutQt, &QAction::triggered, [this] () {
        QMessageBox::aboutQt(this);
    });

    h->addAction(helpAbout);
    h->addAction(helpAboutQt);
}


void WobblyWindow::createShortcuts() {
    // Do not use '/' or '\' in the descriptions. They are special in QSettings.
    shortcuts = {
        { "", "",                   "Open project", &WobblyWindow::openProject },
        { "", "",                   "Open video", &WobblyWindow::openVideo },
        { "", "",                   "Save project", &WobblyWindow::saveProject },
        { "", "",                   "Save project as", &WobblyWindow::saveProjectAs },
        { "", "",                   "Save script", &WobblyWindow::saveScript },
        { "", "",                   "Save script as", &WobblyWindow::saveScriptAs },
        { "", "",                   "Save timecodes", &WobblyWindow::saveTimecodes },
        { "", "",                   "Save timecodes as", &WobblyWindow::saveTimecodesAs },
        { "", "",                   "Save screenshot", &WobblyWindow::saveScreenshot },
        { "", "",                   "Import from project", &WobblyWindow::importFromProject },
        { "", "Ctrl+Z",             "Undo", &WobblyWindow::undo },
        { "", "Ctrl+Y",             "Redo", &WobblyWindow::redo },
        { "", "",                   "Quit", &WobblyWindow::quit },

        { "", "",                   "Show or hide frame details", &WobblyWindow::showHideFrameDetails },
        { "", "",                   "Show or hide cropping", &WobblyWindow::showHideCropping },
        { "", "",                   "Show or hide presets", &WobblyWindow::showHidePresets },
        { "", "",                   "Show or hide pattern editor", &WobblyWindow::showHidePatternEditor },
        { "", "",                   "Show or hide sections", &WobblyWindow::showHideSections },
        { "", "",                   "Show or hide custom lists", &WobblyWindow::showHideCustomLists },
        { "", "",                   "Show or hide frame rates", &WobblyWindow::showHideFrameRates },
        { "", "",                   "Show or hide frozen frames", &WobblyWindow::showHideFrozenFrames },
        { "", "",                   "Show or hide pattern guessing", &WobblyWindow::showHidePatternGuessing },
        { "", "",                   "Show or hide mic search", &WobblyWindow::showHideMicSearchWindow },
        { "", "",                   "Show or hide C match sequences window", &WobblyWindow::showHideCMatchSequencesWindow },
        { "", "",                   "Show or hide interlaced fades window", &WobblyWindow::showHideFadesWindow },
        { "", "",                   "Show or hide combed frames window", &WobblyWindow::showHideCombedFramesWindow },
        { "", "",                   "Show or hide orphan fields window", &WobblyWindow::showHideOrphanFieldsWindow },
        { "", "",                   "Show or hide bookmarks window", &WobblyWindow::showHideBookmarksWindow },

        { "", "",                   "Show or hide frame details printed on the video", &WobblyWindow::showHideFrameDetailsOnVideo },

        { "", "Left",               "Jump 1 frame back", &WobblyWindow::jump1Backward },
        { "", "Right",              "Jump 1 frame forward", &WobblyWindow::jump1Forward },
        { "", "Ctrl+Left",          "Jump 5 frames back", &WobblyWindow::jump5Backward },
        { "", "Ctrl+Right",         "Jump 5 frames forward", &WobblyWindow::jump5Forward },
        { "", "Alt+Left",           "Jump 50 frames back", &WobblyWindow::jump50Backward },
        { "", "Alt+Right",          "Jump 50 frames forward", &WobblyWindow::jump50Forward },
        { "", "Home",               "Jump to first frame", &WobblyWindow::jumpToStart },
        { "", "End",                "Jump to last frame", &WobblyWindow::jumpToEnd },
        { "", "PgDown",             "Jump 20% back", &WobblyWindow::jumpALotBackward },
        { "", "PgUp",               "Jump 20% forward", &WobblyWindow::jumpALotForward },
        { "", "Ctrl+Up",            "Jump to next section start", &WobblyWindow::jumpToNextSectionStart },
        { "", "Ctrl+Down",          "Jump to previous section start", &WobblyWindow::jumpToPreviousSectionStart },
        { "", "Ctrl+Shift+Up",      "Jump to next frame with high mic", &WobblyWindow::jumpToNextMic },
        { "", "Ctrl+Shift+Down",    "Jump to previous frame with high mic", &WobblyWindow::jumpToPreviousMic },
        { "", "Up",                 "Jump to next frame with high dmetric", &WobblyWindow::jumpToNextDMetric },
        { "", "Down",               "Jump to previous frame with high dmetric", &WobblyWindow::jumpToPreviousDMetric },
        { "", "<",                  "Jump to previous bookmark", &WobblyWindow::jumpToPreviousBookmark },
        { "", ">",                  "Jump to next bookmark", &WobblyWindow::jumpToNextBookmark },
        { "", "G",                  "Jump to specific frame", &WobblyWindow::jumpToFrame },
        { "", "Shift+Up",           "Jump to next combed frame", &WobblyWindow::jumpToNextCombedFrame },
        { "", "Shift+Down",         "Jump to previous combed frame", &WobblyWindow::jumpToPreviousCombedFrame },
        { "", "Alt+Up",             "Jump to next section with pattern failure", &WobblyWindow::jumpToNextPatternFailureSection },
        { "", "Alt+Down",           "Jump to previous section with pattern failure", &WobblyWindow::jumpToPreviousPatternFailureSection },
        { "", "S",                  "Cycle the current frame's match", &WobblyWindow::cycleMatchCNB },
        { "", "Ctrl+F",             "Replace current frame with next", &WobblyWindow::freezeForward },
        { "", "Shift+F",            "Replace current frame with previous", &WobblyWindow::freezeBackward },
        { "", "F",                  "Freeze current frame or a range", &WobblyWindow::freezeRange },
        { "", "Q",                  "Delete freezeframe", &WobblyWindow::deleteFreezeFrame },
        { "", "",                   "Toggle all freezeframes in the source tab", &WobblyWindow::toggleFreezeFrames },
        { "", "D",                  "Toggle decimation for the current frame", &WobblyWindow::toggleDecimation },
        { "", "I",                  "Start new section at current frame", &WobblyWindow::addSection },
        { "", "Ctrl+Q",             "Delete current section", &WobblyWindow::deleteSection },
        { "", "P",                  "Toggle postprocessing for the current frame or a range", &WobblyWindow::toggleCombed },
        { "", "B",                  "Toggle bookmark", &WobblyWindow::toggleBookmark },
        { "", "R",                  "Reset the match(es) for the current frame or a range", &WobblyWindow::resetMatch },
        { "", "Ctrl+R",             "Reset the matches for the current section", &WobblyWindow::resetSection },
        { "", "Ctrl+S",             "Rotate the patterns and apply them to the current section", &WobblyWindow::rotateAndSetPatterns },
        { "", "",                   "Set match pattern to range", &WobblyWindow::setMatchPattern },
        { "", "",                   "Set decimation pattern to range", &WobblyWindow::setDecimationPattern },
        { "", "",                   "Set match and decimation patterns to range", &WobblyWindow::setMatchAndDecimationPatterns },
        { "", "F5",                 "Toggle preview mode", &WobblyWindow::togglePreview },
        { "", "Ctrl+Num++",         "Zoom in", &WobblyWindow::zoomIn },
        { "", "Ctrl+Num+-",         "Zoom out", &WobblyWindow::zoomOut },
        { "", "",                   "Guess current section's patterns from matches", &WobblyWindow::guessCurrentSectionPatternsFromMatches },
        { "", "",                   "Guess every section's patterns from matches", &WobblyWindow::guessProjectPatternsFromMatches },
        { "", "Ctrl+Alt+G",         "Guess current section's patterns from mics", &WobblyWindow::guessCurrentSectionPatternsFromMics },
        { "", "Ctrl+Alt+H",         "Guess current section's patterns from dmetrics", &WobblyWindow::guessCurrentSectionPatternsFromDMetrics },
        { "", "Ctrl+Alt+J",         "Guess current section's patterns from mics and dmetrics", &WobblyWindow::guessCurrentSectionPatternsFromMicsAndDMetrics },
        { "", "",                   "Guess every section's patterns from mics", &WobblyWindow::guessProjectPatternsFromMics },
        { "", "",                   "Guess every section's patterns from dmetrics", &WobblyWindow::guessProjectPatternsFromDMetrics },
        { "", "E",                  "Start a range", &WobblyWindow::startRange },
        { "", "Escape",             "Cancel a range", &WobblyWindow::cancelRange },
        { "", "",                   "Select the previous preset", &WobblyWindow::selectPreviousPreset },
        { "", "H",                  "Select the next preset", &WobblyWindow::selectNextPreset },
        { "", "",                   "Assign selected preset to the current section", &WobblyWindow::assignSelectedPresetToCurrentSection },
        { "", "Z",                  "Select the previous custom list", &WobblyWindow::selectPreviousCustomList },
        { "", "X",                  "Select the next custom list", &WobblyWindow::selectNextCustomList },
        { "", "C",                  "Add range to the selected custom list", &WobblyWindow::addRangeToSelectedCustomList },

        { "", "Ctrl+C",             "Copy current frame number to clipboard", &WobblyWindow::copyCurrentFrameNumberToClipboard },
        { "", "Alt+C",              "Copy current frame image to clipboard", &WobblyWindow::copyCurrentFrameImageToClipboard }
    };

    resetShortcuts();
}


void WobblyWindow::resetShortcuts() {
    for (size_t i = 0; i < shortcuts.size(); i++)
        shortcuts[i].keys = shortcuts[i].default_keys;
}


void WobblyWindow::createFrameDetailsViewer() {
    frame_num_label = new QLabel;
    frame_num_label->setTextFormat(Qt::RichText);
    time_label = new QLabel;
    matches_label = new QLabel;
    matches_label->setTextFormat(Qt::RichText);
    matches_label->resize(QFontMetrics(matches_label->font()).horizontalAdvance("CCCCCCCCCCCCCCCCCCCCC"), matches_label->height());
    section_label = new QLabel;
    section_label->setTextFormat(Qt::RichText);
    custom_list_label = new QLabel;
    custom_list_label->setTextFormat(Qt::RichText);
    freeze_label = new QLabel;
    decimate_metric_label = new QLabel;
    mic_label = new QLabel;
    mic_label->setTextFormat(Qt::RichText);
    mmetric_label = new QLabel;
    mmetric_label->setTextFormat(Qt::RichText);
    vmetric_label = new QLabel;
    vmetric_label->setTextFormat(Qt::RichText);
    pict_type_label = new QLabel;
    combed_label = new QLabel;
    bookmark_label = new QLabel;
    bookmark_label->setWordWrap(true);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frame_num_label);
    vbox->addWidget(time_label);
    vbox->addWidget(matches_label);
    vbox->addWidget(section_label);
    vbox->addWidget(custom_list_label);
    vbox->addWidget(freeze_label);
    vbox->addWidget(decimate_metric_label);
    vbox->addWidget(mmetric_label);
    vbox->addWidget(vmetric_label);
    vbox->addWidget(mic_label);
    vbox->addWidget(pict_type_label);
    vbox->addWidget(combed_label);
    vbox->addWidget(bookmark_label);
    vbox->addStretch(1);

    QWidget *details_widget = new QWidget;
    details_widget->setLayout(vbox);

    details_dock = new DockWidget("Frame details", this);
    details_dock->setObjectName("frame details");
    details_dock->setVisible(false);
    details_dock->setFloating(true);
    details_dock->setWidget(details_widget);
    addDockWidget(Qt::LeftDockWidgetArea, details_dock);
    tools_menu->addAction(details_dock->toggleViewAction());
    connect(details_dock, &QDockWidget::visibilityChanged, details_dock, &QDockWidget::setEnabled);
}


void WobblyWindow::createCropAssistant() {
    // Crop.
    crop_box = new QGroupBox(QStringLiteral("Crop"));
    crop_box->setCheckable(true);
    crop_box->setChecked(true);

    const char *crop_prefixes[4] = {
        "Left: ",
        "Top: ",
        "Right: ",
        "Bottom: "
    };

    for (int i = 0; i < 4; i++) {
        crop_spin[i] = new QSpinBox;
        crop_spin[i]->setRange(0, 99999);
        crop_spin[i]->setPrefix(crop_prefixes[i]);
        crop_spin[i]->setSuffix(QStringLiteral(" px"));
    }

    crop_early_check = new QCheckBox("Crop early");

    const char *resize_prefixes[2] = {
        "Width: ",
        "Height: "
    };

    // Resize.
    resize_box = new QGroupBox(QStringLiteral("Resize"));
    resize_box->setCheckable(true);
    resize_box->setChecked(false);

    for (int i = 0; i < 2; i++) {
        resize_spin[i] = new QSpinBox;
        resize_spin[i]->setRange(16, 999999);
        resize_spin[i]->setPrefix(resize_prefixes[i]);
        resize_spin[i]->setSuffix(QStringLiteral(" px"));
    }

    resize_filter_combo = new QComboBox;
    resize_filter_combo->addItems({
                                      "Point",
                                      "Bilinear",
                                      "Bicubic",
                                      "Spline16",
                                      "Spline36",
                                      "Lanczos"
                                  });
    resize_filter_combo->setCurrentIndex(3);

    // Bit depth.
    depth_box = new QGroupBox(QStringLiteral("Bit depth"));
    depth_box->setCheckable(true);
    depth_box->setChecked(false);

    depth_bits_combo = new QComboBox;
    depth_bits_combo->addItems({
                                   "8 bits",
                                   "9 bits",
                                   "10 bits",
                                   "12 bits",
                                   "16 bits, integer",
                                   "16 bits, float",
                                   "32 bits, float"
                               });
    depth_bits_combo->setCurrentIndex(0);

    depth_dither_combo = new QComboBox;
    depth_dither_combo->addItems({
                                     "No dithering",
                                     "Ordered dithering",
                                     "Random dithering",
                                     "Error diffusion"
                                 });
    depth_dither_combo->setCurrentIndex(2);


    // Crop.
    connect(crop_box, &QGroupBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        project->setCropEnabled(checked);

        try {
            evaluateScript(preview);
        } catch (WobblyException &) {

        }
    });

    auto cropChanged = [this] () {
        if (!project)
            return;

        project->setCrop(crop_spin[0]->value(), crop_spin[1]->value(), crop_spin[2]->value(), crop_spin[3]->value());

        try {
            evaluateScript(preview);
        } catch (WobblyException &) {

        }
    };
    for (int i = 0; i < 4; i++)
        connect(crop_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), cropChanged);

    connect(crop_early_check, &QCheckBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        project->setCropEarly(checked);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });

    // Resize.
    connect(resize_box, &QGroupBox::toggled, [this] (bool checked) {
        if (!project)
            return;

        project->setResizeEnabled(checked);

        if (preview && resize_spin[0]->value() % 2 == 0 && resize_spin[1]->value() % 2 == 0) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });

    auto resizeChanged = [this] () {
        if (!project)
            return;

        project->setResize(resize_spin[0]->value(), resize_spin[1]->value(), resize_filter_combo->currentText().toLower().toStdString());

        if (preview && resize_spin[0]->value() % 2 == 0 && resize_spin[1]->value() % 2 == 0) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    };
    for (int i = 0; i < 2; i++)
        connect(resize_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), resizeChanged);

    connect(resize_filter_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::textActivated), [this] (const QString &text) {
        if (!project)
            return;

        project->setResize(resize_spin[0]->value(), resize_spin[1]->value(), text.toLower().toStdString());

        if (preview && resize_spin[0]->value() % 2 == 0 && resize_spin[1]->value() % 2 == 0) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });

    // Bit depth.
    int index_to_bits[] = { 8, 9, 10, 12, 16, 16, 32 };
    bool index_to_float_samples[] = { false, false, false, false, false, true, true };
    const char *index_to_dither[] = { "none", "ordered", "random", "error_diffusion" };

    connect(depth_box, &QGroupBox::toggled, [this, index_to_bits, index_to_float_samples, index_to_dither] (bool checked) {
        if (!project)
            return;

        int bits_index = depth_bits_combo->currentIndex();
        int dither_index = depth_dither_combo->currentIndex();

        project->setBitDepth(index_to_bits[bits_index], index_to_float_samples[bits_index], index_to_dither[dither_index]);
        project->setBitDepthEnabled(checked);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });

    connect(depth_bits_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this, index_to_bits, index_to_float_samples, index_to_dither] (int index) {
        if (!project)
            return;

        int dither_index = depth_dither_combo->currentIndex();

        project->setBitDepth(index_to_bits[index], index_to_float_samples[index], index_to_dither[dither_index]);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });

    connect(depth_dither_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this, index_to_bits, index_to_float_samples, index_to_dither] (int index) {
        if (!project)
            return;

        int bits_index = depth_bits_combo->currentIndex();

        project->setBitDepth(index_to_bits[bits_index], index_to_float_samples[bits_index], index_to_dither[index]);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &) {

            }
        }
    });


    // Crop.
    QVBoxLayout *vbox = new QVBoxLayout;
    for (int i = 0; i < 4; i++) {
        vbox->addWidget(crop_spin[i]);
    }
    vbox->addWidget(crop_early_check);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    crop_box->setLayout(hbox);

    // Resize.
    vbox = new QVBoxLayout;
    for (int i = 0; i < 2; i++) {
        vbox->addWidget(resize_spin[i]);
    }
    vbox->addWidget(resize_filter_combo);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    resize_box->setLayout(hbox);

    // Bit depth.
    vbox = new QVBoxLayout;
    vbox->addWidget(depth_bits_combo);
    vbox->addWidget(depth_dither_combo);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    depth_box->setLayout(hbox);


    vbox = new QVBoxLayout;
    vbox->addWidget(crop_box);
    vbox->addWidget(resize_box);
    vbox->addWidget(depth_box);
    vbox->addStretch(1);


    QWidget *crop_widget = new QWidget;
    crop_widget->setLayout(vbox);

    crop_dock = new DockWidget("Cropping, resizing, bit depth", this);
    crop_dock->setObjectName("crop assistant");
    crop_dock->setVisible(false);
    crop_dock->setFloating(true);
    crop_dock->setWidget(crop_widget);
    addDockWidget(Qt::RightDockWidgetArea, crop_dock);
    tools_menu->addAction(crop_dock->toggleViewAction());
    connect(crop_dock, &DockWidget::visibilityChanged, crop_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPresetEditor() {
    preset_combo = new QComboBox;

    preset_edit = new PresetTextEdit;
    preset_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    preset_edit->setTabChangesFocus(true);
    preset_edit->setToolTip(QStringLiteral(
                "The preset is a Python function. It takes a single parameter, called 'clip'.\n"
                "Filter that and assign the result to the same variable.\n"
                "The return statement will be added automatically.\n"
                "The VapourSynth core object is called 'c'."
    ));

    QPushButton *new_button = new QPushButton(QStringLiteral("New"));
    QPushButton *rename_button = new QPushButton(QStringLiteral("Rename"));
    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));

    connect(preset_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::textActivated), this, &WobblyWindow::presetChanged);

    connect(preset_edit, &PresetTextEdit::focusLost, this, &WobblyWindow::presetEdited);

    connect(new_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        bool ok = false;
        QString preset_name;

        while (!ok) {
            preset_name = QInputDialog::getText(this, QStringLiteral("New preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_name);

            if (!preset_name.isEmpty()) {
                try {
                    int selected_index = getSelectedPreset();

                    // The "selected" preset has nothing to do with what preset is currently displayed in preset_combo.
                    QString selected;
                    if (selected_index > -1)
                        selected = preset_combo->itemText(selected_index);

                    project->addPreset(preset_name.toStdString());
                    commit("Add preset");

                    if (selected_index > -1)
                        setSelectedPreset(preset_combo->findText(selected));


                    preset_combo->setCurrentText(preset_name);

                    presetChanged(preset_name);

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(rename_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        if (preset_combo->currentIndex() == -1)
            return;

        bool ok = false;
        QString preset_name = preset_combo->currentText();

        while (!ok) {
            preset_name = QInputDialog::getText(this, QStringLiteral("Rename preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_name);

            if (!preset_name.isEmpty() && preset_name != preset_combo->currentText()) {
                try {
                    project->renamePreset(preset_combo->currentText().toStdString(), preset_name.toStdString());
                    commit("Rename preset");

                    int index = preset_combo->findText(preset_name);
                    setSelectedPreset(index);

                    preset_combo->setCurrentText(preset_name);

                    updateFrameDetails();

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        int index = preset_combo->currentIndex();
        if (index == -1)
            return;

        const std::string &preset = preset_combo->currentText().toStdString();

        bool preset_in_use = project->isPresetInUse(preset);

        if (preset_in_use) {
            QMessageBox::StandardButton answer = QMessageBox::question(this, QStringLiteral("Delete preset?"), QStringLiteral("Preset '%1' is in use. Delete anyway?").arg(preset.c_str()), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (answer == QMessageBox::No)
                return;
        }

        project->deletePreset(preset);
        commit("Delete preset");

        setSelectedPreset(selected_preset);

        if (preset_combo->count()) {
            index = std::min(index, preset_combo->count() - 1);
            preset_combo->setCurrentIndex(index);
        }
        presetChanged(preset_combo->currentText());
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new_button);
    hbox->addWidget(rename_button);
    hbox->addWidget(delete_button);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(preset_combo);
    vbox->addWidget(preset_edit);
    vbox->addLayout(hbox);

    QWidget *preset_widget = new QWidget;
    preset_widget->setLayout(vbox);


    preset_dock = new DockWidget("Presets", this);
    preset_dock->setObjectName("preset editor");
    preset_dock->setVisible(false);
    preset_dock->setFloating(true);
    preset_dock->setWidget(preset_widget);
    addDockWidget(Qt::RightDockWidgetArea, preset_dock);
    tools_menu->addAction(preset_dock->toggleViewAction());
    connect(preset_dock, &DockWidget::visibilityChanged, preset_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPatternEditor() {
    match_pattern_edit = new QLineEdit(match_pattern);
    decimation_pattern_edit = new QLineEdit(decimation_pattern);

    QRegExpValidator *match_validator = new QRegExpValidator(QRegExp("[pbcnu]{5,}"), this);
    QRegExpValidator *decimation_validator = new QRegExpValidator(QRegExp("[dk]{5,}"), this);

    match_pattern_edit->setValidator(match_validator);
    decimation_pattern_edit->setValidator(decimation_validator);

    QPushButton *fill_24fps_button = new QPushButton(QStringLiteral("&24 fps"));
    QPushButton *fill_30fps_button = new QPushButton(QStringLiteral("&30 fps"));


    connect(match_pattern_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        match_pattern = text;
    });

    connect(decimation_pattern_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        decimation_pattern = text;
    });

    connect(fill_24fps_button, &QPushButton::clicked, [this] () {
        match_pattern = "cccnn";
        match_pattern_edit->setText(match_pattern);

        decimation_pattern = "kkkkd";
        decimation_pattern_edit->setText(decimation_pattern);
    });

    connect(fill_30fps_button, &QPushButton::clicked, [this] () {
        match_pattern = "ccccc";
        match_pattern_edit->setText(match_pattern);

        decimation_pattern = "kkkkk";
        decimation_pattern_edit->setText(decimation_pattern);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel(QStringLiteral("Match pattern:")));
    vbox->addWidget(match_pattern_edit);
    vbox->addWidget(new QLabel(QStringLiteral("Decimation pattern:")));
    vbox->addWidget(decimation_pattern_edit);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(fill_24fps_button);
    hbox->addWidget(fill_30fps_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);
    vbox->addStretch(1);

    QWidget *pattern_widget = new QWidget;
    pattern_widget->setLayout(vbox);


    pattern_dock = new DockWidget("Pattern editor", this);
    pattern_dock->setObjectName("pattern editor");
    pattern_dock->setVisible(false);
    pattern_dock->setFloating(true);
    pattern_dock->setWidget(pattern_widget);
    addDockWidget(Qt::RightDockWidgetArea, pattern_dock);
    tools_menu->addAction(pattern_dock->toggleViewAction());
    connect(pattern_dock, &DockWidget::visibilityChanged, pattern_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createSectionsEditor() {
    sections_view = new TableView;
    sections_proxy_model = new SectionsProxyModel(this);
    sections_view->setModel(sections_proxy_model);

    QPushButton *delete_sections_button = new QPushButton("Delete");

    short_sections_box = new QGroupBox("Show only short sections");
    short_sections_box->setCheckable(true);
    short_sections_box->setChecked(false);

    short_sections_spin = new QSpinBox;
    short_sections_spin->setValue(10);
    short_sections_spin->setPrefix(QStringLiteral("Maximum: "));
    short_sections_spin->setSuffix(QStringLiteral(" frames"));

    section_presets_list = new ListWidget;
    section_presets_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    section_presets_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QPushButton *move_preset_up_button = new QPushButton("Move up");
    QPushButton *move_preset_down_button = new QPushButton("Move down");
    QPushButton *remove_preset_button = new QPushButton("Remove");

    preset_list = new QListView;
    preset_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preset_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QPushButton *append_presets_button = new QPushButton("Append");


    connect(sections_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        bool ok;
        int frame = sections_view->model()->data(sections_view->model()->index(index.row(), 0)).toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(sections_view, &TableView::deletePressed, delete_sections_button, &QPushButton::click);

    connect(delete_sections_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = sections_view->selectionModel()->selectedRows();

        if (!selection.size())
            return;

        // Can't use the model indexes after modifying the model.
        std::vector<Section> sections;
        sections.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++)
            sections.push_back(Section(*project->findSection(selection[i].data().toInt())));

        for (size_t i = 0; i < sections.size(); i++)
            if (sections[i].start != 0)
                project->deleteSection(sections[i].start);
        commit("Delete section(s)");

        bool update_needed = false;

        for (size_t i = 0; i < sections.size(); i++) {
            if (project->findSection(sections[i].start)->presets != sections[i].presets) {
                update_needed = true;
                break;
            }
        }

        if (sections_view->model()->rowCount())
            sections_view->selectRow(sections_view->currentIndex().row());

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(short_sections_box, &QGroupBox::clicked, [this] (bool checked) {
        if (!project)
            return;

        short_sections_spin->valueChanged(short_sections_spin->value());

        sections_proxy_model->setHideSections(checked);
    });

    connect(short_sections_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        std::unordered_set<int> long_sections;

        const Section *section = project->findSection(0);
        while (section) {
            int section_length = project->getSectionEnd(section->start) - section->start;

            if (section_length > value)
                long_sections.insert(section->start);

            section = project->findNextSection(section->start);
        }

        sections_proxy_model->setHiddenSections(long_sections);
    });

    connect(section_presets_list, &ListWidget::deletePressed, remove_preset_button, &QPushButton::click);

    connect(move_preset_up_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QList<QListWidgetItem *> selected_presets = section_presets_list->selectedItems();

        if (!selected_presets.size())
            return;

        int sections_row = sections_view->currentIndex().row();
        bool ok;
        int section_start = sections_view->model()->data(sections_view->model()->index(sections_row, 0)).toInt(&ok);

        if (!ok)
            return;

        std::vector<int> preset_indexes;
        preset_indexes.reserve(selected_presets.size());

        for (int i = 0; i < selected_presets.size(); i++)
            preset_indexes.push_back(section_presets_list->row(selected_presets[i]));

        if (preset_indexes[0] == 0)
            return;

        for (size_t i = 0; i < preset_indexes.size(); i++)
            project->moveSectionPresetUp(section_start, preset_indexes[i]);
        commit("Move section preset");

        for (size_t i = 0; i < preset_indexes.size(); i++)
            section_presets_list->item(preset_indexes[i] - 1)->setSelected(true);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(move_preset_down_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QList<QListWidgetItem *> selected_presets = section_presets_list->selectedItems();

        if (!selected_presets.size())
            return;

        int sections_row = sections_view->currentIndex().row();
        bool ok;
        int section_start = sections_view->model()->data(sections_view->model()->index(sections_row, 0)).toInt(&ok);

        if (!ok)
            return;

        std::vector<int> preset_indexes;
        preset_indexes.reserve(selected_presets.size());

        for (int i = 0; i < selected_presets.size(); i++)
            preset_indexes.push_back(section_presets_list->row(selected_presets[i]));

        if (preset_indexes.back() == section_presets_list->count() - 1)
            return;

        for (int i = preset_indexes.size() - 1; i >= 0; i--)
            project->moveSectionPresetDown(section_start, preset_indexes[i]);
        commit("Move section preset");

        for (size_t i = 0; i < preset_indexes.size(); i++)
            section_presets_list->item(preset_indexes[i] + 1)->setSelected(true);

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(remove_preset_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QList<QListWidgetItem *> selected_presets = section_presets_list->selectedItems();

        if (!selected_presets.size())
            return;

        int sections_row = sections_view->currentIndex().row();
        bool ok;
        int section_start = sections_view->model()->data(sections_view->model()->index(sections_row, 0)).toInt(&ok);

        if (!ok)
            return;

        std::vector<int> preset_indexes;
        preset_indexes.reserve(selected_presets.size());

        for (int i = 0; i < selected_presets.size(); i++)
            preset_indexes.push_back(section_presets_list->row(selected_presets[i]));

        for (int i = preset_indexes.size() - 1; i >= 0; i--)
            project->deleteSectionPreset(section_start, preset_indexes[i]);
        commit("Remove preset from section");

        if (preview) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(append_presets_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selected_presets = preset_list->selectionModel()->selectedRows();
        QModelIndexList selected_sections = sections_view->selectionModel()->selectedRows();

        if (selected_presets.size()) {
            for (auto section = selected_sections.cbegin(); section != selected_sections.cend(); section++)
                for (auto preset = selected_presets.cbegin(); preset != selected_presets.cend(); preset++) {
                    bool ok;
                    int section_start = section->data().toInt(&ok);
                    if (ok)
                        project->setSectionPreset(section_start, preset->data().toString().toStdString());
                }
            commit("Add preset to section");

            if (selected_sections.size()) {
                if (preview) {
                    try {
                        evaluateFinalScript();
                    } catch (WobblyException &e) {
                        errorPopup(e.what());

                        togglePreview();
                    }
                }

                updateFrameDetails();
            }
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Sections:"));
    vbox->addWidget(sections_view, 1);

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(delete_sections_button);
    vbox2->addStretch(1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(short_sections_spin);
    hbox->addStretch(1);
    short_sections_box->setLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);
    hbox->addWidget(short_sections_box);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QHBoxLayout *hbox2 = new QHBoxLayout;
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Section's presets:"));
    vbox->addWidget(section_presets_list, 1);

    vbox2 = new QVBoxLayout;
    vbox2->addWidget(move_preset_up_button);
    vbox2->addWidget(move_preset_down_button);
    vbox2->addWidget(remove_preset_button);
    hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);
    hbox->addStretch(1);
    vbox->addLayout(hbox);
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Available presets:"));
    vbox->addWidget(preset_list, 1);

    hbox = new QHBoxLayout;
    hbox->addWidget(append_presets_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);
    hbox2->addLayout(vbox);


    QWidget *sections_widget = new QWidget;
    sections_widget->setLayout(hbox2);


    sections_dock = new DockWidget("Sections", this);
    sections_dock->setObjectName("sections editor");
    sections_dock->setVisible(false);
    sections_dock->setFloating(true);
    sections_dock->setWidget(sections_widget);
    addDockWidget(Qt::RightDockWidgetArea, sections_dock);
    tools_menu->addAction(sections_dock->toggleViewAction());
    connect(sections_dock, &DockWidget::visibilityChanged, sections_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createCustomListsEditor() {
    cl_view = new TableView;


    QPushButton *cl_new_button = new QPushButton("New");
    QPushButton *cl_rename_button = new QPushButton("Rename");
    QPushButton *cl_delete_button = new QPushButton("Delete");
    QPushButton *cl_move_up_button = new QPushButton("Move up");
    QPushButton *cl_move_down_button = new QPushButton("Move down");


    cl_presets_box = new QComboBox;


    QGroupBox *cl_position_box = new QGroupBox("Position in the filter chain");

    const char *positions[] = {
        "Post source",
        "Post field match",
        "Post decimate"
    };

    cl_position_group = new QButtonGroup(this);
    for (int i = 0; i < 3; i++)
        cl_position_group->addButton(new QRadioButton(positions[i]), i);
    cl_position_group->button(PostSource)->setChecked(true);


    cl_ranges_view = new TableView;


    QPushButton *cl_delete_range_button = new QPushButton("Delete");

    QPushButton *cl_send_range_button = new QPushButton("Send to list");
    QMenu *cl_send_range_menu = new QMenu(this);
    cl_send_range_button->setMenu(cl_send_range_menu);

    QPushButton *cl_copy_range_button = new QPushButton("Copy to list");
    QMenu *cl_copy_range_menu = new QMenu(this);
    cl_copy_range_button->setMenu(cl_copy_range_menu);


    connect(cl_view, &TableView::deletePressed, cl_delete_button, &QPushButton::click);

    connect(cl_new_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        bool ok = false;
        QString cl_name;

        while (!ok) {
            cl_name = QInputDialog::getText(
                        this,
                        QStringLiteral("New custom list"),
                        QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."),
                        QLineEdit::Normal,
                        cl_name);

            if (!cl_name.isEmpty()) {
                try {
                    project->addCustomList(cl_name.toStdString());
                    commit("Add custom list");

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(cl_rename_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        const CustomListsModel *cl = project->getCustomListsModel();

        int cl_index = current_index.row();

        QString old_name = QString::fromStdString(cl->at(cl_index).name);

        bool ok = false;
        QString new_name = old_name;

        while (!ok) {
            new_name = QInputDialog::getText(
                        this,
                        QStringLiteral("Rename custom list"),
                        QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."),
                        QLineEdit::Normal,
                        new_name);

            if (!new_name.isEmpty()) {
                try {
                    project->renameCustomList(old_name.toStdString(), new_name.toStdString());
                    commit("Rename custom list");

                    if (cl_index == getSelectedCustomList())
                        setSelectedCustomList(cl_index);

                    updateFrameDetails();

                    ok = true;
                } catch (WobblyException &e) {
                    errorPopup(e.what());
                }
            } else
                ok = true;
        }
    });

    connect(cl_delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = cl_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> indexes;
        indexes.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++)
            indexes.push_back(selection[i].row());

        std::sort(indexes.begin(), indexes.end());

        bool update_needed = false;

        for (int i = indexes.size() - 1; i >= 0; i--) {
            if (project->isCustomListInUse(indexes[i]))
                update_needed = true;

            project->deleteCustomList(indexes[i]);

            if (indexes[i] == selected_custom_list)
                setSelectedCustomList(selected_custom_list);
            else if (indexes[i] < selected_custom_list)
                selected_custom_list--;
        }
        commit("Delete custom list(s)");

        if (cl_view->model()->rowCount())
            cl_view->selectRow(cl_view->currentIndex().row());

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(cl_move_up_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = cl_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> indexes;
        indexes.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++)
            indexes.push_back(selection[i].row());

        std::sort(indexes.begin(), indexes.end());

        if (!indexes.size() || indexes[0] == 0)
            return;

        bool update_needed = false;

        for (size_t i = 0; i < indexes.size(); i++) {
            if (project->isCustomListInUse(indexes[i]))
                update_needed = true;

            project->moveCustomListUp(indexes[i]);

            if (indexes[i] == selected_custom_list + 1)
                selected_custom_list++;
        }
        commit("Move custom list(s)");

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(cl_move_down_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = cl_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> indexes;
        indexes.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++)
            indexes.push_back(selection[i].row());

        std::sort(indexes.begin(), indexes.end());

        if (!indexes.size() || indexes.back() == cl_view->model()->rowCount() - 1)
            return;

        bool update_needed = false;

        for (int i = indexes.size() - 1; i >= 0; i--) {
            if (project->isCustomListInUse(indexes[i]))
                update_needed = true;

            project->moveCustomListDown(indexes[i]);

            if (indexes[i] == selected_custom_list - 1)
                selected_custom_list--;
        }
        commit("Move custom list(s)");

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(cl_presets_box, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::textActivated), [this] (const QString &text) {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        int cl_index = current_index.row();

        bool update_needed = project->isCustomListInUse(cl_index) && project->getCustomListPreset(cl_index) != text.toStdString();

        project->setCustomListPreset(cl_index, text.toStdString());
        commit("Set custom list preset");

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }
    });

    connect(cl_position_group, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::idClicked), [this] (int id) {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        int cl_index = current_index.row();

        PositionInFilterChain new_position = (PositionInFilterChain)id;

        bool update_needed = project->isCustomListInUse(cl_index) && new_position != project->getCustomListPosition(cl_index);

        project->setCustomListPosition(cl_index, new_position);
        commit("Set custom list position");

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }
    });

    connect(cl_ranges_view, &TableView::deletePressed, cl_delete_range_button, &QPushButton::click);

    connect(cl_ranges_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        if (!project)
            return;

        bool ok;
        int frame = index.data().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(cl_delete_range_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        int cl_index = current_index.row();

        QModelIndexList selection = cl_ranges_view->selectionModel()->selectedRows();

        if (!selection.size())
            return;

        // Can't use the model indexes after modifying the model.
        std::vector<int> frames;
        frames.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++) {
            bool ok;
            int frame = selection[i].data().toInt(&ok);
            if (ok)
                frames.push_back(frame);
        }

        bool update_needed = project->isCustomListInUse(cl_index);

        for (size_t i = 0; i < frames.size(); i++)
            project->deleteCustomListRange(cl_index, frames[i]);
        commit("Delete frames from custom list");

        if (cl_ranges_view->model()->rowCount())
            cl_ranges_view->selectRow(cl_ranges_view->currentIndex().row());

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    });

    connect(cl_send_range_menu, &QMenu::aboutToShow, [this, cl_send_range_menu, cl_copy_range_menu] () {
        if (!project)
            return;

        cl_send_range_menu->clear();
        cl_copy_range_menu->clear();

        const CustomListsModel *custom_lists = project->getCustomListsModel();

        for (size_t i = 0; i < custom_lists->size(); i++) {
            QString cl_name = QString::fromStdString(custom_lists->at(i).name);
            QAction *send_action = cl_send_range_menu->addAction(cl_name);
            QAction *copy_action = cl_copy_range_menu->addAction(cl_name);
            send_action->setData((int)i);
            copy_action->setData((int)i);
        }
    });

    connect(cl_copy_range_menu, &QMenu::aboutToShow, cl_send_range_menu, &QMenu::aboutToShow);

    connect(cl_send_range_menu, &QMenu::triggered, [this, cl_delete_range_button] (QAction *action) {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        int cl_src_index = current_index.row();

        int cl_dst_index = action->data().toInt();
        if (cl_src_index == cl_dst_index)
            return;

        QModelIndexList selection = cl_ranges_view->selectionModel()->selectedRows();

        if (!selection.size())
            return;

        for (int i = 0; i < selection.size(); i++) {
            const FrameRange *range = project->findCustomListRange(cl_src_index, selection[i].data().toInt());
            project->addCustomListRange(cl_dst_index, range->first, range->last);
        }
        commit("Copy frames to custom list");   // This implementation copies the frames and then deletes them, which creates two undo events, but it's not really worth the effort to fix...

        // An update is only needed if the source custom list wasn't in use before deleting the ranges
        // and the destination custom list is in use after adding the ranges.
        // cl_delete_range_button->click() takes care of updating in the case where the source custom list
        // *was* in use before deleting the ranges.
        bool update_needed = !project->isCustomListInUse(cl_src_index) && project->isCustomListInUse(cl_dst_index);

        cl_delete_range_button->click();

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }
    });

    connect(cl_copy_range_menu, &QMenu::triggered, [this] (QAction *action) {
        if (!project)
            return;

        QModelIndex current_index = cl_view->selectionModel()->currentIndex();
        if (!current_index.isValid())
            return;

        int cl_src_index = current_index.row();

        int cl_dst_index = action->data().toInt();
        if (cl_src_index == cl_dst_index)
            return;

        QModelIndexList selection = cl_ranges_view->selectionModel()->selectedRows();

        if (!selection.size())
            return;

        for (int i = 0; i < selection.size(); i++) {
            const FrameRange *range = project->findCustomListRange(cl_src_index, selection[i].data().toInt());
            project->addCustomListRange(cl_dst_index, range->first, range->last);
        }
        commit("Copy frames to custom list");

        bool update_needed = project->isCustomListInUse(cl_dst_index);

        if (preview && update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(cl_view);

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(cl_new_button);
    vbox2->addWidget(cl_rename_button);
    vbox2->addWidget(cl_delete_button);
    vbox2->addWidget(cl_move_up_button);
    vbox2->addWidget(cl_move_down_button);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);

    vbox2 = new QVBoxLayout;
    for (int i = 0; i < 3; i++)
        vbox2->addWidget(cl_position_group->button(i));
    cl_position_box->setLayout(vbox2);

    vbox2 = new QVBoxLayout;
    vbox2->addWidget(cl_presets_box);
    vbox2->addWidget(cl_position_box);

    hbox->addLayout(vbox2);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QHBoxLayout *hbox2 = new QHBoxLayout;
    hbox2->addLayout(vbox);

    vbox = new QVBoxLayout;
    vbox->addWidget(cl_ranges_view);

    hbox = new QHBoxLayout;
    hbox->addWidget(cl_delete_range_button);
    hbox->addWidget(cl_send_range_button);
    hbox->addWidget(cl_copy_range_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox2->addLayout(vbox);


    QWidget *cl_widget = new QWidget;
    cl_widget->setLayout(hbox2);


    cl_dock = new DockWidget("Custom lists", this);
    cl_dock->setObjectName("custom lists editor");
    cl_dock->setVisible(false);
    cl_dock->setFloating(true);
    cl_dock->setWidget(cl_widget);
    addDockWidget(Qt::RightDockWidgetArea, cl_dock);
    tools_menu->addAction(cl_dock->toggleViewAction());
    connect(cl_dock, &DockWidget::visibilityChanged, cl_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFrameRatesViewer() {
    QGroupBox *frame_rates_group = new QGroupBox("Show rates");
    frame_rates_group->setCheckable(false);

    frame_rates_buttons = new QButtonGroup(this);
    frame_rates_buttons->setExclusive(false);
    const char *rates[] = { "&30", "2&4", "1&8", "1&2", "&6" };
    for (int i = 0; i < 5; i++)
        frame_rates_buttons->addButton(new QCheckBox(rates[i] + QStringLiteral(" fps")), i);

    frame_rates_table = new TableWidget(0, 4, this);
    frame_rates_table->setHorizontalHeaderLabels({ "Start", "End", "Frame rate", "Length" });


    connect(frame_rates_buttons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::idClicked), [this] () {
        if (!project)
            return;

        std::array<bool, 5> shown_rates;
        for (int i = 0; i < 5; i++)
            shown_rates[i] = frame_rates_buttons->button(i)->isChecked();

        project->setShownFrameRates(shown_rates);

        updateFrameRatesViewer();
    });


    connect(frame_rates_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = frame_rates_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    for (int i = 0; i < 5; i++)
        hbox->addWidget(frame_rates_buttons->button(i));

    frame_rates_group->setLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(frame_rates_group);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);
    vbox->addWidget(frame_rates_table);


    QWidget *frame_rates_widget = new QWidget;
    frame_rates_widget->setLayout(vbox);


    frame_rates_dock = new DockWidget("Frame rates", this);
    frame_rates_dock->setObjectName("frame rates viewer");
    frame_rates_dock->setVisible(false);
    frame_rates_dock->setFloating(true);
    frame_rates_dock->setWidget(frame_rates_widget);
    addDockWidget(Qt::RightDockWidgetArea, frame_rates_dock);
    tools_menu->addAction(frame_rates_dock->toggleViewAction());
    connect(frame_rates_dock, &DockWidget::visibilityChanged, frame_rates_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFrozenFramesViewer() {
    frozen_frames_view = new TableView;

    QPushButton *delete_button = new QPushButton("Delete");


    connect(frozen_frames_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        bool ok;
        int frame = index.data().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(frozen_frames_view, &TableView::deletePressed, delete_button, &QPushButton::click);

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = frozen_frames_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> frames;
        frames.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++) {
            bool ok;
            int frame = frozen_frames_view->model()->data(selection[i]).toInt(&ok);
            if (ok)
                frames.push_back(frame);
        }

        for (size_t i = 0 ; i < frames.size(); i++)
            project->deleteFreezeFrame(frames[i]);
        commit("Delete frozen frames");

        if (frozen_frames_view->model()->rowCount())
            frozen_frames_view->selectRow(frozen_frames_view->currentIndex().row());

        if (selection.size()) {
            try {
                evaluateMainDisplayScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        }
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frozen_frames_view);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(delete_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);


    QWidget *frozen_frames_widget = new QWidget;
    frozen_frames_widget->setLayout(vbox);


    frozen_frames_dock = new DockWidget("Frozen frames", this);
    frozen_frames_dock->setObjectName("frozen frames viewer");
    frozen_frames_dock->setVisible(false);
    frozen_frames_dock->setFloating(true);
    frozen_frames_dock->setWidget(frozen_frames_widget);
    addDockWidget(Qt::RightDockWidgetArea, frozen_frames_dock);
    tools_menu->addAction(frozen_frames_dock->toggleViewAction());
    connect(frozen_frames_dock, &DockWidget::visibilityChanged, frozen_frames_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPatternGuessingWindow() {
    QGroupBox *pg_methods_group = new QGroupBox(QStringLiteral("Guessing method"));

    std::map<int, QString> guessing_methods = {
        { PatternGuessingFromMatches, "From matches" },
        { PatternGuessingFromMics, "From mics" },
        { PatternGuessingFromDMetrics, "From dmetrics" },
        { PatternGuessingFromMicsAndDMetrics, "From mics+dmetrics" },
    };
    pg_methods_buttons = new QButtonGroup(this);
    for (auto it = guessing_methods.cbegin(); it != guessing_methods.cend(); it++)
        pg_methods_buttons->addButton(new QRadioButton(it->second), it->first);
    pg_methods_buttons->button(PatternGuessingFromMatches)->setChecked(true);

    pg_length_spin = new QSpinBox;
    pg_length_spin->setMaximum(999);
    pg_length_spin->setPrefix(QStringLiteral("Minimum length: "));
    pg_length_spin->setSuffix(QStringLiteral(" frames"));
    pg_length_spin->setValue(10);
    pg_length_spin->setToolTip(QStringLiteral("Sections shorter than this will be skipped."));

    pg_edge_cutoff = new QSpinBox;
    pg_edge_cutoff->setMaximum(3);
    pg_edge_cutoff->setPrefix(QStringLiteral("Edge Cutoff: "));
    pg_edge_cutoff->setSuffix(QStringLiteral(" frames"));
    pg_edge_cutoff->setValue(1);
    pg_edge_cutoff->setToolTip(QStringLiteral("Sections will be cut at the start and end by this amount of frames."));

    QGroupBox *pg_n_match_group = new QGroupBox(QStringLiteral("Use third N match"));

    const char *third_n_match[] = {
        "Always",
        "Never",
        "If it has lower mic"
    };
    pg_n_match_buttons = new QButtonGroup(this);
    for (int i = 0; i < 3; i++)
        pg_n_match_buttons->addButton(new QRadioButton(third_n_match[i]), i);
    pg_n_match_buttons->button(UseThirdNMatchNever)->setChecked(true);

    pg_n_match_buttons->button(UseThirdNMatchAlways)->setToolTip(QStringLiteral(
        "Always generate 'ccnnn' matches.\n"
        "\n"
        "Sometimes helps with field-blended hard telecine."));
    pg_n_match_buttons->button(UseThirdNMatchNever)->setToolTip(QStringLiteral(
        "Always generate 'cccnn' matches.\n"
        "\n"
        "Good for clean hard telecine."));
    pg_n_match_buttons->button(UseThirdNMatchIfPrettier)->setToolTip(QStringLiteral(
        "Generate 'ccnnn' matches if they result in a lower mic than\n"
        "with 'cccnn' matches (per cycle).\n"
        "\n"
        "Use with field-blended hard telecine."));

    QGroupBox *pg_decimate_group = new QGroupBox(QStringLiteral("Decimate"));

    const char *decimate[] = {
        "First duplicate",
        "Second duplicate",
        "Duplicate with higher mic per cycle",
        "Duplicate with higher mic per section"
    };
    pg_decimate_buttons = new QButtonGroup(this);
    for (int i = 0; i < 4; i++)
        pg_decimate_buttons->addButton(new QRadioButton(decimate[i]), i);
    pg_decimate_buttons->button(DropFirstDuplicate)->setChecked(true);

    pg_decimate_buttons->button(DropFirstDuplicate)->setToolTip(QStringLiteral(
        "Always drop the first duplicate. The first duplicate may have\n"
        "more compression artifacts than the second one.\n"
        "\n"
        "Use with clean hard telecine."));
    pg_decimate_buttons->button(DropSecondDuplicate)->setToolTip(QStringLiteral(
        "Always drop the second duplicate.\n"
        "\n"
        "Use with clean hard telecine."));
    pg_decimate_buttons->button(DropUglierDuplicatePerCycle)->setToolTip(QStringLiteral(
        "Drop the duplicate that is more likely to be combed in each cycle.\n"
        "\n"
        "When the first duplicate happens to be the last frame in the cycle,\n"
        "this will be done per section, to avoid creating unwanted 18 fps and\n"
        "30 fps cycles.\n"
        "\n"
        "Use with field-blended hard telecine."));
    pg_decimate_buttons->button(DropUglierDuplicatePerSection)->setToolTip(QStringLiteral(
        "Drop the duplicate that is more likely to be combed, on average,\n"
        "in the entire section.\n"
        "\n"
        "Use with field-blended hard telecine."));

    QGroupBox *pg_use_patterns_group = new QGroupBox(QStringLiteral("Use patterns"));

    std::map<int, QString> use_patterns = {
        { PatternCCCNN, "CCCNN" },
        { PatternCCNNN, "CCNNN" },
        { PatternCCCCN, "CCCCN" },
        { PatternCCCCC, "CCCCC" }
    };
    pg_use_patterns_buttons = new QButtonGroup(this);
    pg_use_patterns_buttons->setExclusive(false);
    for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++) {
        pg_use_patterns_buttons->addButton(new QCheckBox(it->second), it->first);
        pg_use_patterns_buttons->button(it->first)->setChecked(it->first == PatternCCCNN);
    }

    QPushButton *pg_process_section_button = new QPushButton(QStringLiteral("Process current section"));

    QPushButton *pg_process_project_button = new QPushButton(QStringLiteral("Process project"));

    pg_failures_table = new TableWidget(0, 2, this);
    pg_failures_table->setHorizontalHeaderLabels({ "Section", "Reason for failure" });


    connect(pg_use_patterns_buttons, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), [this] (int id, bool checked) {
        if (id == PatternCCCNN && !checked && !pg_use_patterns_buttons->button(PatternCCNNN)->isChecked())
            pg_use_patterns_buttons->button(PatternCCNNN)->setChecked(true);
        else if (id == PatternCCNNN && !checked && !pg_use_patterns_buttons->button(PatternCCCNN)->isChecked())
            pg_use_patterns_buttons->button(PatternCCCNN)->setChecked(true);
    });

    connect(pg_process_section_button, &QPushButton::clicked, [this] () {
        if (pg_methods_buttons->checkedId() == PatternGuessingFromMatches)
            guessCurrentSectionPatternsFromMatches();
        else if (pg_methods_buttons->checkedId() == PatternGuessingFromDMetrics)
            guessCurrentSectionPatternsFromDMetrics();
        else if (pg_methods_buttons->checkedId() == PatternGuessingFromMicsAndDMetrics)
            guessCurrentSectionPatternsFromMicsAndDMetrics();
        else
            guessCurrentSectionPatternsFromMics();
    });

    connect(pg_process_project_button, &QPushButton::clicked, [this] () {
        if (pg_methods_buttons->checkedId() == PatternGuessingFromMatches)
            guessProjectPatternsFromMatches();
        else if (pg_methods_buttons->checkedId() == PatternGuessingFromDMetrics)
            guessProjectPatternsFromDMetrics();
        else if (pg_methods_buttons->checkedId() == PatternGuessingFromMicsAndDMetrics)
            guessProjectPatternsFromMicsAndDMetrics();
        else
            guessProjectPatternsFromMics();
    });

    connect(pg_failures_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = pg_failures_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    for (auto it = guessing_methods.cbegin(); it != guessing_methods.cend(); it++)
        vbox->addWidget(pg_methods_buttons->button(it->first));
    pg_methods_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (int i = 0; i < 3; i++)
        vbox->addWidget(pg_n_match_buttons->button(i));
    pg_n_match_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (int i = 0; i < 4; i++)
        vbox->addWidget(pg_decimate_buttons->button(i));
    pg_decimate_group->setLayout(vbox);

    vbox = new QVBoxLayout;
    for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++)
        vbox->addWidget(pg_use_patterns_buttons->button(it->first));
    pg_use_patterns_group->setLayout(vbox);

    vbox = new QVBoxLayout;

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(pg_methods_group);

    QVBoxLayout *pvbox = new QVBoxLayout;
    pvbox->addWidget(pg_length_spin);
    pvbox->addWidget(pg_edge_cutoff);

    hbox->addLayout(pvbox);

    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(pg_n_match_group);
    hbox->addWidget(pg_decimate_group);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    // Kind of awful to put it here, but replaceWidget() only works the first two times it's called. (wtf?)
    // Or maybe the second time it removes "from", but doesn't insert "to"?
    connect(pg_methods_buttons, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), [pg_n_match_group, pg_use_patterns_group, hbox] (int id) {
        QWidget *from = pg_n_match_group;
        QWidget *to = pg_use_patterns_group;

        if (id == PatternGuessingFromMatches)
            std::swap(from, to);

        int index = hbox->indexOf(from);
        if (index > -1) {
            hbox->removeWidget(from);
            from->hide();
            hbox->insertWidget(index, to);
            to->show();
        }
    });

    hbox = new QHBoxLayout;
    hbox->addWidget(pg_process_section_button);
    hbox->addWidget(pg_process_project_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addWidget(pg_failures_table);


    QWidget *pg_widget = new QWidget;
    pg_widget->setLayout(vbox);


    pg_dock = new DockWidget("Pattern guessing", this);
    pg_dock->setObjectName("pattern guessing window");
    pg_dock->setVisible(false);
    pg_dock->setFloating(true);
    pg_dock->setWidget(pg_widget);
    addDockWidget(Qt::RightDockWidgetArea, pg_dock);
    tools_menu->addAction(pg_dock->toggleViewAction());
    connect(pg_dock, &DockWidget::visibilityChanged, pg_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createMicSearchWindow() {
    mic_search_minimum_spin = new QSpinBox;
    mic_search_minimum_spin->setRange(0, 256);
    mic_search_minimum_spin->setPrefix(QStringLiteral("Minimum diff: "));

    QPushButton *mic_search_previous_button = new QPushButton("Jump to previous");
    QPushButton *mic_search_next_button = new QPushButton("Jump to next");


    connect(mic_search_minimum_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        project->setMicSearchMinimum(value);
    });

    connect(mic_search_previous_button, &QPushButton::clicked, this, &WobblyWindow::jumpToPreviousMic);

    connect(mic_search_next_button, &QPushButton::clicked, this, &WobblyWindow::jumpToNextMic);


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(mic_search_minimum_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(mic_search_previous_button);
    hbox->addWidget(mic_search_next_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);
    vbox->addStretch(1);


    QWidget *mic_search_widget = new QWidget;
    mic_search_widget->setLayout(vbox);


    mic_search_dock = new DockWidget("Mic search", this);
    mic_search_dock->setObjectName("mic search window");
    mic_search_dock->setVisible(false);
    mic_search_dock->setFloating(true);
    mic_search_dock->setWidget(mic_search_widget);
    addDockWidget(Qt::RightDockWidgetArea, mic_search_dock);
    tools_menu->addAction(mic_search_dock->toggleViewAction());
    connect(mic_search_dock, &DockWidget::visibilityChanged, mic_search_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createDMetricSearchWindow() {
    dmetric_search_minimum_spin = new QSpinBox;
    dmetric_search_minimum_spin->setRange(0, 256);
    dmetric_search_minimum_spin->setPrefix(QStringLiteral("Minimum diff: "));

    QPushButton *dmetric_search_previous_button = new QPushButton("Jump to previous");
    QPushButton *dmetric_search_next_button = new QPushButton("Jump to next");

    connect(dmetric_search_minimum_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        project->setDMetricSearchMinimum(value);
    });

    connect(dmetric_search_previous_button, &QPushButton::clicked, this, &WobblyWindow::jumpToPreviousDMetric);

    connect(dmetric_search_next_button, &QPushButton::clicked, this, &WobblyWindow::jumpToNextDMetric);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(dmetric_search_minimum_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(dmetric_search_previous_button);
    hbox->addWidget(dmetric_search_next_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);
    vbox->addStretch(1);

    QWidget *dmetric_search_widget = new QWidget;
    dmetric_search_widget->setLayout(vbox);

    dmetric_search_dock = new DockWidget("DMetric search", this);
    dmetric_search_dock->setObjectName("dmetric search window");
    dmetric_search_dock->setVisible(false);
    dmetric_search_dock->setFloating(true);
    dmetric_search_dock->setWidget(dmetric_search_widget);
    addDockWidget(Qt::RightDockWidgetArea, dmetric_search_dock);
    tools_menu->addAction(dmetric_search_dock->toggleViewAction());
    connect(dmetric_search_dock, &DockWidget::visibilityChanged, dmetric_search_dock, &DockWidget::setEnabled);
}

void WobblyWindow::createCMatchSequencesWindow() {
    c_match_minimum_spin = new QSpinBox;
    c_match_minimum_spin->setRange(4, 999);
    c_match_minimum_spin->setPrefix(QStringLiteral("Minimum: "));
    c_match_minimum_spin->setSuffix(QStringLiteral(" frames"));

    c_match_sequences_table = new TableWidget(0, 2, this);
    c_match_sequences_table->setHorizontalHeaderLabels({ "Start", "Length" });


    connect(c_match_minimum_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        if (!project)
            return;

        project->setCMatchSequencesMinimum(value);

        updateCMatchSequencesWindow();
    });

    connect(c_match_sequences_table, &TableWidget::cellDoubleClicked, [this] (int row) {
        QTableWidgetItem *item = c_match_sequences_table->item(row, 0);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(c_match_minimum_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);

    vbox->addWidget(c_match_sequences_table);


    QWidget *c_match_sequences_widget = new QWidget;
    c_match_sequences_widget->setLayout(vbox);


    c_match_sequences_dock = new DockWidget("C match sequences", this);
    c_match_sequences_dock->setObjectName("c match sequences window");
    c_match_sequences_dock->setVisible(false);
    c_match_sequences_dock->setFloating(true);
    c_match_sequences_dock->setWidget(c_match_sequences_widget);
    addDockWidget(Qt::RightDockWidgetArea, c_match_sequences_dock);
    tools_menu->addAction(c_match_sequences_dock->toggleViewAction());
    connect(c_match_sequences_dock, &DockWidget::visibilityChanged, c_match_sequences_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createFadesWindow() {
    fades_gaps_spin = new QSpinBox;
    fades_gaps_spin->setRange(0, 100);
    fades_gaps_spin->setValue(1);
    fades_gaps_spin->setPrefix(QStringLiteral("Ignore gaps of "));
    fades_gaps_spin->setSuffix(QStringLiteral(" frames or fewer"));

    fades_table = new TableWidget(0, 2, this);
    fades_table->setHorizontalHeaderLabels({ "Start", "End" });


    connect(fades_gaps_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] () {
        if (!project)
            return;

        updateFadesWindow();
    });

    connect(fades_table, &TableWidget::cellDoubleClicked, [this] (int row, int column) {
        QTableWidgetItem *item = fades_table->item(row, column);
        bool ok;
        int frame = item->text().toInt(&ok);
        if (ok)
            requestFrames(frame);
    });


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(fades_gaps_spin);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(hbox);
    vbox->addWidget(fades_table);


    QWidget *fades_widget = new QWidget;
    fades_widget->setLayout(vbox);


    fades_dock = new DockWidget("Interlaced fades", this);
    fades_dock->setObjectName("interlaced fades window");
    fades_dock->setVisible(false);
    fades_dock->setFloating(true);
    fades_dock->setWidget(fades_widget);
    addDockWidget(Qt::RightDockWidgetArea, fades_dock);
    tools_menu->addAction(fades_dock->toggleViewAction());
    connect(fades_dock, &DockWidget::visibilityChanged, fades_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createCombedFramesWindow() {
    combed_view = new TableView;

    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));

    QPushButton *refresh_button = new QPushButton(QStringLiteral("Refresh"));
    refresh_button->setToolTip(QStringLiteral("Run the 'final' script through tdm.IsCombed to see what frames are still combed."));


    connect(combed_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        bool ok;
        int frame = combed_view->model()->data(index).toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    connect(combed_view, &TableView::deletePressed, delete_button, &QPushButton::click);

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = combed_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> frames;
        frames.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++) {
            bool ok;
            int frame = combed_view->model()->data(selection[i]).toInt(&ok);
            if (ok)
                frames.push_back(frame);
        }

        for (size_t i = 0 ; i < frames.size(); i++)
            project->deleteCombedFrame(frames[i]);
        commit("Delete combed frame(s)");

        if (combed_view->model()->rowCount())
            combed_view->selectRow(combed_view->currentIndex().row());

        updateFrameDetails();
    });

    connect(refresh_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        std::string script;

        try {
            script = project->generateFinalScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());

            return;
        }

        setEnabled(false);

        script += "c.max_cache_size = " + std::to_string(settings_cache_spin->value()) + "\n";

        CombedFramesCollector *collector = new CombedFramesCollector(vssapi, vsapi, vscore, vsscript);

        ProgressDialog *progress_dialog = new ProgressDialog;
        progress_dialog->setModal(true);
        progress_dialog->setWindowTitle(QStringLiteral("Detecting combed frames..."));
        progress_dialog->setLabel(new QLabel);
        progress_dialog->reset();
        progress_dialog->setMinimum(0);
        progress_dialog->setMaximum(project->getNumFrames(PostDecimate));
        progress_dialog->setValue(0);

        connect(collector, &CombedFramesCollector::errorMessage, this, &WobblyWindow::errorPopup);

        connect(collector, &CombedFramesCollector::progressUpdate, progress_dialog, &QProgressDialog::setValue);

        connect(collector, &CombedFramesCollector::speedUpdate, [progress_dialog] (double fps, QString time_left) {
            progress_dialog->setLabelText(QStringLiteral("%1 fps, %2 left").arg(fps, 0, 'f', 2).arg(time_left));
        });

        connect(collector, &CombedFramesCollector::combedFramesCollected, [this] (const std::set<int> &combed_frames) {
            project->clearCombedFrames();

            for (auto it = combed_frames.cbegin(); it != combed_frames.cend(); it++)
                project->addCombedFrame(project->frameNumberBeforeDecimation(*it));

            commit("Find combed frames");
            updateFrameDetails();
        });

        connect(collector, &CombedFramesCollector::workFinished, [this, collector, progress_dialog] () {
            collector->deleteLater();
            progress_dialog->deleteLater();

            QApplication::alert(this, 0);

            setEnabled(true);
        });

        connect(progress_dialog, &ProgressDialog::canceled, collector, &CombedFramesCollector::stop);

        connect(progress_dialog, &ProgressDialog::minimiseChanged, [this] (bool minimised) {
            if (minimised)
                setWindowState(windowState() | Qt::WindowMinimized);
            else
                setWindowState(windowState() & ~Qt::WindowMinimized);
        });

        collector->start(script, (project_path.isEmpty() ? video_path : project_path).toUtf8().constData());
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(combed_view);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(delete_button);
    hbox->addStretch(1);
    hbox->addWidget(refresh_button);
    vbox->addLayout(hbox);


    QWidget *combed_widget = new QWidget;
    combed_widget->setLayout(vbox);


    combed_dock = new DockWidget("Combed frames", this);
    combed_dock->setObjectName("combed frames window");
    combed_dock->setVisible(false);
    combed_dock->setFloating(true);
    combed_dock->setWidget(combed_widget);
    addDockWidget(Qt::RightDockWidgetArea, combed_dock);
    tools_menu->addAction(combed_dock->toggleViewAction());
    connect(combed_dock, &DockWidget::visibilityChanged, combed_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createOrphanFieldsWindow() {
    orphan_view = new TableView;

    connect(orphan_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        bool ok;
        int frame = orphan_view->model()->data(index).toInt(&ok);
        if (ok)
            requestFrames(frame);
    });

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(orphan_view);

    QWidget *orphan_widget = new QWidget;
    orphan_widget->setLayout(vbox);

    orphan_dock = new DockWidget("Orphan fields", this);
    orphan_dock->setObjectName("orphan fields window");
    orphan_dock->setVisible(false);
    orphan_dock->setFloating(true);
    orphan_dock->setWidget(orphan_widget);
    addDockWidget(Qt::RightDockWidgetArea, orphan_dock);
    tools_menu->addAction(orphan_dock->toggleViewAction());
    connect(orphan_dock, &DockWidget::visibilityChanged, orphan_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createBookmarksWindow() {
    bookmarks_view = new TableView;
    bookmarks_view->setEditTriggers(QAbstractItemView::DoubleClicked);

    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));


    connect(bookmarks_view, &TableView::doubleClicked, [this] (const QModelIndex &index) {
        if (index.column() == BookmarksModel::FrameColumn) {
            bool ok;
            int frame = bookmarks_view->model()->data(index).toInt(&ok);
            if (ok)
                requestFrames(frame);
        }
    });

    connect(bookmarks_view, &TableView::deletePressed, delete_button, &QPushButton::click);

    connect(delete_button, &QPushButton::clicked, [this] () {
        if (!project)
            return;

        QModelIndexList selection = bookmarks_view->selectionModel()->selectedRows();

        // Can't use the model indexes after modifying the model.
        std::vector<int> frames;
        frames.reserve(selection.size());

        for (int i = 0; i < selection.size(); i++) {
            bool ok;
            int frame = bookmarks_view->model()->data(selection[i]).toInt(&ok);
            if (ok)
                frames.push_back(frame);
        }

        for (size_t i = 0 ; i < frames.size(); i++)
            project->deleteBookmark(frames[i]);
        commit("Delete bookmark(s)");

        if (bookmarks_view->model()->rowCount())
            bookmarks_view->selectRow(bookmarks_view->currentIndex().row());

        updateFrameDetails();
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(bookmarks_view);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(delete_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);


    QWidget *bookmarks_widget = new QWidget;
    bookmarks_widget->setLayout(vbox);


    bookmarks_dock = new DockWidget(QStringLiteral("Bookmarks"), this);
    bookmarks_dock->setObjectName(QStringLiteral("bookmarks window"));
    bookmarks_dock->setVisible(false);
    bookmarks_dock->setFloating(true);
    bookmarks_dock->setWidget(bookmarks_widget);
    addDockWidget(Qt::RightDockWidgetArea, bookmarks_dock);
    tools_menu->addAction(bookmarks_dock->toggleViewAction());
    connect(bookmarks_dock, &DockWidget::visibilityChanged, bookmarks_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createSettingsWindow() {
    settings_compact_projects_check = new QCheckBox("Create compact project files");

    settings_use_relative_paths_check = new QCheckBox(QStringLiteral("Use relative paths in project files"));

    settings_print_details_check = new QCheckBox(QStringLiteral("Print frame details on top of the video"));

    settings_bookmark_description_check = new QCheckBox(QStringLiteral("Ask for bookmark description"));

    settings_decimation_function_combo = new QComboBox;
    settings_decimation_function_combo->addItems({ "Auto", "SelectEvery", "DeleteFrames" });

    settings_font_spin = new QSpinBox;
    settings_font_spin->setRange(4, 99);

    overlay_size_spin = new QSpinBox;
    overlay_size_spin->setRange(1, 10);

    application_style_combo = new QComboBox;
    application_style_combo->addItems({ "Old", "Light", "Dark" });
    application_style_combo->setCurrentIndex(0);

    settings_colormatrix_combo = new QComboBox;
    settings_colormatrix_combo->addItems({
                                             "BT 601",
                                             "BT 709",
                                             "BT 2020 NCL",
                                             "BT 2020 CL"
                                         });
    settings_colormatrix_combo->setCurrentIndex(1);

    settings_cache_spin = new QSpinBox;
    settings_cache_spin->setRange(1, 99999);
    settings_cache_spin->setValue(4096);
    settings_cache_spin->setSuffix(QStringLiteral(" MiB"));

    settings_undo_steps_spin = new SpinBox;
    settings_undo_steps_spin->setRange(0, 1000);

    settings_num_thumbnails_spin = new SpinBox;
    settings_num_thumbnails_spin->setRange(-1, 21);
    settings_num_thumbnails_spin->setSingleStep(2);
    settings_num_thumbnails_spin->setSpecialValueText(QStringLiteral("none"));
    settings_num_thumbnails_spin->lineEdit()->setReadOnly(true);

    settings_thumbnail_size_dspin = new QDoubleSpinBox;
    settings_thumbnail_size_dspin->setDecimals(1);
    settings_thumbnail_size_dspin->setSingleStep(0.5);
    settings_thumbnail_size_dspin->setSuffix(QStringLiteral("% of screen size"));

    settings_shortcuts_table = new TableWidget(0, 3, this);
    settings_shortcuts_table->setHorizontalHeaderLabels({ "Current", "Default", "Description" });

    QLineEdit *settings_shortcut_edit = new QLineEdit;

    QPushButton *settings_reset_shortcuts_button = new QPushButton("Reset selected shortcuts");


    connect(settings_compact_projects_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue(KEY_COMPACT_PROJECT_FILES, checked);
    });

    connect(settings_use_relative_paths_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue(KEY_USE_RELATIVE_PATHS, checked);
    });

    connect(settings_print_details_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue(KEY_PRINT_DETAILS_ON_VIDEO, checked);

        updateFrameDetails();
    });

    connect(settings_bookmark_description_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue(KEY_ASK_FOR_BOOKMARK_DESCRIPTION, checked);
    });

    connect(settings_decimation_function_combo, &QComboBox::currentTextChanged, [this] (const QString &text) {
        settings.setValue(KEY_DECIMATION_FUNCTION, text);
    });

    connect(settings_font_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        QFont font = QApplication::font();
        font.setPointSize(value);
        QApplication::setFont(font);

        settings.setValue(KEY_FONT_SIZE, value);
    });

    connect(overlay_size_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        overlay_label->setOverlayScaling(value);

        settings.setValue(KEY_OVERLAY_SIZE, value);
    });

    connect(application_style_combo, &QComboBox::currentTextChanged, [this] (const QString &text) {
        settings.setValue(KEY_APPLICATION_STYLE, text);

        if (text == "Old") {
            setStyleSheet("");
        } else {
            QString stylesDir = QApplication::applicationDirPath() + "/styles/";

            QDir::addSearchPath("light_images", stylesDir + "light/rc");
            QDir::addSearchPath("dark_images", stylesDir + "dark/rc");

            QFile styleSheet(stylesDir + text.toLower().toUtf8().constData() + "/style.qss");
            if(styleSheet.open(QFile::ReadOnly | QFile::Text))
                setStyleSheet(styleSheet.readAll());
        }
    });

    connect(settings_colormatrix_combo, &QComboBox::currentTextChanged, [this] (const QString &text) {
        settings.setValue(KEY_COLORMATRIX, text);

        if (!project)
            return;

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    });

    connect(settings_cache_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
        settings.setValue(KEY_MAXIMUM_CACHE_SIZE, value);
    });

    connect(settings_undo_steps_spin, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), [this] (int value) {
        if (project)
            project->setUndoSteps(size_t(value));
        settings.setValue(KEY_UNDO_STEPS, value);
    });

    connect(settings_num_thumbnails_spin, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), [this] (int num_thumbnails) {
        settings.setValue(KEY_NUMBER_OF_THUMBNAILS, num_thumbnails);

        int first_visible = (MAX_THUMBNAILS - num_thumbnails) / 2;
        int last_visible = first_visible + num_thumbnails - 1;

        for (int i = 0; i < MAX_THUMBNAILS; i++) {
            bool visible = i >= first_visible && i <= last_visible;

            thumb_labels[i]->setVisible(visible);

            if (!visible)
                thumb_labels[i]->setPixmap(QPixmap());

            if (visible && !project)
                thumb_labels[i]->setPixmap(splash_thumb);
        }

        if (!project)
            return;

        requestFrames(current_frame);
    });

    connect(settings_thumbnail_size_dspin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [this] (double percentage) {
        settings.setValue(KEY_THUMBNAIL_SIZE, percentage);

        splash_thumb = getThumbnail(splash_image);

        int num_thumbnails = settings_num_thumbnails_spin->value();

        if (num_thumbnails > 0) {
            if (project) {
                requestFrames(current_frame);
            } else {
                int first_visible = (MAX_THUMBNAILS - num_thumbnails) / 2;
                int last_visible = first_visible + num_thumbnails - 1;

                for (int i = first_visible; i <= last_visible; i++)
                    thumb_labels[i]->setPixmap(splash_thumb);
            }
        }
    });

    connect(settings_shortcuts_table, &TableWidget::cellDoubleClicked, settings_shortcut_edit, static_cast<void (QLineEdit::*)()>(&QLineEdit::setFocus));

    connect(settings_shortcuts_table, &TableWidget::currentCellChanged, [this, settings_shortcut_edit] (int currentRow) {
        if (currentRow < 0)
            return;

        settings_shortcut_edit->setText(shortcuts[currentRow].keys);
    });

    connect(settings_shortcut_edit, &QLineEdit::textEdited, [this] (const QString &text) {
        int row = settings_shortcuts_table->currentRow();
        if (row < 0)
            return;

        QString keys;
        QKeySequence key_sequence(text, QKeySequence::NativeText);
        if (key_sequence.count() <= 1)
            keys = key_sequence.toString(QKeySequence::PortableText);
        shortcuts[row].keys = keys;
        settings_shortcuts_table->item(row, 0)->setText(keys);
        settings_shortcuts_table->resizeColumnToContents(0);
    });

    connect(settings_shortcut_edit, &QLineEdit::editingFinished, [this] () {
        int row = settings_shortcuts_table->currentRow();
        if (row < 0)
            return;

        // XXX No duplicate shortcuts.

        QString settings_key = KEY_KEYS + shortcuts[row].description;

        if (shortcuts[row].keys == shortcuts[row].default_keys)
            settings.remove(settings_key);
        else
            settings.setValue(settings_key, shortcuts[row].keys);
    });

    connect(settings_reset_shortcuts_button, &QPushButton::clicked, [this, settings_shortcut_edit] () {
        int current_row = settings_shortcuts_table->currentRow();

        auto selection = settings_shortcuts_table->selectedRanges();

        for (int i = 0; i < selection.size(); i++) {
            for (int j = selection[i].topRow(); j <= selection[i].bottomRow(); j++) {
                if (shortcuts[j].keys != shortcuts[j].default_keys) {
                    shortcuts[j].keys = shortcuts[j].default_keys;

                    settings_shortcuts_table->item(j, 0)->setText(shortcuts[j].keys);

                    if (j == current_row)
                        settings_shortcut_edit->setText(shortcuts[j].keys);

                    settings.remove("user_interface/keys/" + shortcuts[j].description);
                }
            }
        }
    });


    QTabWidget *settings_tabs = new QTabWidget;

    QFormLayout *form = new QFormLayout;
    form->addRow(settings_compact_projects_check);
    form->addRow(settings_use_relative_paths_check);
    form->addRow(settings_print_details_check);
    form->addRow(settings_bookmark_description_check);
    form->addRow(QStringLiteral("Decimation function"), settings_decimation_function_combo);
    form->addRow(QStringLiteral("Font size"), settings_font_spin);
    form->addRow(QStringLiteral("Overlay size"), overlay_size_spin);
    form->addRow(QStringLiteral("Application style"), application_style_combo);
    form->addRow(QStringLiteral("Colormatrix"), settings_colormatrix_combo);
    form->addRow(QStringLiteral("Maximum cache size"), settings_cache_spin);
    form->addRow(QStringLiteral("Maximum undo steps"), settings_undo_steps_spin);
    form->addRow(QStringLiteral("Number of thumbnails"), settings_num_thumbnails_spin);
    form->addRow(QStringLiteral("Thumbnail size"), settings_thumbnail_size_dspin);

    QWidget *settings_general_widget = new QWidget;
    settings_general_widget->setLayout(form);
    settings_tabs->addTab(settings_general_widget, "General");

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(settings_shortcuts_table);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Edit shortcut:"));
    hbox->addWidget(settings_shortcut_edit);
    hbox->addWidget(settings_reset_shortcuts_button);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    QWidget *settings_shortcuts_widget = new QWidget;
    settings_shortcuts_widget->setLayout(vbox);
    settings_tabs->addTab(settings_shortcuts_widget, "Keyboard shortcuts");


    settings_dock = new DockWidget("Settings", this);
    settings_dock->setObjectName("settings window");
    settings_dock->setVisible(false);
    settings_dock->setFloating(true);
    settings_dock->setWidget(settings_tabs);
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    tools_menu->addAction(settings_dock->toggleViewAction());
    connect(settings_dock, &DockWidget::visibilityChanged, settings_dock, &DockWidget::setEnabled);
}


void WobblyWindow::createPluginWindow() {
    auto filters = getRequiredFilterStates(vsapi, vscore);

    QTableWidget *plugin_table = new QTableWidget(filters.size(), 2, this);
    plugin_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    plugin_table->setHorizontalHeaderLabels({ "Plugin", "Status" });
    int row = 0;
    for (const auto &iter : filters) {
        plugin_table->setItem(row, 0, new QTableWidgetItem(iter.first.c_str()));
        plugin_table->setItem(row, 1, new QTableWidgetItem((iter.second == FilterState::Exists) ? "Available" : ((iter.second == FilterState::MissingFilter) ? "Invalid version" : "Missing")));
        row++;
    }

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(plugin_table);

    QWidget *plugin_window = new QWidget;
    plugin_window->setLayout(vbox);
    plugin_window->setWindowModality(Qt::ApplicationModal);

    QAction *plugin_action = new QAction("Check plugins");
    connect(plugin_action, &QAction::triggered, plugin_window, &QWidget::show);
    tools_menu->addAction(plugin_action);
}


void WobblyWindow::drawColorBars() {
    auto drawRect = [this] (int left, int top, int width, int height, int red, int green, int blue) {
        uint8_t *ptr = splash_image.bits();
        int stride = splash_image.bytesPerLine();
        ptr += stride * top;

        for (int x = left; x < left + width; x++) {
            ptr[x*4] = blue;
            ptr[x*4 + 1] = green;
            ptr[x*4 + 2] = red;
            ptr[x*4 + 3] = 255;
        }

        for (int y = 1; y < height; y++)
            memcpy(ptr + y * stride + left * 4, ptr + left * 4, width * 4);
    };

    auto drawGrayHorizontalGradient = [this] (int left, int top, int width, int height, int start, int end) {
        uint8_t *ptr = splash_image.bits();
        int stride = splash_image.bytesPerLine();
        ptr += stride * top;

        for (int x = left; x < left + width; x++) {
            float weight_end = (x - left) / (float)width;
            float weight_start = 1.0f - weight_end;

            int value = start * weight_start + end * weight_end;

            ptr[x*4] = value;
            ptr[x*4 + 1] = value;
            ptr[x*4 + 2] = value;
            ptr[x*4 + 3] = 255;
        }

        for (int y = 1; y < height; y++)
            memcpy(ptr + y * stride + left * 4, ptr + left * 4, width * 4);
    };

    drawRect(  0,   0,  90, 280, 104, 104, 104);
    drawRect( 90,   0,  77, 280, 180, 180, 180);
    drawRect(167,   0,  77, 280, 180, 180,  16);
    drawRect(244,   0,  77, 280,  16, 180, 180);
    drawRect(321,   0,  78, 280,  16, 180,  16);
    drawRect(399,   0,  77, 280, 180,  16, 180);
    drawRect(476,   0,  77, 280, 180,  16,  16);
    drawRect(553,   0,  77, 280,  16,  16, 180);
    drawRect(630,   0,  90, 280, 104, 104, 104);

    drawRect(  0, 280,  90,  40,  16, 235, 235);
    drawRect( 90, 280,  77,  40, 235, 235, 235);
    drawRect(167, 280, 463,  40, 180, 180, 180);
    drawRect(630, 280,  90,  40,  16,  16, 235);

    drawRect(  0, 320,  90,  40, 235, 235,  16);
    drawRect( 90, 320,  77,  40,  16,  16,  16);
    drawGrayHorizontalGradient(167, 320, 386, 40, 17, 234);
    drawRect(553, 320,  77,  40, 235, 235, 235);
    drawRect(630, 320,  90,  40, 235,  16,  16);

    drawRect(  0, 360,  90, 120,  49,  49,  49);
    drawRect( 90, 360, 116, 120,  16,  16,  16);
    drawRect(206, 360, 154, 120, 235, 235, 235);
    drawRect(360, 360,  64, 120,  16,  16,  16);
    drawRect(424, 360,  25, 120,  12,  12,  12);
    drawRect(449, 360,  27, 120,  16,  16,  16);
    drawRect(476, 360,  25, 120,  20,  20,  20);
    drawRect(501, 360,  27, 120,  16,  16,  16);
    drawRect(528, 360,  25, 120,  25,  25,  25);
    drawRect(553, 360,  77, 120,  16,  16,  16);
    drawRect(630, 360,  90, 120,  49,  49,  49);
}


void WobblyWindow::createUI() {
    setAcceptDrops(true);

    createMenu();
    createShortcuts();

    setWindowTitle(window_title);

    statusBar()->setSizeGripEnabled(true);

    selected_preset_label = new QLabel(QStringLiteral("Selected preset: "));
    selected_custom_list_label = new QLabel(QStringLiteral("Selected custom list: "));
    zoom_label = new QLabel(QStringLiteral("Zoom: 1x"));
    statusBar()->addPermanentWidget(selected_preset_label);
    statusBar()->addPermanentWidget(selected_custom_list_label);
    statusBar()->addPermanentWidget(zoom_label);

    drawColorBars();

    tab_bar = new QTabBar;
    tab_bar->addTab(QStringLiteral("Source"));
    tab_bar->addTab(QStringLiteral("Preview"));
    tab_bar->setExpanding(false);
    tab_bar->setEnabled(false);
    tab_bar->setFocusPolicy(Qt::NoFocus);

    frame_label = new FrameLabel;
    frame_label->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < MAX_THUMBNAILS; i++) {
        thumb_labels[i] = new QLabel;
        thumb_labels[i]->setAlignment(Qt::AlignCenter);
        thumb_labels[i]->setPixmap(splash_thumb);
        thumb_labels[i]->setVisible(false);
    }

    frame_scroll = new ScrollArea;
    frame_scroll->resize(720, 480);
    frame_scroll->setFocusPolicy(Qt::ClickFocus);
    frame_scroll->setAlignment(Qt::AlignCenter);
    frame_scroll->setWidgetResizable(true);
    frame_scroll->setWidget(frame_label);

    overlay_label = new OverlayLabel;
    overlay_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay_label->setStyleSheet("background-color: none;");

    frame_slider = new QSlider(Qt::Horizontal);
    frame_slider->setTracking(false);
    frame_slider->setFocusPolicy(Qt::NoFocus);


    connect(tab_bar, &QTabBar::currentChanged, this, &WobblyWindow::togglePreview);


    connect(frame_label, &FrameLabel::pixmapSizeChanged, overlay_label, &OverlayLabel::setFramePixmapSize);


    connect(frame_slider, &QSlider::valueChanged, [this] (int value) {
        if (!project)
            return;

        requestFrames(value);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(tab_bar);

    vbox->addWidget(frame_scroll);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addStretch(1);
    for (int i = 0; i < MAX_THUMBNAILS; i++)
        hbox->addWidget(thumb_labels[i]);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    vbox->addWidget(frame_slider);

    QWidget *central_widget = new QWidget;
    central_widget->setLayout(vbox);

    setCentralWidget(central_widget);

    vbox = new QVBoxLayout;
    vbox->addWidget(overlay_label);
    frame_scroll->setLayout(vbox);


    createFrameDetailsViewer();
    createCropAssistant();
    createPresetEditor();
    createPatternEditor();
    createSectionsEditor();
    createCustomListsEditor();
    createFrameRatesViewer();
    createFrozenFramesViewer();
    createPatternGuessingWindow();
    createMicSearchWindow();
    createDMetricSearchWindow();
    createCMatchSequencesWindow();
    createFadesWindow();
    createCombedFramesWindow();
    createOrphanFieldsWindow();
    createBookmarksWindow();
    createSettingsWindow();


    frame_label->setPixmap(QPixmap::fromImage(splash_image));

    splash_thumb = getThumbnail(splash_image);

    for (int i = 0; i < MAX_THUMBNAILS; i++)
        thumb_labels[i]->setPixmap(splash_thumb);
}


void VS_CC messageHandler(int msgType, const char *msg, void *userData) {
    WobblyWindow *window = (WobblyWindow *)userData;

    Qt::ConnectionType type;
    if (QThread::currentThread() == window->thread())
        type = Qt::DirectConnection;
    else
        type = Qt::QueuedConnection;

    QMetaObject::invokeMethod(window, "vsLogPopup", type, Q_ARG(int, msgType), Q_ARG(QString, QString(msg)));
}


void WobblyWindow::vsLogPopup(int msgType, const QString &msg) {
    QString message;

    if (msgType == mtFatal) {
        if (project) {
            if (project_path.isEmpty())
                project_path = video_path + ".wob";

            realSaveProject(project_path);

            message += "Your work has been saved to '" + project_path + "'. ";
        }
        writeSettings();

        message += "Wobbly will now close.\n\n";
    }

    message += "Message type: ";

    if (msgType == mtFatal) {
        message += "fatal";
    } else if (msgType == mtCritical) {
        message += "critical";
    } else if (msgType == mtWarning) {
        message += "warning";
    } else if (msgType == mtDebug) {
        message += "debug";
    } else {
        message += "unknown";
    }

    message += ". Message: ";
    message += msg;

    QMessageBox::information(this, QStringLiteral("vsLog"), message);
}


void WobblyWindow::initialiseVapourSynth() {
    GetVSScriptAPIFunc newVSScriptAPI = fetchVSScript();

    std::string oldlocale(setlocale(LC_ALL, NULL));
    vssapi = newVSScriptAPI(VSSCRIPT_API_VERSION);
    setlocale(LC_ALL, oldlocale.c_str());

    if (!vssapi)
        throw WobblyException("Fatal error: failed to initialise VSScript. Your VapourSynth installation is probably broken. Python probably couldn't 'import vapoursynth'.");

    vsapi = vssapi->getVSAPI(VAPOURSYNTH_API_VERSION);
    if (!vsapi)
        throw WobblyException("Fatal error: failed to acquire VapourSynth API struct. Did you update the VapourSynth library but not the Python module (or the other way around)?");

    vscore = vsapi->createCore(0);
    if (!vscore)
        throw WobblyException("Fatal error: failed to create VapourSynth core object.");

    vsapi->addLogHandler(messageHandler, nullptr, this, vscore);

    vsscript = vssapi->createScript(vscore);
    if (!vsscript)
        throw WobblyException(std::string("Fatal error: failed to create VSScript object. Error message: ") + vssapi->getError(vsscript));

}


void WobblyWindow::cleanUpVapourSynth() {
    frame_label->setPixmap(QPixmap());
    for (int i = 0; i < MAX_THUMBNAILS; i++)
        thumb_labels[i]->setPixmap(QPixmap());

    for (int i = 0; i < 2; i++) {
        vsapi->freeNode(vsnode[i]);
        vsnode[i] = nullptr;
    }

    vssapi->freeScript(vsscript);
    vsscript = nullptr;
    vscore = nullptr;
}


void WobblyWindow::updateGeometry() {
    const std::string &state = project->getUIState();
    if (state.size())
        restoreState(QByteArray::fromBase64(QByteArray(state.c_str(), state.size())));

    const std::string &geometry = project->getUIGeometry();
    if (geometry.size())
        restoreGeometry(QByteArray::fromBase64(QByteArray(geometry.c_str(), geometry.size())));
}

void WobblyWindow::updateWindowTitle() {
    setWindowTitle(QStringLiteral("%1 - %2%3").arg(window_title).arg(project->isModified() ? "*" : "").arg(project_path.isEmpty() ? video_path : project_path));
}


void WobblyWindow::initialiseCropAssistant() {
    // Crop.
    const Crop &crop = project->getCrop();
    int crop_values[4] = { crop.left, crop.top, crop.right, crop.bottom };

    for (int i = 0; i < 4; i++) {
        QSignalBlocker block(crop_spin[i]);
        crop_spin[i]->setValue(crop_values[i]);
    }

    crop_box->setChecked(project->isCropEnabled());
    crop_early_check->setChecked(project->isCropEarly());


    // Resize.
    const Resize &resize = project->getResize();
    int resize_values[2] = { resize.width, resize.height };

    for (int i = 0; i < 2; i++) {
        QSignalBlocker block(resize_spin[i]);
        resize_spin[i]->setValue(resize_values[i]);
    }

    {
        QSignalBlocker block(resize_box);
        resize_box->setChecked(project->isResizeEnabled());
    }

    QString filter = QString::fromStdString(resize.filter);
    filter[0] = filter[0].toUpper();
    resize_filter_combo->setCurrentText(filter);


    // Bit depth.
    std::unordered_map<int, int> bits_to_index[2] = {
        { { 8, 0 }, { 9, 1 }, {10, 2 }, { 12, 3 }, { 16, 4 } },
        { { 16, 5 }, { 32, 6 } }
    };
    std::unordered_map<std::string, int> dither_to_index = {
        { "none", 0 },
        { "ordered", 1 },
        { "random", 2 },
        { "error_diffusion", 3 }
    };
    const Depth &depth = project->getBitDepth();
    {
        QSignalBlocker block(depth_box);
        depth_box->setChecked(depth.enabled);
    }
    depth_bits_combo->setCurrentIndex(bits_to_index[(int)depth.float_samples][depth.bits]);
    depth_dither_combo->setCurrentIndex(dither_to_index[depth.dither]);


    connect(crop_dock, &DockWidget::visibilityChanged, [this] {
        if (!project)
            return;

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }
    });
}


void WobblyWindow::initialisePresetEditor() {
    preset_combo->setModel(project->getPresetsModel());

    if (preset_combo->count()) {
        preset_combo->setCurrentIndex(0);
        presetChanged(preset_combo->currentText());
    }
}


void WobblyWindow::initialiseSectionsEditor() {
    QItemSelectionModel *m = preset_list->selectionModel();
    preset_list->setModel(project->getPresetsModel());
    delete m;

    sections_proxy_model->setSourceModel(project->getSectionsModel());

    connect(sections_view->selectionModel(), &QItemSelectionModel::currentRowChanged, [this] (const QModelIndex &current, const QModelIndex &previous) {
        (void)previous;

        section_presets_list->clear();

        if (!current.isValid())
            return;

        bool ok;
        int frame = sections_view->model()->data(sections_view->model()->index(current.row(), 0)).toInt(&ok);
        if (ok) {
            const Section *section = project->findSection(frame);
            for (auto it = section->presets.cbegin(); it != section->presets.cend(); it++)
                section_presets_list->addItem(QString::fromStdString(*it));
        }
    });

    connect(project->getSectionsModel(), &SectionsModel::dataChanged, [this] (const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        QModelIndex current_index = sections_view->currentIndex();

        if (!current_index.isValid())
            return;

        if (current_index.row() < topLeft.row() || current_index.row() > bottomRight.row() ||
            SectionsModel::PresetsColumn < topLeft.column() || SectionsModel::PresetsColumn > bottomRight.column())
            return;

        section_presets_list->clear();
        bool ok;
        int frame = sections_view->model()->data(sections_view->model()->index(current_index.row(), 0)).toInt(&ok);
        if (ok) {
            const Section *section = project->findSection(frame);
            for (auto it = section->presets.cbegin(); it != section->presets.cend(); it++)
                section_presets_list->addItem(QString::fromStdString(*it));
        }
    });
}


void WobblyWindow::initialiseCustomListsEditor() {
    cl_view->setModel(project->getCustomListsModel());
    cl_presets_box->setModel(project->getPresetsModel());

    cl_view->resizeColumnsToContents();

    connect(cl_view->selectionModel(), &QItemSelectionModel::currentRowChanged, [this] (const QModelIndex &current, const QModelIndex &previous) {
        (void)previous;

        const CustomListsModel *cl = project->getCustomListsModel();

        if (!cl->size())
            return;

        if (!current.isValid())
            return;

        const CustomList &list = cl->at(current.row());

        cl_position_group->button(list.position)->setChecked(true);

        cl_presets_box->setCurrentText(QString::fromStdString(list.preset));

        cl_ranges_view->setModel(list.ranges.get());
    });
}


void WobblyWindow::updateFrameRatesViewer() {
    auto selection = frame_rates_table->selectedRanges();

    int row_count_before = frame_rates_table->rowCount();

    int verScrollValue = frame_rates_table->verticalScrollBar()->value();

    frame_rates_table->setRowCount(0);

    auto ranges = project->getDecimationRanges();

    int rows = 0;
    for (size_t i = 0; i < ranges.size(); i++) {
        if (frame_rates_buttons->button(ranges[i].num_dropped)->isChecked()) {
            rows++;
            frame_rates_table->setRowCount(rows);

            QTableWidgetItem *item = new QTableWidgetItem(QString::number(ranges[i].start));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 0, item);

            int end;
            if (i < ranges.size() - 1)
                end = ranges[i + 1].start - 1;
            else
                end = project->getNumFrames(PostSource) - 1;

            item = new QTableWidgetItem(QString::number(end));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 1, item);

            const char *rates[] = {
                "30000/1001",
                "24000/1001",
                "18000/1001",
                "12000/1001",
                "6000/1001"
            };
            item = new QTableWidgetItem(rates[ranges[i].num_dropped]);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 2, item);

            item = new QTableWidgetItem(QString::number(end - ranges[i].start + 1));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            frame_rates_table->setItem(rows - 1, 3, item);
        }
    }

    frame_rates_table->resizeColumnsToContents();

    int row_count_after = frame_rates_table->rowCount();

    if (row_count_before == row_count_after) {
        for (int i = 0; i < selection.size(); i++)
            frame_rates_table->setRangeSelected(selection[i], true);
    } else if (row_count_after) {
        frame_rates_table->verticalScrollBar()->setValue(verScrollValue);
    }
}


void WobblyWindow::initialiseFrameRatesViewer() {
    auto rates = project->getShownFrameRates();

    for (int i = 0; i < 5; i++)
        frame_rates_buttons->button(i)->setChecked(rates[i]);

    updateFrameRatesViewer();
}


void WobblyWindow::initialiseFrozenFramesViewer() {
    frozen_frames_view->setModel(project->getFrozenFramesModel());

    frozen_frames_view->resizeColumnsToContents();
}


void WobblyWindow::updatePatternGuessingWindow() {
    pg_failures_table->setRowCount(0);

    const char *reasons[] = {
        "Section too short",
        "Ambiguous pattern"
    };

    auto pg = project->getPatternGuessing();

    pg_failures_table->setRowCount(pg.failures.size());

    int rows = 0;

    for (auto it = pg.failures.cbegin(); it != pg.failures.cend(); it++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(it->second.start));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pg_failures_table->setItem(rows, 0, item);

        item = new QTableWidgetItem(reasons[it->second.reason]);
        pg_failures_table->setItem(rows, 1, item);

        rows++;
    }

    pg_failures_table->resizeColumnsToContents();
}


void WobblyWindow::initialisePatternGuessingWindow() {
    auto pg = project->getPatternGuessing();

    if (pg.failures.size()) {
        pg_methods_buttons->button(pg.method)->setChecked(true);

        pg_length_spin->setValue(pg.minimum_length);
        pg_edge_cutoff->setValue(pg.edge_cutoff);

        pg_n_match_buttons->button(pg.third_n_match)->setChecked(true);

        pg_decimate_buttons->button(pg.decimation)->setChecked(true);

        auto buttons = pg_use_patterns_buttons->buttons();
        for (int i = 0; i < buttons.size(); i++)
            buttons[i]->setChecked(pg.use_patterns & pg_use_patterns_buttons->id(buttons[i]));
    }

    updatePatternGuessingWindow();
}


void WobblyWindow::initialiseMicSearchWindow() {
    {
        QSignalBlocker block(mic_search_minimum_spin);
        mic_search_minimum_spin->setValue(project->getMicSearchMinimum());
    }
}


void WobblyWindow::initialiseDMetricSearchWindow() {
    {
        QSignalBlocker block(dmetric_search_minimum_spin);
        dmetric_search_minimum_spin->setValue(project->getDMetricSearchMinimum());
    }
}

void WobblyWindow::updateCMatchSequencesWindow() {
    const auto &sequences = project->getCMatchSequences(c_match_minimum_spin->value());

    c_match_sequences_table->setRowCount(0);
    c_match_sequences_table->setRowCount(sequences.size());

    int row = 0;
    for (auto it = sequences.cbegin(); it != sequences.cend(); it++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(it->first));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        c_match_sequences_table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(it->second));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        c_match_sequences_table->setItem(row, 1, item);

        row++;
    }

    if (sequences.size())
        c_match_sequences_table->selectRow(0);

    c_match_sequences_table->resizeColumnsToContents();
}


void WobblyWindow::initialiseCMatchSequencesWindow() {
    {
        QSignalBlocker block(c_match_minimum_spin);
        c_match_minimum_spin->setValue(project->getCMatchSequencesMinimum());
    }

    updateCMatchSequencesWindow();
}


void WobblyWindow::updateFadesWindow() {
    auto fades = project->getInterlacedFades();

    int ignore_gaps = fades_gaps_spin->value();

    std::vector<FrameRange> fades_ranges;

    auto it = fades.cbegin();
    if (it == fades.cend()) {
        fades_table->setRowCount(0);
        return;
    }

    int start = it->first;
    int end = start;

    it++;
    for ( ; it != fades.cend(); it++) {
        if (it->first - end - 1 > ignore_gaps) {
            fades_ranges.push_back({ start, end });
            start = it->first;
            end = start;
        } else {
            end = it->first;
        }
        if (it == (fades.cend()--))
            fades_ranges.push_back({ start, end });
    }

    fades_table->setRowCount(fades_ranges.size());

    for (auto range = fades_ranges.cbegin(); range != fades_ranges.cend(); range++) {
        int row = std::distance(fades_ranges.cbegin(), range);

        QTableWidgetItem *item = new QTableWidgetItem(QString::number(range->first));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fades_table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(range->last));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fades_table->setItem(row, 1, item);
    }
}


void WobblyWindow::initialiseCombedFramesWindow() {
    combed_view->setModel(project->getCombedFramesModel());

    combed_view->resizeColumnsToContents();
}


void WobblyWindow::initialiseOrphanFieldsWindow() {
    orphan_view->setModel(project->getOrphanFieldsModel());

    orphan_view->resizeColumnsToContents();
}


void WobblyWindow::initialiseBookmarksWindow() {
    bookmarks_view->setModel(project->getBookmarksModel());

    connect(project->getBookmarksModel(), &BookmarksModel::dataChanged, [this] (const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        commit("Rename bookmark");
        if (topLeft == bottomRight) {
            int frame = bookmarks_view->model()->index(topLeft.row(), BookmarksModel::FrameColumn).data().toInt();

            if (frame == current_frame)
                updateFrameDetails();
        }
    });
}


void WobblyWindow::initialiseUIFromProject() {
    updateWindowTitle();

    tab_bar->setEnabled(true);

    frame_slider->setRange(0, project->getNumFrames(PostSource) - 1);
    frame_slider->setPageStep(project->getNumFrames(PostSource) * 20 / 100);

    // Zoom.
    zoom_label->setText(QStringLiteral("Zoom: %1x").arg(project->getZoom()));

    updateGeometry();

    project->setUndoSteps(size_t(settings.value(KEY_UNDO_STEPS, 50).toInt()));
    project->updateOrphanFields();

    initialiseCropAssistant();
    initialisePresetEditor();
    initialiseSectionsEditor();
    initialiseCustomListsEditor();
    initialiseFrameRatesViewer();
    initialiseFrozenFramesViewer();
    initialisePatternGuessingWindow();
    initialiseMicSearchWindow();
    initialiseDMetricSearchWindow();
    initialiseCMatchSequencesWindow();
    updateFadesWindow();
    initialiseCombedFramesWindow();
    initialiseOrphanFieldsWindow();
    initialiseBookmarksWindow();
}


void WobblyWindow::realOpenProject(const QString &path) {
    WobblyProject *tmp = new WobblyProject(true);

    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);

        tmp->readProject(path.toStdString());

        QApplication::restoreOverrideCursor();

        project_path = path;
        video_path.clear();

        if (project)
            delete project;
        project = tmp;

        addRecentFile(path);

        current_frame = project->getLastVisitedFrame();

        initialiseUIFromProject();
        project->commit("Initial");

        vssapi->evaluateBuffer(vsscript, "vs.clear_output(1)", "wobbly.cleanup");

        connect(project, &WobblyProject::modifiedChanged, this, &WobblyWindow::updateWindowTitle);

        evaluateMainDisplayScript();
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());

        if (tmp != project) {
            // If readProject failed then we don't need tmp.
            // We carry on with whatever project was open previously (if any).
            delete tmp;
        } else {
            // If it was evaluateMainDisplayScript that failed, then just request the current frame.
            // Obviously it won't display anything, but it will update the user interface.
            requestFrames(current_frame);
        }
    }
}


void WobblyWindow::openFile(const QString &path) {
    if (path.endsWith(".wob") || path.endsWith(".json"))
        realOpenProject(path);
    else
        realOpenVideo(path);
}


void WobblyWindow::openProject() {
    if (askToSaveIfModified() == QMessageBox::Cancel)
        return;

    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Wobbly project"), settings.value(KEY_LAST_DIR).toString(), QStringLiteral("Wobbly projects (*.wob);;All files (*)"));

    if (!path.isNull()) {
        settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

        realOpenProject(path);
    }
}

const char *WobblyWindow::getArgsForSourceFilter(const QString &source_filter) {
    if (source_filter == "bs.VideoSource")
        return ", rff=True";
    return "";
}

void WobblyWindow::realOpenVideo(const QString &path) {
    try {
        QString source_filter;

        QString extension = path.mid(path.lastIndexOf('.') + 1);

        if (extension == "dgi")
            source_filter = "dgdecodenv.DGSource";
        else if (extension == "d2v")
            source_filter = "d2v.Source";
        else
            source_filter = "bs.VideoSource";

        QString script = QStringLiteral(
                    "import vapoursynth as vs\n"
                    "\n"
                    "c = vs.core\n"
                    "\n"
                    "c.%1(r'%2'%3).set_output()\n");
        script = script
            .arg(source_filter)
            .arg(QString::fromStdString(handleSingleQuotes(path.toStdString())))
            .arg(getArgsForSourceFilter(source_filter));

        QApplication::setOverrideCursor(Qt::WaitCursor);


        if (vssapi->evaluateBuffer(vsscript, script.toUtf8().constData(), path.toUtf8().constData())) {
            std::string error = vssapi->getError(vsscript);
            // The traceback is mostly unnecessary noise.
            size_t traceback = error.find("Traceback");
            if (traceback != std::string::npos)
                error.insert(traceback, 1, '\n');

            QApplication::restoreOverrideCursor();

            throw WobblyException("Can't extract basic information from the video file: script evaluation failed. Error message:\n" + error);
        }

        QApplication::restoreOverrideCursor();

        VSNode *node = vssapi->getOutputNode(vsscript, 0);
        if (!node)
            throw WobblyException("Can't extract basic information from the video file: script evaluated successfully, but no node found at output index 0.");

        VSVideoInfo vi = *vsapi->getVideoInfo(node);

        vsapi->freeNode(node);

        if (project)
            delete project;

        video_path = path;
        if (settings_use_relative_paths_check->isChecked())
            video_path = QFileInfo(path).fileName();

        project = new WobblyProject(true, video_path.toStdString(), source_filter.toStdString(), vi.fpsNum, vi.fpsDen, vi.width, vi.height, vi.numFrames);
        project->addTrim(0, vi.numFrames - 1);

        video_path = path;
        project_path.clear();

        initialiseUIFromProject();
        project->commit("Initial");

        vssapi->evaluateBuffer(vsscript, "vs.clear_output(1)", "wobbly.cleanup");

        evaluateMainDisplayScript();

        addRecentFile(path);

        connect(project, &WobblyProject::modifiedChanged, this, &WobblyWindow::updateWindowTitle);
    } catch(WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::openVideo() {
    if (askToSaveIfModified() == QMessageBox::Cancel)
        return;

    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open video file"), settings.value(KEY_LAST_DIR).toString());

    if (!path.isNull()) {
        settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

        realOpenVideo(path);
    }
}


void WobblyWindow::realSaveProject(const QString &path) {
    if (!project)
        return;

    // The currently selected preset might not have been stored in the project yet.
    presetEdited();

    project->setLastVisitedFrame(current_frame);

    const QByteArray &state = saveState().toBase64();
    const QByteArray &geometry = saveGeometry().toBase64();
    project->setUIState(std::string(state.constData(), state.size()));
    project->setUIGeometry(std::string(geometry.constData(), geometry.size()));

    QApplication::setOverrideCursor(Qt::WaitCursor);

    try {
        project->writeProject(path.toStdString(), settings_compact_projects_check->isChecked());
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        throw e;
    }

    QApplication::restoreOverrideCursor();

    project_path = path;
    video_path.clear();

    updateWindowTitle();

    addRecentFile(path);
}


void WobblyWindow::saveProject() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        if (project_path.isEmpty())
            saveProjectAs();
        else
            realSaveProject(project_path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveProjectAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Wobbly project"), settings.value(KEY_LAST_DIR).toString(), QStringLiteral("Wobbly projects (*.wob);;All files (*)"));

        if (!path.isNull()) {
            settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

            realSaveProject(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::realSaveScript(const QString &path) {
    // The currently selected preset might not have been stored in the project yet.
    presetEdited();

    FinalScriptFormat format{};

    QString decimation_function = settings_decimation_function_combo->currentText();
    if (decimation_function == "SelectEvery")
        format.decimation_function = DecimationFunction::SELECTEVERY;
    else if (decimation_function == "DeleteFrames")
        format.decimation_function = DecimationFunction::DELETEFRAMES;
    else
        format.decimation_function = DecimationFunction::AUTO;

    std::string script = project->generateFinalScript(false, format);

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open script '" + path.toStdString() + "'. Error message: " + file.errorString().toStdString());

    file.write(script.c_str(), script.size());
}


void WobblyWindow::saveScript() {
    try {
        if (!project)
            throw WobblyException("Can't save the script because no project has been loaded.");

        QString path;
        if (project_path.isEmpty())
            path = video_path;
        else
            path = project_path;
        path += ".vpy";

        realSaveScript(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveScriptAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the script because no project has been loaded.");

        QString dir;
        if (project_path.isEmpty())
            dir = video_path;
        else
            dir = project_path;
        dir += ".vpy";

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save script"), dir, QStringLiteral("VapourSynth scripts (*.py *.vpy);;All files (*)"));

        if (!path.isNull()) {
            settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

            realSaveScript(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::realSaveTimecodes(const QString &path) {
    std::string tc = project->generateTimecodesV1();

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open timecodes file '" + path.toStdString() + "'. Error message: " + file.errorString().toStdString());

    file.write(tc.c_str(), tc.size());
}


void WobblyWindow::realSaveSections(const QString &path) {
    std::string tc = project->generateKeyframesV1();

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open timecodes file '" + path.toStdString() + "'. Error message: " + file.errorString().toStdString());

    file.write(tc.c_str(), tc.size());
}


void WobblyWindow::saveTimecodes() {
    try {
        if (!project)
            throw WobblyException("Can't save the timecodes because no project has been loaded.");

        QString path;
        if (project_path.isEmpty())
            path = video_path;
        else
            path = project_path;
        path += ".vfr.txt";

        realSaveTimecodes(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveTimecodesAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the timecodes because no project has been loaded.");

        QString dir;
        if (project_path.isEmpty())
            dir = video_path;
        else
            dir = project_path;
        dir += ".vfr.txt";

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save timecodes"), dir, QStringLiteral("Timecodes v1 files (*.txt);;All files (*)"));

        if (!path.isNull()) {
            settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

            realSaveTimecodes(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveScreenshot() {
    QString path;
    if (project_path.isEmpty() && video_path.isEmpty())
        path = "wobbly";
    else if (project_path.isEmpty())
        path = video_path;
    else
        path = project_path;
    path += ".png";

    path = QFileDialog::getSaveFileName(this, QStringLiteral("Save screenshot"), path, QStringLiteral("PNG images (*.png);;All files (*)"));

    if (!path.isNull()) {
        settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

        frame_label->pixmap(Qt::ReturnByValue).scaled(original_frame_width, original_frame_height, Qt::IgnoreAspectRatio, Qt::FastTransformation).save(path, "png");
    }
}


void WobblyWindow::saveSections() {
    try {
        if (!project)
            throw WobblyException("Can't save the sections because no project has been loaded.");

        QString path;
        if (project_path.isEmpty())
            path = video_path;
        else
            path = project_path;
        path += ".txt";

        realSaveSections(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveSectionsAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the sections because no project has been loaded.");

        QString dir;
        if (project_path.isEmpty())
            dir = video_path;
        else
            dir = project_path;
        dir += ".txt";

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save sections"), dir, QStringLiteral("Keyframes v1 files (*.txt);;All files (*)"));

        if (!path.isNull()) {
            settings.setValue(KEY_LAST_DIR, QFileInfo(path).absolutePath());

            realSaveSections(path);
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


QMessageBox::StandardButton WobblyWindow::askToSaveIfModified() {
    QMessageBox::StandardButton answer = QMessageBox::NoButton;

    if (project && project->isModified()) {
        answer = QMessageBox::question(this, QStringLiteral("Save?"), QStringLiteral("Save project?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

        if (answer == QMessageBox::Yes)
            saveProject();
    }

    return answer;
}


QString getPreviousInSeries(const QString &path) {
    QFileInfo info(path);
    QDir dir = info.dir();
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    QString this_project = info.fileName();

    for (int i = 0; i < files.size(); ) {
        if (files[i].size() != this_project.size())
            files.removeAt(i);
        else
            i++;
    }

    for (int i = 0; i < files.size(); ) {
        if (files[i] == this_project) {
            i++;
            continue;
        }

        bool belongs = true;

        for (int j = 0; j < this_project.size(); j++) {
            if (!((this_project[j].isDigit() && files[i][j].isDigit()) || (this_project[j] == files[i][j]))) {
                belongs = false;
                break;
            }
        }

        if (!belongs)
            files.removeAt(i);
        else
            i++;
    }

    QString previous_name;

    int index = files.indexOf(this_project);
    if (index > 0)
        previous_name = dir.absoluteFilePath(files[index - 1]);

    return previous_name;
}


void WobblyWindow::importFromProject() {
    if (!import_window) {
        QString previous_name;

        if (!project_path.isEmpty())
            previous_name = getPreviousInSeries(project_path);

        ImportedThings import_things;
        import_things.geometry = true;
        import_things.zoom = true;
        import_things.presets = true;
        import_things.custom_lists = true;
        import_things.crop = true;
        import_things.resize = true;
        import_things.bit_depth = true;
        import_things.mic_search = true;

        import_window = new ImportWindow(previous_name, import_things, this);

        connect(import_window, &ImportWindow::import, [this] (const QString &file_name, const ImportedThings &imports) {
            if (!project)
                return;

            try {
                project->importFromOtherProject(file_name.toStdString(), imports);

                initialiseUIFromProject();

                requestFrames(current_frame);

                import_window->hide();
            } catch (WobblyException &e) {
                errorPopup(e.what());
            }
        });

        connect(import_window, &ImportWindow::previousWanted, [this] () {
            QString prev;

            if (!project_path.isEmpty())
                prev = getPreviousInSeries(project_path);

            import_window->setFileName(prev);
        });
    }

    import_window->show();
    import_window->raise();
    import_window->activateWindow();
}


void WobblyWindow::quit() {
    close();
}


void WobblyWindow::showHideFrameDetails() {
    details_dock->setVisible(!details_dock->isVisible());
}


void WobblyWindow::showHideCropping() {
    crop_dock->setVisible(!crop_dock->isVisible());
}


void WobblyWindow::showHidePresets() {
    preset_dock->setVisible(!preset_dock->isVisible());
}


void WobblyWindow::showHidePatternEditor() {
    pattern_dock->setVisible(!pattern_dock->isVisible());
}


void WobblyWindow::showHideSections() {
    sections_dock->setVisible(!sections_dock->isVisible());
}


void WobblyWindow::showHideCustomLists() {
    cl_dock->setVisible(!cl_dock->isVisible());
}


void WobblyWindow::showHideFrameRates() {
    frame_rates_dock->setVisible(!frame_rates_dock->isVisible());
}


void WobblyWindow::showHideFrozenFrames() {
    frozen_frames_dock->setVisible(!frozen_frames_dock->isVisible());
}


void WobblyWindow::showHidePatternGuessing() {
    pg_dock->setVisible(!pg_dock->isVisible());
}


void WobblyWindow::showHideMicSearchWindow() {
    mic_search_dock->setVisible(!mic_search_dock->isVisible());
}


void WobblyWindow::showHideCMatchSequencesWindow() {
    c_match_sequences_dock->setVisible(!c_match_sequences_dock->isVisible());
}


void WobblyWindow::showHideFadesWindow() {
    fades_dock->setVisible(!fades_dock->isVisible());
}


void WobblyWindow::showHideCombedFramesWindow() {
    combed_dock->setVisible(!combed_dock->isVisible());
}


void WobblyWindow::showHideOrphanFieldsWindow() {
    orphan_dock->setVisible(!orphan_dock->isVisible());
}


void WobblyWindow::showHideBookmarksWindow() {
    bookmarks_dock->setVisible(!bookmarks_dock->isVisible());
}


void WobblyWindow::showHideFrameDetailsOnVideo() {
    settings_print_details_check->setChecked(!settings_print_details_check->isChecked());
}


void WobblyWindow::evaluateScript(bool final_script) {
    std::string script;

    if (final_script)
        script = project->generateFinalScript();
    else
        script = project->generateMainDisplayScript();

    QString m = settings_colormatrix_combo->currentText();
    std::string matrix = "709";
    std::string transfer = "709";
    std::string primaries = "709";

    if (m == "BT 601") {
        matrix = "470bg";
        transfer = "601";
        primaries = "170m";
    } else if (m == "BT 709") {
        matrix = "709";
        transfer = "709";
        primaries = "709";
    } else if (m == "BT 2020 NCL") {
        matrix = "2020ncl";
        transfer = "709";
        primaries = "2020";
    } else if (m == "BT 2020 CL") {
        matrix = "2020cl";
        transfer = "709";
        primaries = "2020";
    }

    script +=
            "src = vs.get_output(index=0)\n"

            "if isinstance(src, vs.VideoOutputTuple):\n"
            "    src = src[0]\n"

            "if src.format is None:\n"
            "    raise vs.Error('The output clip has unknown format. Wobbly cannot display such clips.')\n";

    if (crop_dock->isVisible() && project->isCropEnabled() && !final_script) {
        script += "src = c.std.CropRel(clip=src, left=";
        script += std::to_string(crop_spin[0]->value()) + ", top=";
        script += std::to_string(crop_spin[1]->value()) + ", right=";
        script += std::to_string(crop_spin[2]->value()) + ", bottom=";
        script += std::to_string(crop_spin[3]->value()) + ")\n";

        script +=
                "src = c.resize.Bicubic(clip=src, format=vs.RGB24, dither_type='random', matrix_in_s='" + matrix + "', transfer_in_s='" + transfer + "', primaries_in_s='" + primaries + "')\n";

        script += "src = c.std.AddBorders(clip=src, left=";
        script += std::to_string(crop_spin[0]->value()) + ", top=";
        script += std::to_string(crop_spin[1]->value()) + ", right=";
        script += std::to_string(crop_spin[2]->value()) + ", bottom=";
        script += std::to_string(crop_spin[3]->value()) + ", color=[224, 81, 255])\n";

        script += "c.query_video_format(vs.GRAY, vs.INTEGER, 32, 0, 0)\n";
    } else {
        script +=
            "c.query_video_format(vs.GRAY, vs.INTEGER, 32, 0, 0)\n"
            "src = c.resize.Bicubic(clip=src, format=vs.RGB24, dither_type='random', matrix_in_s='" + matrix + "', transfer_in_s='" + transfer + "', primaries_in_s='" + primaries + "')\n";
    }

    script +=
            "src.set_output()\n";

    script +=
            "c.max_cache_size = " + std::to_string(settings_cache_spin->value()) + "\n";

    if (vssapi->evaluateBuffer(vsscript, script.c_str(), (project_path.isEmpty() ? video_path : project_path).toUtf8().constData())) {
        std::string error = vssapi->getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.insert(traceback, 1, '\n');

        throw WobblyException("Failed to evaluate " + std::string(final_script ? "final" : "main display") + " script. Error message:\n" + error);
    }

    int node_index = (int)final_script;

    vsapi->freeNode(vsnode[node_index]);

    vsnode[node_index] = vssapi->getOutputNode(vsscript, 0);
    if (!vsnode[node_index])
        throw WobblyException(std::string(final_script ? "Final" : "Main display") + " script evaluated successfully, but no node found at output index 0.");

    requestFrames(current_frame);
}


void WobblyWindow::evaluateMainDisplayScript() {
    evaluateScript(false);
}


void WobblyWindow::evaluateFinalScript() {
    evaluateScript(true);
}


void VS_CC frameDoneCallback(void *userData, const VSFrame *f, int n, VSNode *, const char *errorMsg) {
    CallbackData *callback_data = (CallbackData *)userData;

    callback_data->vsapi->freeNode(callback_data->node);

    // Qt::DirectConnection = frameDone runs in the worker threads
    // Qt::QueuedConnection = frameDone runs in the GUI thread
    QMetaObject::invokeMethod(callback_data->window, "frameDone", Qt::QueuedConnection,
                              Q_ARG(void *, (void *)f),
                              Q_ARG(int, n),
                              Q_ARG(bool, callback_data->preview_node),
                              Q_ARG(QString, QString(errorMsg)));
    // Pass a copy of the error message because the pointer won't be valid after this function returns.

    delete callback_data;
}


void WobblyWindow::requestFrames(int n) {
    n = std::max(0, std::min(n, project->getNumFrames(PostSource) - 1));

    current_frame = n;

    {
        QSignalBlocker block(frame_slider);
        frame_slider->setValue(n);
    }

    current_pict_type.clear();
    updateFrameDetails();

    if (!vsnode[(int)preview])
        return;

    if (pending_requests && pending_requests_node == vsnode[(int)preview])
        return;

    pending_frame = n;

    int frame_num = n;
    int last_frame = project->getNumFrames(PostSource) - 1;
    if (preview) {
        frame_num = project->frameNumberAfterDecimation(n);
        last_frame = project->getNumFrames(PostDecimate) - 1;
    }

    int num_thumbnails = settings_num_thumbnails_spin->value();
    int first_visible = (MAX_THUMBNAILS - num_thumbnails) / 2;
    int last_visible = first_visible + num_thumbnails - 1;

    for (int i = 0; i < num_thumbnails / 2 - frame_num; i++)
        thumb_labels[first_visible + i]->setPixmap(splash_thumb);

    for (int i = 0; i < num_thumbnails / 2 - (last_frame - frame_num); i++)
        thumb_labels[last_visible - i]->setPixmap(splash_thumb);

    pending_requests_node = vsnode[(int)preview];

    for (int i = std::max(0, frame_num - num_thumbnails / 2); i < std::min(frame_num + num_thumbnails / 2 + 1, last_frame + 1); i++) {
        pending_requests++;
        CallbackData *callback_data = new CallbackData(this, vsapi->addNodeRef(vsnode[(int)preview]), preview, vsapi);
        vsapi->getFrameAsync(i, vsnode[(int)preview], frameDoneCallback, (void *)callback_data);
    }

    // restoreOverrideCursor called in frameDone
    QApplication::setOverrideCursor(Qt::BusyCursor);
}


// Runs in the GUI thread.
void WobblyWindow::frameDone(void *framev, int n, bool preview_node, const QString &errorMsg) {
    const VSFrame *frame = (const VSFrame *)framev;

    pending_requests--;

    if (!frame) {
        // setOverrideCursor called in requestFrames
        QApplication::restoreOverrideCursor();

        errorPopup(QStringLiteral("Failed to retrieve frame %1. Error message: %2").arg(n).arg(errorMsg).toUtf8().constData());

        return;
    }

    // error pointer must be non-null to enable non-exceptional return in case of missing/bad property
    int pict_type_error;
    const char *pict_type_data = vsapi->mapGetData(vsapi->getFramePropertiesRO(frame), "_PictType", 0, &pict_type_error);
    QString pict_type(pict_type_data ? pict_type_data : "&lt;unknown&gt;");

    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    uint8_t *frame_data = packRGBFrame(vsapi, frame);
    vsapi->freeFrame(frame);

    QImage image = QImage(frame_data, width, height, width * 4, QImage::Format_RGB32, free, frame_data);

    int offset;
    if (preview_node)
        offset = n - project->frameNumberAfterDecimation(pending_frame);
    else
        offset = n - pending_frame;

    if (offset == 0) {
        int zoom = project->getZoom();
        frame_label->setPixmap(QPixmap::fromImage(image).scaled(width * zoom, height * zoom, Qt::IgnoreAspectRatio, Qt::FastTransformation));

        // setOverrideCursor called in requestFrames
        QApplication::restoreOverrideCursor();

        current_pict_type = pict_type;
        original_frame_width = width;
        original_frame_height = height;

        // current_pict_type has changed
        updateFrameDetails();
    }

    thumb_labels[offset + MAX_THUMBNAILS / 2]->setPixmap(getThumbnail(image));

    if (!pending_requests && pending_frame != current_frame)
        requestFrames(current_frame);
}


void WobblyWindow::updateFrameDetails() {
    if (!project)
        return;

    QString frame("Frame: ");

    if (!preview)
        frame += "<b>";

    frame += QString::number(current_frame);

    if (!preview)
        frame += "</b>";

    frame += " | ";

    if (preview)
        frame += "<b>";

    frame += QString::number(project->frameNumberAfterDecimation(current_frame));

    if (preview)
        frame += "</b>";

    frame_num_label->setText(frame);


    time_label->setText(QString::fromStdString("Time: " + project->frameToTime(current_frame)));


    int matches_start = std::max(0, current_frame - 10);
    int matches_end = std::min(current_frame + 10, project->getNumFrames(PostSource) - 1);

    QString matches("Matches: ");
    for (int i = matches_start; i <= matches_end; i++) {
        char match = project->getMatch(i);

        bool is_decimated = project->isDecimatedFrame(i);

        if (i % 5 == 0)
            matches += "<u>";

        if (is_decimated)
            matches += "<s>";

        if (i == current_frame)
            match += 'C' - 'c';
        matches += match;

        if (is_decimated)
            matches += "</s>";

        if (i % 5 == 0)
            matches += "</u>";
    }
    matches_label->setText(matches);


    if (project->isCombedFrame(current_frame))
        combed_label->setText(QStringLiteral("Combed"));
    else
        combed_label->clear();


    decimate_metric_label->setText(QStringLiteral("DMetric: ") + QString::number(project->getDecimateMetric(current_frame)));


    int match_index2 = matchCharToIndexDMetrics(project->getMatch(current_frame));
    QString mmetrics("MMetrics: ");
    for (int i = 0; i < 3; i++) {
        if (i == match_index2)
            mmetrics += "<b>";

        mmetrics += QStringLiteral("%1 ").arg((int)project->getMMetrics(current_frame)[i]);

        if (i == match_index2)
            mmetrics += "</b>";
    }
    mmetric_label->setText(mmetrics);

    QString vmetrics("VMetrics: ");
    for (int i = 0; i < 3; i++) {
        if (i == match_index2)
            vmetrics += "<b>";

        vmetrics += QStringLiteral("%1 ").arg((int)project->getVMetrics(current_frame)[i]);

        if (i == match_index2)
            vmetrics += "</b>";
    }
    vmetric_label->setText(vmetrics);

    int match_index = matchCharToIndex(project->getMatch(current_frame));
    QString mics("Mics: ");
    for (int i = 0; i < 5; i++) {
        if (i == match_index)
            mics += "<b>";

        mics += QStringLiteral("%1 ").arg((int)project->getMics(current_frame)[i]);

        if (i == match_index)
            mics += "</b>";
    }
    mic_label->setText(mics);


    const Section *current_section = project->findSection(current_frame);
    int section_start = current_section->start;
    int section_end = project->getSectionEnd(section_start) - 1;
    int section_length = section_end - section_start + 1;

    QString presets;
    for (auto it = current_section->presets.cbegin(); it != current_section->presets.cend(); it++) {
        if (!presets.isEmpty())
            presets += "\n";
        presets += QString::fromStdString(*it);
    }

    if (presets.isNull())
        presets = "&lt;none&gt;";

    int section_length_after_decimation = project->frameNumberAfterDecimation(section_end + 1) - project->frameNumberAfterDecimation(section_start);
    if (section_end + 1 == project->getNumFrames(PostSource) - 1 && project->isDecimatedFrame(section_end + 1))
        section_length_after_decimation++;

    section_label->setText(QStringLiteral("Section: [%1,%2] = %3 | %4<br />Presets:<br />%5").arg(section_start).arg(section_end).arg(section_length).arg(section_length_after_decimation).arg(presets));


    QString custom_lists;
    const CustomListsModel *lists = project->getCustomListsModel();
    for (size_t i = 0; i < lists->size(); i++) {
        const FrameRange *range = project->findCustomListRange(i, current_frame);
        if (range) {
            if (!custom_lists.isEmpty())
                custom_lists += "\n";
            custom_lists += QStringLiteral("%1: [%2,%3]").arg(QString::fromStdString(lists->at(i).name)).arg(range->first).arg(range->last);
        }
    }

    if (custom_lists.isNull())
        custom_lists = "&lt;none&gt;";

    custom_list_label->setText(QStringLiteral("Custom lists:<br />%1").arg(custom_lists));


    const FreezeFrame *freeze = project->findFreezeFrame(current_frame);
    if (freeze) {
        QString strike_open, strike_close;

        if (!project->getFreezeFramesWanted() && !preview) {
            strike_open = "<s>";
            strike_close = "</s>";
        }

        freeze_label->setText(QStringLiteral("%1Frozen: [%2,%3,%4]%5").arg(strike_open).arg(freeze->first).arg(freeze->last).arg(freeze->replacement).arg(strike_close));
    } else {
        freeze_label->clear();
    }


    pict_type_label->setText(QStringLiteral("Picture type: ") + current_pict_type);


    const Bookmark *bookmark = project->getBookmark(current_frame);

    if (bookmark) {
        if (bookmark->description.size())
            bookmark_label->setText(QStringLiteral("Bookmark: ") + QString::fromStdString(bookmark->description));
        else
            bookmark_label->setText(QStringLiteral("Bookmark"));
    } else {
        bookmark_label->clear();
    }


    if (settings_print_details_check->isChecked()) {
        QString drawn_text = frame_num_label->text() + "<br />";
        drawn_text += time_label->text() + "<br />";
        drawn_text += matches_label->text() + "<br />";
        drawn_text += section_label->text() + "<br />";
        drawn_text += custom_list_label->text() + "<br />";
        drawn_text += freeze_label->text() + "<br />";
        drawn_text += decimate_metric_label->text() + "<br />";
        drawn_text += mmetric_label->text() + "<br />";
        drawn_text += vmetric_label->text() + "<br />";
        drawn_text += mic_label->text() + "<br />";
        drawn_text += pict_type_label->text() + "<br />";
        drawn_text += combed_label->text() + "<br />";
        drawn_text += bookmark_label->text();

        overlay_label->setText(drawn_text);
    } else {
        overlay_label->clear();
    }
}


void WobblyWindow::jumpRelative(int offset) {
    if (!project)
        return;

    int target = current_frame + offset;

    if (target < 0)
        target = 0;
    if (target >= project->getNumFrames(PostSource))
        target = project->getNumFrames(PostSource) - 1;

    if (preview) {
        int skip = offset < 0 ? -1 : 1;

        while (true) {
            if (target == 0 || target == project->getNumFrames(PostSource) - 1)
                skip = -skip;

            if (!project->isDecimatedFrame(target))
                break;

            target += skip;
        }
    }

    requestFrames(target);
}


void WobblyWindow::jump1Backward() {
    jumpRelative(-1);
}


void WobblyWindow::jump1Forward() {
    jumpRelative(1);
}


void WobblyWindow::jump5Backward() {
    jumpRelative(-5);
}


void WobblyWindow::jump5Forward() {
    jumpRelative(5);
}


void WobblyWindow::jump50Backward() {
    jumpRelative(-50);
}


void WobblyWindow::jump50Forward() {
    jumpRelative(50);
}


void WobblyWindow::jumpALotBackward() {
    if (!project)
        return;

    int twenty_percent = project->getNumFrames(PostSource) * 20 / 100;

    jumpRelative(-twenty_percent);
}


void WobblyWindow::jumpALotForward() {
    if (!project)
        return;

    int twenty_percent = project->getNumFrames(PostSource) * 20 / 100;

    jumpRelative(twenty_percent);
}


void WobblyWindow::jumpToStart() {
    jumpRelative(0 - current_frame);
}


void WobblyWindow::jumpToEnd() {
    if (!project)
        return;

    jumpRelative(project->getNumFrames(PostSource) - current_frame);
}


void WobblyWindow::jumpToNextSectionStart() {
    if (!project)
        return;

    const Section *next_section = project->findNextSection(current_frame);

    if (next_section)
        jumpRelative(next_section->start - current_frame);
}


void WobblyWindow::jumpToPreviousSectionStart() {
    if (!project)
        return;

    if (current_frame == 0)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start == current_frame)
        section = project->findSection(current_frame - 1);

    jumpRelative(section->start - current_frame);
}


void WobblyWindow::jumpToPreviousMic() {
    if (!project)
        return;

    int frame = project->getPreviousFrameWithMic(mic_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextMic() {
    if (!project)
        return;

    int frame = project->getNextFrameWithMic(mic_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}

void WobblyWindow::jumpToPreviousDMetric() {
    if (!project)
        return;

    int frame = project->getPreviousFrameWithDMetric(dmetric_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextDMetric() {
    if (!project)
        return;

    int frame = project->getNextFrameWithDMetric(dmetric_search_minimum_spin->value(), current_frame);
    if (frame != -1)
        requestFrames(frame);
}

void WobblyWindow::jumpToPreviousBookmark() {
    if (!project)
        return;

    int frame = project->findPreviousBookmark(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextBookmark() {
    if (!project)
        return;

    int frame = project->findNextBookmark(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::jumpToFrame() {
    if (!project)
        return;

    bool ok;
    int frame = QInputDialog::getInt(this, QStringLiteral("Jump to frame"), QStringLiteral("Destination frame:"), current_frame, 0, project->getNumFrames(PostSource) - 1, 1, &ok);
    if (ok)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextCombedFrame() {
    if (!project)
        return;

    int frame = project->findNextCombedFrame(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::jumpToPreviousCombedFrame() {
    if (!project)
        return;

    int frame = project->findPreviousCombedFrame(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::jumpToNextPatternFailureSection() {
    if (!project)
        return;

    int frame = project->findNextAmbiguousPatternSection(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::jumpToPreviousPatternFailureSection() {
    if (!project)
        return;

    int frame = project->findPreviousAmbiguousPatternSection(current_frame);
    if (frame != current_frame)
        requestFrames(frame);
}


void WobblyWindow::cycleMatchCNB() {
    if (!project)
        return;

    project->cycleMatchCNB(current_frame);
    commit("Cycle frame's match");

    updateSectionOrphanFields(current_frame);

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::freezeForward() {
    if (!project)
        return;

    if (current_frame == project->getNumFrames(PostSource) - 1)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame + 1);
        commit("Add freeze frame");

        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
        //statusBar()->showMessage(QStringLiteral("Couldn't freeze forward."), 5000);
    }
}


void WobblyWindow::freezeBackward() {
    if (!project)
        return;

    if (current_frame == 0)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame - 1);
        commit("Add freeze frame");

        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
        //statusBar()->showMessage(QStringLiteral("Couldn't freeze backward."), 5000);
    }
}


void WobblyWindow::freezeRange() {
    if (!project)
        return;

    static FreezeFrame ff = { -1, -1, -1 };

    if (ff.first == -1) {
        if (range_start == -1) {
            ff.first = current_frame;
            ff.last = current_frame;
        } else {
            finishRange();

            ff.first = range_start;
            ff.last = range_end;

            cancelRange();
        }

        freeze_label->setText(QStringLiteral("Freezing [%1,%2]").arg(ff.first).arg(ff.last));
    } else if (ff.replacement == -1) {
        ff.replacement = current_frame;
        try {
            project->addFreezeFrame(ff.first, ff.last, ff.replacement);
            commit("Freeze range");

            evaluateScript(preview);
        } catch (WobblyException &e) {
            updateFrameDetails();

            errorPopup(e.what());
            //statusBar()->showMessage(QStringLiteral("Couldn't freeze range."), 5000);
        }

        ff = { -1, -1, -1 };
    }
}


void WobblyWindow::deleteFreezeFrame() {
    if (!project)
        return;

    const FreezeFrame *ff = project->findFreezeFrame(current_frame);
    if (ff) {
        project->deleteFreezeFrame(ff->first);
        commit("Delete freeze frame");

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::toggleFreezeFrames() {
    if (!project)
        return;

    project->setFreezeFramesWanted(!project->getFreezeFramesWanted());

    if (!preview) {
        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::toggleDecimation() {
    if (!project)
        return;

    if (project->isDecimatedFrame(current_frame))
        project->deleteDecimatedFrame(current_frame);
    else
        project->addDecimatedFrame(current_frame);
    commit("Toggle decimation");

    updateSectionOrphanFields(current_frame);

    if (preview) {
        try {
            evaluateFinalScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());

            togglePreview();
        }
    }

    // Handles updating current_frame and stuff so the right frame numbers will be displayed.
    jumpRelative(0);

    updateFrameDetails();

    updateFrameRatesViewer();
}


void WobblyWindow::toggleCombed() {
    if (!project)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }

    if (project->isCombedFrame(current_frame))
        for (int i = start; i <= end; i++)
            project->deleteCombedFrame(i);
    else
        for (int i = start; i <= end; i++)
            project->addCombedFrame(i);

    commit("Toggle combed");

    /// Uncomment if combed frames ever get filtered
//    if (preview) {
//        try {
//            evaluateFinalScript();
//        } catch (WobblyException &e) {
//            errorPopup(e.what());

//            togglePreview();
//        }
//    }

    updateFrameDetails();
}


void WobblyWindow::toggleBookmark() {
    if (!project)
        return;

    if (project->isBookmark(current_frame)) {
        project->deleteBookmark(current_frame);
    } else {
        bool ok = true;
        QString description;

        if (settings.value(KEY_ASK_FOR_BOOKMARK_DESCRIPTION).toBool())
            description = QInputDialog::getText(this, QStringLiteral("Bookmark description"), QStringLiteral("Optional description for the bookmark:"), QLineEdit::Normal, QString(), &ok);

        if (ok)
            project->addBookmark(current_frame, description.toStdString());
    }
    commit("Toggle bookmark");

    updateFrameDetails();
}


void WobblyWindow::addSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start != current_frame) {
        project->addSection(current_frame);
        commit("Add section");

        if (preview && section->presets.size()) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    }
}


void WobblyWindow::deleteSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start != 0) {
        bool update_needed = false;

        if (preview) {
            const Section *previous_section = project->findSection(section->start - 1);

            if (section->presets != previous_section->presets)
                update_needed = true;
        }

        project->deleteSection(section->start);
        commit("Delete section");

        if (update_needed) {
            try {
                evaluateFinalScript();
            } catch (WobblyException &e) {
                errorPopup(e.what());

                togglePreview();
            }
        }

        updateFrameDetails();
    }
}


void WobblyWindow::presetChanged(const QString &text) {
    if (!project)
        return;

    if (text.isEmpty())
        preset_edit->setPlainText(QString());
    else
        preset_edit->setPlainText(QString::fromStdString(project->getPresetContents(text.toStdString())));
}


void WobblyWindow::presetEdited() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    std::string name = preset_combo->currentText().toStdString();
    std::string contents = preset_edit->toPlainText().toStdString();
    if (contents != project->getPresetContents(name)) {
        project->setPresetContents(name, contents);
        commit("Edit preset");
    }
}


void WobblyWindow::resetMatch() {
    if (!project)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }


    project->resetRangeMatches(start, end);
    commit("Reset match(es)");

    updateSectionOrphanFields(current_frame);

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::resetSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);

    project->resetSectionMatches(section->start);
    commit("Reset section");

    updateSectionOrphanFields(section);

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::rotateAndSetPatterns() {
    if (!project)
        return;

    int size = match_pattern.size();
    match_pattern.prepend(match_pattern[size - 1]);
    match_pattern.truncate(size);
    match_pattern_edit->setText(match_pattern);

    size = decimation_pattern.size();
    decimation_pattern.prepend(decimation_pattern[size - 1]);
    decimation_pattern.truncate(size);
    decimation_pattern_edit->setText(decimation_pattern);

    const Section *section = project->findSection(current_frame);

    project->setSectionMatchesFromPattern(section->start, match_pattern.toStdString());
    project->setSectionDecimationFromPattern(section->start, decimation_pattern.toStdString());
    commit("Rotate section pattern");

    updateSectionOrphanFields(section);

    updateFrameRatesViewer();

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setMatchPattern() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeMatchesFromPattern(range_start, range_end, match_pattern.toStdString());
    commit("Set match pattern to range");

    cancelRange();

    updateSectionOrphanFields(current_frame);

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setDecimationPattern() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeDecimationFromPattern(range_start, range_end, decimation_pattern.toStdString());
    commit("Set decimation pattern to range");

    cancelRange();

    updateFrameRatesViewer();

    if (project)
        project->updateOrphanFields();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::setMatchAndDecimationPatterns() {
    if (!project)
        return;

    if (range_start == -1)
        return;

    finishRange();

    project->setRangeMatchesFromPattern(range_start, range_end, match_pattern.toStdString());
    project->setRangeDecimationFromPattern(range_start, range_end, decimation_pattern.toStdString());
    commit("Set match and decimation patterns to range");

    cancelRange();

    updateFrameRatesViewer();

    updateSectionOrphanFields(current_frame);

    updateCMatchSequencesWindow();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::updateSectionOrphanFields(int frame) {
    if (!project)
        return;

    const Section *section = project->findSection(frame);

    updateSectionOrphanFields(section);
}


void WobblyWindow::updateSectionOrphanFields(const Section *section) {
    if (!project)
        return;

    project->updateSectionOrphanFields(section->start, project->getSectionEnd(section->start));
}


void WobblyWindow::guessCurrentSectionPatternsFromMics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int section_start = project->findSection(current_frame)->start;

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    bool success;

    try {
        success = project->guessSectionPatternsFromMics(section_start, pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess section patterns from mics");
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());

        return;
    }

    updatePatternGuessingWindow();

    QApplication::restoreOverrideCursor();

    if (success) {
        updateFrameRatesViewer();

        updateSectionOrphanFields(current_frame);

        updateCMatchSequencesWindow();

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}

void WobblyWindow::guessCurrentSectionPatternsFromDMetrics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int section_start = project->findSection(current_frame)->start;

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    bool success;

    try {
        success = project->guessSectionPatternsFromDMetrics(section_start, pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess section patterns from dmetrics");
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());

        return;
    }

    updatePatternGuessingWindow();

    QApplication::restoreOverrideCursor();

    if (success) {
        updateFrameRatesViewer();

        updateSectionOrphanFields(current_frame);

        updateCMatchSequencesWindow();

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}

void WobblyWindow::guessCurrentSectionPatternsFromMicsAndDMetrics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int section_start = project->findSection(current_frame)->start;

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    bool success;

    try {
        success = project->guessSectionPatternsFromMicsAndDMetrics(section_start, pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess section patterns from mics and dmetrics");
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());

        return;
    }

    updatePatternGuessingWindow();

    QApplication::restoreOverrideCursor();

    if (success) {
        updateFrameRatesViewer();

        updateSectionOrphanFields(current_frame);

        updateCMatchSequencesWindow();

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}

void WobblyWindow::guessProjectPatternsFromMics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    try {
        project->clearOrphanFields();

        project->guessProjectPatternsFromMics(pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess project patterns from mics");

        QApplication::restoreOverrideCursor();

        updatePatternGuessingWindow();

        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        evaluateScript(preview);
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());
    }
}


void WobblyWindow::guessProjectPatternsFromDMetrics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    try {
        project->clearOrphanFields();

        project->guessProjectPatternsFromDMetrics(pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess section patterns from dmetrics");

        QApplication::restoreOverrideCursor();

        updatePatternGuessingWindow();

        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        evaluateScript(preview);
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());
    }
}


void WobblyWindow::guessProjectPatternsFromMicsAndDMetrics() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int use_patterns = 0;
    auto buttons = pg_use_patterns_buttons->buttons();
    for (int i = 0; i < buttons.size(); i++)
        if (buttons[i]->isChecked())
            use_patterns |= pg_use_patterns_buttons->id(buttons[i]);

    try {
        project->clearOrphanFields();

        project->guessProjectPatternsFromMicsAndDMetrics(pg_length_spin->value(), pg_edge_cutoff->value(), use_patterns, pg_decimate_buttons->checkedId());
        commit("Guess project patterns from mics and dmetrics");

        QApplication::restoreOverrideCursor();

        updatePatternGuessingWindow();

        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        evaluateScript(preview);
    } catch (WobblyException &e) {
        QApplication::restoreOverrideCursor();

        errorPopup(e.what());
    }
}


void WobblyWindow::guessCurrentSectionPatternsFromMatches() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int section_start = project->findSection(current_frame)->start;

    bool success = project->guessSectionPatternsFromMatches(section_start, pg_length_spin->value(), pg_edge_cutoff->value(), pg_n_match_buttons->checkedId(), pg_decimate_buttons->checkedId());
    commit("Guess section patterns from matches");

    updatePatternGuessingWindow();

    QApplication::restoreOverrideCursor();

    if (success) {
        updateFrameRatesViewer();

        updateCMatchSequencesWindow();

        try {
            evaluateScript(preview);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::guessProjectPatternsFromMatches() {
    if (!project)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    project->clearOrphanFields();

    project->guessProjectPatternsFromMatches(pg_length_spin->value(), pg_edge_cutoff->value(), pg_n_match_buttons->checkedId(), pg_decimate_buttons->checkedId());
    commit("Guess project patterns from matches");

    updatePatternGuessingWindow();

    updateFrameRatesViewer();

    updateCMatchSequencesWindow();

    QApplication::restoreOverrideCursor();

    try {
        evaluateScript(preview);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::togglePreview() {
    if (!project)
        return;

    preview = !preview;

    try {
        if (preview) {
            // Yucky.
            if (preset_edit->hasFocus())
                presetEdited();

            evaluateFinalScript();
        } else {
            evaluateMainDisplayScript();
        }
    } catch (WobblyException &e) {
        errorPopup(e.what());
        preview = !preview;
    }

    {
        QSignalBlocker block(tab_bar);
        tab_bar->setCurrentIndex((int)preview);
    }
}

void WobblyWindow::undo() {
    if (!project) return;
    project->undo();
    updateAfterUndo();
}

void WobblyWindow::redo() {
    if (!project) return;
    project->redo();
    updateAfterUndo();
}

void WobblyWindow::commit(std::string message) {
    if (!project) return;
    project->commit(message);

    updateUndoActions();
}

void WobblyWindow::updateUndoActions() {
    if (!project) {
        undo_action->setEnabled(false);
        redo_action->setEnabled(false);
    } else {
        std::string undo_text = project->getUndoDescription();
        std::string redo_text = project->getRedoDescription();

        if (undo_text.empty()) {
            undo_action->setEnabled(false);
            undo_action->setText("Undo");
        } else {
            undo_action->setEnabled(true);
            undo_action->setText(QStringLiteral("Undo %1").arg(QString::fromStdString(undo_text)));
        }

        if (redo_text.empty()) {
            redo_action->setEnabled(false);
            redo_action->setText("Redo");
        } else {
            redo_action->setEnabled(true);
            redo_action->setText(QStringLiteral("Redo %1").arg(QString::fromStdString(redo_text)));
        }
    }
}

void WobblyWindow::updateAfterUndo() {
    updateUndoActions();

    project->updateOrphanFields();
    updateFrameRatesViewer();
    updatePatternGuessingWindow();
    updateCMatchSequencesWindow();
    updateFadesWindow();
    presetChanged(preset_combo->currentText());

    evaluateMainDisplayScript();
}


void WobblyWindow::zoom(bool in) {
    if (!project)
        return;

    int zoom = project->getZoom();
    if ((!in && zoom > 1) || (in && zoom < 8)) {
        zoom += in ? 1 : -1;
        project->setZoom(zoom);
        try {
            requestFrames(current_frame);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }

    zoom_label->setText(QStringLiteral("Zoom: %1x").arg(zoom));
}


void WobblyWindow::zoomIn() {
    zoom(true);
}


void WobblyWindow::zoomOut() {
    zoom(false);
}


void WobblyWindow::startRange() {
    if (!project)
        return;

    range_start = current_frame;

    statusBar()->showMessage(QStringLiteral("Range start: %1").arg(range_start), 0);
}


void WobblyWindow::finishRange() {
    if (!project)
        return;

    range_end = current_frame;

    if (range_start > range_end)
        std::swap(range_start, range_end);
}


void WobblyWindow::cancelRange() {
    if (!project)
        return;

    range_start = range_end = -1;

    statusBar()->clearMessage();
}


int WobblyWindow::getSelectedPreset() const {
    return selected_preset;
}


void WobblyWindow::setSelectedPreset(int index) {
    PresetsModel *presets_model = project->getPresetsModel();

    if (index >= presets_model->rowCount())
        index = presets_model->rowCount() - 1;

    selected_preset = index;

    QString preset_name;
    if (selected_preset > -1)
        preset_name = presets_model->data(presets_model->index(selected_preset)).toString();

    selected_preset_label->setText("Selected preset: " + preset_name);
}


void WobblyWindow::selectPreviousPreset() {
    if (!project)
        return;

    int num_presets = project->getPresetsModel()->rowCount();

    int index = getSelectedPreset();

    if (num_presets == 0) {
        index = -1;
    } else if (num_presets == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = num_presets - 1;
        } else {
            if (index == 0)
                index = num_presets;
            index--;
        }
    }

    setSelectedPreset(index);
}


void WobblyWindow::selectNextPreset() {
    if (!project)
        return;

    int num_presets = project->getPresetsModel()->rowCount();

    int index = getSelectedPreset();

    if (num_presets == 0) {
        index = -1;
    } else if (num_presets == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = 0;
        } else {
            index = (index + 1) % num_presets;
        }
    }

    setSelectedPreset(index);
}


int WobblyWindow::getSelectedCustomList() const {
    return selected_custom_list;
}


void WobblyWindow::setSelectedCustomList(int index) {
    if (!project)
        return;

    const CustomListsModel *cl = project->getCustomListsModel();

    if (index >= (int)cl->size())
        index = cl->size() - 1;

    selected_custom_list = index;

    selected_custom_list_label->setText(QStringLiteral("Selected custom list: ") + (selected_custom_list > -1 ? cl->at(selected_custom_list).name.c_str() : ""));
}


void WobblyWindow::selectPreviousCustomList() {
    if (!project)
        return;

    const CustomListsModel *cl = project->getCustomListsModel();

    int index = getSelectedCustomList();

    if (cl->size() == 0) {
        index = -1;
    } else if (cl->size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = cl->size() - 1;
        } else {
            if (index == 0)
                index = cl->size();
            index--;
        }
    }

    setSelectedCustomList(index);
}


void WobblyWindow::selectNextCustomList() {
    if (!project)
        return;

    const CustomListsModel *cl = project->getCustomListsModel();

    int index = getSelectedCustomList();

    if (cl->size() == 0) {
        index = -1;
    } else if (cl->size() == 1) {
        index = 0;
    } else {
        if (index == -1) {
            index = 0;
        } else {
            index = (index + 1) % cl->size();
        }
    }

    setSelectedCustomList(index);
}


void WobblyWindow::assignSelectedPresetToCurrentSection() {
    if (!project)
        return;

    if (selected_preset == -1)
        return;

    PresetsModel *presets_model = project->getPresetsModel();

    int section_start = project->findSection(current_frame)->start;
    project->setSectionPreset(section_start, presets_model->data(presets_model->index(selected_preset)).toString().toStdString());
    commit("Assign preset");

    if (preview) {
        try {
            evaluateFinalScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());

            togglePreview();
        }
    }

    updateFrameDetails();
}


void WobblyWindow::addRangeToSelectedCustomList() {
    if (!project)
        return;

    if (selected_custom_list == -1)
        return;

    int start, end;

    if (range_start == -1) {
        start = current_frame;
        end = current_frame;
    } else {
        finishRange();

        start = range_start;
        end = range_end;

        cancelRange();
    }

    try {
        project->addCustomListRange(selected_custom_list, start, end);
        commit("Add range to custom list");

        updateFrameDetails();
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }

    if (preview) {
        try {
            evaluateFinalScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());

            togglePreview();
        }
    }
}


void WobblyWindow::copyCurrentFrameNumberToClipboard() {
    if(project != nullptr) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if(preview) {
            clipboard->setText(QString::number(project->frameNumberAfterDecimation(current_frame)));
        } else {
            clipboard->setText(QString::number(current_frame));
        }
    }
}


void WobblyWindow::copyCurrentFrameImageToClipboard() {
    if(project != nullptr) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setPixmap(frame_label->pixmap(Qt::ReturnByValue).scaled(original_frame_width, original_frame_height, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    }
}


QSize WobblyWindow::getThumbnailSize(QSize image_size) {
    QSize thumbnail_size;
    QRect desktop_rect = QApplication::desktop()->screenGeometry(this);
    double percentage = settings_thumbnail_size_dspin->value();

    if (desktop_rect.width() >= desktop_rect.height()) {
        thumbnail_size.setHeight((int)(desktop_rect.height() * percentage / 100));
        thumbnail_size.setWidth((int)(image_size.width() * thumbnail_size.height() / image_size.height()));
    } else {
        thumbnail_size.setHeight((int)(desktop_rect.width() * percentage / 100));
        thumbnail_size.setWidth((int)(image_size.width() * thumbnail_size.height() / image_size.height()));
    }

    return thumbnail_size;
}


QPixmap WobblyWindow::getThumbnail(const QImage &image) {
    QSize thumbnail_size = getThumbnailSize(image.size());

    return QPixmap::fromImage(image.scaled(thumbnail_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
