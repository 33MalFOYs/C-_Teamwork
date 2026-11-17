#include "main_window.hpp"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("Personal Expense Tracker");
    QApplication::setApplicationVersion("0.4.0");
    QCommandLineParser arguments;
    arguments.addHelpOption(); arguments.addVersionOption();
    QCommandLineOption config({"c", "config"}, "Local database configuration file.", "path", "config.local");
    arguments.addOption(config); arguments.process(application);
    expense_tracker::gui::MainWindow window(std::filesystem::u8path(arguments.value(config).toStdString()));
    window.show();
    return application.exec();
}

